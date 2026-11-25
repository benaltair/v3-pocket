"""A single poll covers multiple releases, including an annotated tag and a backport."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".github"))
from select_release import pending, resolve_tags


def release(version, day=20, draft=False, prerelease=False):
    return dict(tag_name="repeater-v" + version, published_at=f"2026-11-{day:02}T00:00:00Z",
                draft=draft, prerelease=prerelease)


releases = [release("1.18.0"), release("1.17.0", 1), release("1.17.2"),
            release("1.19.0"), release("1.20.0"), release("2.0.0", draft=True),
            release("2.1.0", prerelease=True), release("2.0.0-rc1"), release("1.19.1")]
releases.append({**release("9.0.0"), "tag_name": "companion-v9.0.0"})
tags = pending(releases, "repeater-v1.18.0", "2026-11-15T00:00:00Z", {"repeater-v1.19.1"})
assert tags == ["repeater-v1.17.2", "repeater-v1.19.0", "repeater-v1.20.0"]
assert pending(releases, "repeater-v1.18.0", "2026-11-15T00:00:00Z", set(tags) | {"repeater-v1.19.1"}) == []
refs = "\n".join(["a"*40+" refs/tags/"+tags[0], "b"*40+" refs/tags/"+tags[1],
                  "c"*40+" refs/tags/"+tags[1]+"^{}", "d"*40+" refs/tags/"+tags[2]])
assert [r["commit"] for r in resolve_tags(refs, tags)] == [n*40 for n in "acd"]
print("Verified multi-release detection, stable backports, prior reviews and annotated tag resolution.")
