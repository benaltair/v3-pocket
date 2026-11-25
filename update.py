"""Build V3 Pocket against a verified MeshCore repeater release."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
UPSTREAM = "https://github.com/meshcore-dev/MeshCore.git"
TARGET = "Heltec_v3_repeater_pocket"
UI_REVISION = 5
PIN = json.loads((ROOT / "upstream.json").read_text())


def host_tests(repo, temp):
    tests = ROOT / "tests"
    screen = temp / "screen-test"
    contrast = temp / "contrast-test"
    subprocess.run(["c++", "-std=c++11", "-DV3_POCKET_REPEATER", "-I", tests,
                    "-I", "src", "-I", "examples/simple_repeater", "-I", "variants/heltec_v3",
                    tests / "test.cpp", "src/helpers/ui/MomentaryButton.cpp", "-o", screen], cwd=repo, check=True)
    subprocess.run([screen], stdout=subprocess.DEVNULL, check=True)
    subprocess.run(["c++", "-std=c++11", "-DPOCKET_OLED_CONTRAST=0x58", "-DPIN_OLED_RESET=-1",
                    "-I", tests / "contrast", "-I", "src", tests / "contrast/contrast.cpp",
                    "src/helpers/ui/SSD1306Display.cpp", "-o", contrast], cwd=repo, check=True)
    subprocess.run([contrast], check=True)
    return screen


def verify_application(data, partitions, version):
    # ESP-IDF application descriptor follows the first segment header. Merged
    # flash dumps and bootloader images do not carry it at this offset.
    if (len(data) < 72 or data[0] != 0xe9 or not 1 <= data[1] <= 16
            or data[3] >> 4 != 3 or struct.unpack_from("<H", data, 12)[0] != 9
            or data[32:36] != bytes.fromhex("3254cdab") or data[23] != 1):
        raise ValueError("Expected an 8 MB ESP32-S3 application with an appended digest")
    if hashlib.sha256(data[:-32]).digest() != data[-32:]:
        raise ValueError("Invalid ESP image SHA-256")
    if version.encode() + b"\0" not in data:
        raise ValueError("Missing Pocket firmware version")
    slots = []
    for magic, kind, subtype, start, size, _, flags in struct.iter_unpack("<HBBII16sI", partitions):
        if magic != 0x50aa:
            break
        if kind == 0:
            slots.append((subtype, start, size, flags))
    if slots != [(0x10, 0x10000, 0x330000, 0), (0x11, 0x340000, 0x330000, 0)]:
        raise ValueError("App partition layout changed and needs review")
    if len(data) > min(slot[2] for slot in slots):
        raise ValueError("Application exceeds the OTA slot")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", default=PIN["tag"])
    parser.add_argument("--expect-commit")
    parser.add_argument("--source", default=UPSTREAM, help="Official repository or local release mirror")
    parser.add_argument("--check-only", action="store_true", help="Apply the overlay and run host tests")
    parser.add_argument("--preview", action="store_true", help="Regenerate README images (requires Pillow and a full build)")
    parser.add_argument("--out", type=Path, default=Path("builds"))
    args = parser.parse_args()
    if not re.fullmatch(r"repeater-v\d+\.\d+\.\d+", args.release):
        parser.error("Expected a stable repeater-vX.Y.Z release")
    if args.preview and args.check_only:
        parser.error("--preview needs the display font from a full build")
    version = args.release.removeprefix("repeater-") + f"-pocket{UI_REVISION}"
    source = str(Path(args.source).resolve()) if Path(args.source).is_dir() else args.source
    with tempfile.TemporaryDirectory(prefix="v3-pocket-") as temp_name:
        temp = Path(temp_name)
        repo = temp / "MeshCore"
        subprocess.run(["git", "init", "-q", repo], check=True)
        def git(*args):
            return subprocess.check_output(["git", *args], text=True, cwd=repo).strip()
        git("fetch", "--depth=1", source, "refs/tags/" + args.release)
        git("checkout", "-q", "--detach", "FETCH_HEAD")
        commit = git("rev-parse", "HEAD")
        if ((args.release == PIN["tag"] and commit != PIN["commit"])
                or (args.expect_commit and commit != args.expect_commit)):
            raise ValueError("Upstream commit does not match the release pin")
        git("apply", ROOT / "patches/repeater.patch")
        for header in sorted((ROOT / "src").glob("*.h")):
            dest = repo / "variants/heltec_v3" / header.name
            if dest.exists():
                raise FileExistsError(dest)
            shutil.copyfile(header, dest)
        screen = host_tests(repo, temp)
        print(f"Host tests passed for {args.release} at {commit}.", flush=True)
        if args.check_only:
            return
        env = dict(os.environ, V3_POCKET_VERSION=version)
        subprocess.run(["pio", "run", "-e", TARGET], cwd=repo, env=env, check=True)
        product = repo / ".pio/build" / TARGET
        data = (product / "firmware.bin").read_bytes()
        table = (product / "partitions.bin").read_bytes()
        verify_application(data, table, version)
        if args.preview:
            deps = Path(env.get("PLATFORMIO_LIBDEPS_DIR", repo / ".pio/libdeps"))
            from render import render
            render(screen, deps / TARGET / "Adafruit GFX Library", ROOT / "docs")
        image = f"heltec-v3-repeater-{version}-app.bin"
        manifest = dict(board="Heltec WiFi LoRa 32 V3", target=TARGET, release=args.release,
                        commit=commit, version=version, application=image, bytes=len(data),
                        sha256=hashlib.sha256(data).hexdigest(), partition_table_sha256=hashlib.sha256(table).hexdigest())
        args.out.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=args.out) as staging:
            folder = Path(staging) / version
            folder.mkdir()
            (folder / image).write_bytes(data)
            (folder / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
            folder.rename(args.out / version)
        print(f"Verified {args.out / version / image}", flush=True)


if __name__ == "__main__":
    main()
