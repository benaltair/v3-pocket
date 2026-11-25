"""Publish the public firmware from the successful main-branch build."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

PROJECT = "V3 Pocket"
INPUTS = ['upstream.json', 'src', 'patches', 'update.py']
MARKER = "<!-- pocket-firmware-release -->"


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def release_info(repo, version):
    cmd = ["gh", "release", "view", version, "-R", repo, "--json",
           "isDraft,isPrerelease,body,targetCommitish,assets,url"]
    result = subprocess.run(cmd, text=True, capture_output=True)
    if result.returncode == 0:
        return json.loads(result.stdout)
    if "release not found" in result.stderr.lower() or "HTTP 404" in result.stderr:
        return None
    raise RuntimeError(result.stderr.strip())


def checked_files(directory):
    manifests = list(directory.rglob("manifest.json"))
    if len(manifests) != 1:
        raise ValueError("Expected one build manifest")
    path = manifests[0]
    m = json.loads(path.read_text())
    pin = json.loads(Path("upstream.json").read_text())
    version = m["version"]
    if (m["release"] != pin["tag"] or m["commit"] != pin["commit"]
            or m["target"] != "Heltec_v3_repeater_pocket"
            or not re.fullmatch(re.escape(pin["tag"].removeprefix("repeater-")) + r"-pocket[0-9]+", version)):
        raise ValueError("Unexpected repeater firmware or upstream pin")
    name = f"heltec-v3-repeater-{version}-app.bin"
    if m["application"] != name:
        raise ValueError("Unexpected application name")
    hashes = {name: m["sha256"]}
    if {f.name for f in path.parent.iterdir()} != set(hashes) | {"manifest.json"}:
        raise ValueError("Unexpected files in the firmware bundle")
    for name, digest in hashes.items():
        if hashlib.sha256((path.parent / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f"Incorrect checksum for {name}")
    return version, pin["tag"], [path] + [path.parent / name for name in hashes]


def publish(directory, repo, commit):
    version, upstream, files = checked_files(directory)
    release = release_info(repo, version)
    if release and not release["isDraft"]:
        # Documentation and CI changes can rebuild non-reproducible binaries.
        # Keep the original tested assets when the firmware inputs are identical.
        subprocess.run(["git", "diff", "--exit-code", "--quiet", f"refs/tags/{version}", commit,
                        "--", *INPUTS], check=True)
        uploaded = {a["name"] for a in release["assets"] if a["state"] == "uploaded"}
        if release["isPrerelease"] or uploaded != {f.name for f in files}:
            raise ValueError("Published release assets need maintainer review")
        print(f"Already published: {release['url']}")
        return
    if release and (MARKER not in release["body"] or release["targetCommitish"] != commit):
        raise ValueError("An existing draft for this version needs maintainer review")
    target = output("gh", "api", f"repos/{repo}/git/ref/heads/main", "--jq", ".object.sha")
    if target != commit:
        print("A newer main commit will publish its build.")
        return
    if not release:
        notes = (f"Public-default **{PROJECT}** firmware, based on MeshCore `{upstream}`.\n\n"
                 "Use the `-app.bin` for the firmware update on a Heltec WiFi LoRa 32 V3 repeater with the compatible 8 MB dual-OTA layout.\n\n"
                 f"The checksums are in `manifest.json`. [Device quick start]"
                 f"(https://github.com/{repo}/blob/{commit}/README.md#quick-start)\n\n{MARKER}\n")
        with tempfile.TemporaryDirectory() as tmp:
            body = Path(tmp) / "notes.md"
            body.write_text(notes)
            subprocess.run(["gh", "release", "create", version, "-R", repo, "--target", commit,
                            "--draft", "--title", f"{PROJECT} {version}", "--notes-file", body], check=True)
    # A failed upload can leave a draft. Only a draft owned by this run's
    # source revision can be resumed. Public assets are never overwritten.
    subprocess.run(["gh", "release", "upload", version, "-R", repo, "--clobber", *files], check=True)
    draft = release_info(repo, version)
    expected = {f.name: "sha256:" + hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    uploaded = {a["name"]: a["digest"] for a in draft["assets"] if a["state"] == "uploaded"}
    if not draft["isDraft"] or expected != uploaded:
        raise ValueError("GitHub asset verification failed before publication")
    subprocess.run(["gh", "release", "edit", version, "-R", repo, "--draft=false", "--latest"], check=True)


if __name__ == "__main__":
    publish(Path(sys.argv[1]), os.environ["GITHUB_REPOSITORY"], os.environ["GITHUB_SHA"])
