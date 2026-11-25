"""Plan a pinned build or one independent review per new stable repeater release."""
import json
import os
from pathlib import Path
import re
import subprocess

UPSTREAM = "meshcore-dev/MeshCore"
TAG = re.compile(r"repeater-v(\d+)\.(\d+)\.(\d+)")
SHA = re.compile(r"[0-9a-f]{40}")


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def version(tag):
    return tuple(map(int, TAG.fullmatch(tag).groups()))


def pending(releases, pin, created, reviewed):
    # Catch up a new fork from its pin. Also notice stable backports published
    # after the fork was created, even if a higher version has since merged.
    return sorted({
        r["tag_name"] for r in releases
        if not r["draft"] and not r["prerelease"] and TAG.fullmatch(r["tag_name"])
        and r["tag_name"] != pin and r["tag_name"] not in reviewed
        and (r["published_at"] >= created or version(r["tag_name"]) > version(pin))
    }, key=version)


def resolve_tags(refs, tags):
    refs = dict(line.split()[::-1] for line in refs.splitlines())
    result = []
    for tag in tags:
        ref = "refs/tags/" + tag
        commit = refs.get(ref + "^{}") or refs.get(ref)
        if not commit or not SHA.fullmatch(commit):
            raise ValueError(f"Could not resolve upstream tag {tag} to a commit")
        result.append({"tag": tag, "commit": commit})
    return result


def api_pages(endpoint):
    pages = json.loads(output("gh", "api", "--paginate", "--slurp", endpoint))
    return [item for page in pages for item in page]


def main():
    pin = json.loads(Path("upstream.json").read_text())
    if not TAG.fullmatch(pin["tag"]) or not SHA.fullmatch(pin["commit"]):
        raise ValueError("Invalid pinned repeater release")
    event = os.environ["GITHUB_EVENT_NAME"]
    updates = []
    if event in ("schedule", "workflow_dispatch") and os.environ["GITHUB_REF_NAME"] == os.environ["BASE"]:
        repo = os.environ["GITHUB_REPOSITORY"]
        created = output("gh", "api", "repos/" + repo, "--jq", ".created_at")
        releases = api_pages(f"repos/{UPSTREAM}/releases?per_page=100")
        prs = api_pages(f"repos/{repo}/pulls?state=all&per_page=100")
        reviewed = {pr["head"]["ref"].removeprefix("updates/") for pr in prs
                    if pr["head"]["ref"].startswith("updates/")
                    and (pr["head"]["repo"] or {}).get("full_name") == repo
                    and pr["base"]["ref"] == os.environ["BASE"]}
        tags = pending(releases, pin["tag"], created, reviewed)
        if tags:
            refs = output("git", "ls-remote", "--tags", f"https://github.com/{UPSTREAM}.git", "refs/tags/repeater-v*")
            updates = resolve_tags(refs, tags)
    pinned = event in ("push", "pull_request") or (event == "workflow_dispatch" and not updates)
    with open(os.environ["GITHUB_OUTPUT"], "a") as out:
        print("tag=" + pin["tag"], file=out)
        print("commit=" + pin["commit"], file=out)
        print("pinned=" + str(pinned).lower(), file=out)
        print("updates=" + json.dumps(updates, separators=(",", ":")), file=out)
    print(f"Pinned {pin['tag']}. New releases to review: " + (", ".join(r["tag"] for r in updates) or "none"))


if __name__ == "__main__":
    main()
