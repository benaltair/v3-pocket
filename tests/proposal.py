import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

kit=Path(__file__).resolve().parents[1]
def run(*argv, cwd, env=None):
    return subprocess.run(argv, cwd=cwd, env=env, check=True, capture_output=True, text=True).stdout

with tempfile.TemporaryDirectory(prefix='v3-pr-test-') as tmp:
    root=Path(tmp)
    repo=root/'checkout'
    remote=root/'origin.git'
    run('git','init','-q','--bare',str(remote),cwd=root)
    run('git','init','-q','-b','main',str(repo),cwd=root)
    shutil.copy(kit/'upstream.json',repo)
    run('git','add','upstream.json',cwd=repo)
    run('git','-c','user.name=Test','-c','user.email=test@example.invalid','commit','-qm','fixture',cwd=repo)
    run('git','remote','add','origin',str(remote),cwd=repo)
    run('git','push','-q','origin','main',cwd=repo)
    fake=root/'gh'
    fake.write_text("""#!/bin/sh
set -eu
case "$1:$2" in
 api:-X) if test -f "$RUNNER_TEMP/created"; then echo 1; else echo 0; fi ;;
 pr:create) echo "$@" > "$RUNNER_TEMP/created";;
 *) exit 1;;
esac
""")
    fake.chmod(0o755)
    cfg=dict(os.environ,PATH=str(root)+os.pathsep+os.environ['PATH'],
             RUNNER_TEMP=str(root),TAG='repeater-v1.18.99',COMMIT='a'*40,RESULT='failure',
             GITHUB_SERVER_URL='https://example.invalid',GITHUB_REPOSITORY='test/pocket',
             GITHUB_RUN_ID='123',BASE='main',REVIEWER='maintainer')
    run('bash',kit/'.github/propose.sh',cwd=repo,env=cfg)
    branch='refs/heads/updates/'+cfg['TAG']
    assert json.loads(run('git','show',branch+':upstream.json',cwd=remote))=={'tag':cfg['TAG'],'commit':cfg['COMMIT']}
    assert ' --draft ' in (root/'created').read_text() and '**failure**' in (root/'update-body.md').read_text()
    assert '--reviewer maintainer' in (root/'created').read_text()
    assert json.loads(run('git','show','main:upstream.json',cwd=remote))['tag']!='repeater-v1.18.99'
    assert 'already exists' in run('bash',kit/'.github/propose.sh',cwd=repo,env=cfg)
    print('Verified draft proposal creation and duplicate suppression against a local test repository.')
