"""Exercise publication without GitHub credentials or a device."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / ".github"))
import publish

pin = json.loads((root / "upstream.json").read_text())
with tempfile.TemporaryDirectory() as tmp:
    folder = Path(tmp)
    data = b"application fixture"
    sha = hashlib.sha256(data).hexdigest()
    version = pin["tag"].removeprefix("repeater-") + "-pocket5"
    assets = [f"heltec-v3-repeater-{version}-app.bin"]
    m = dict(version=version, release=pin["tag"], commit=pin["commit"], sha256=sha,
             application=assets[0], target="Heltec_v3_repeater_pocket")
    for name in assets:
        (folder / name).write_bytes(data)
    manifest = folder / "manifest.json"
    manifest.write_text(json.dumps(m))
    files = assets + ["manifest.json"]
    remote = [{"name": n, "state": "uploaded", "digest": "sha256:" + hashlib.sha256((folder/n).read_bytes()).hexdigest()}
              for n in files]
    draft = dict(isDraft=True, isPrerelease=False, assets=remote, body=publish.MARKER, targetCommitish="abc")
    for previous, expected in [(None, ["create", "upload", "edit"]), (draft, ["upload", "edit"])]:
        with patch.object(publish, "release_info", side_effect=[previous, draft]), patch.object(publish, "output", return_value="abc"), patch("subprocess.run") as run:
            publish.publish(folder, "owner/repo", "abc")
            assert [call.args[0][2] for call in run.call_args_list] == expected
    published = dict(draft, isDraft=False, url="https://example.invalid/release")
    with patch.object(publish, "release_info", return_value=published), patch("subprocess.run") as run:
        publish.publish(folder, "owner/repo", "abc")
        assert len(run.call_args_list) == 1 and run.call_args.args[0][:2] == ["git", "diff"]
    (folder/assets[0]).write_bytes(b"corrupt")
    try:
        publish.checked_files(folder)
    except ValueError:
        pass
    else:
        raise AssertionError("Incorrect checksum was accepted")
print("Verified release checks, draft recovery and existing-release handling.")
