#!/usr/bin/env python3
"""Fetch fixed, verified build inputs. Never downloads an executable script."""
import hashlib
import json
from pathlib import Path
import shutil
import stat
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
LOCK = json.loads((ROOT / "dependencies.json").read_text(encoding="utf-8"))


def fetch(url, destination, expected=None):
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        request = urllib.request.Request(url, headers={"User-Agent": "Xiaomaodou-build"})
        with urllib.request.urlopen(request, timeout=120) as source:
            content = source.read()
        if expected and hashlib.sha256(content).hexdigest() != expected:
            raise RuntimeError(f"Digest mismatch: {destination.name}")
        destination.write_bytes(content)
    content = destination.read_bytes()
    if expected and hashlib.sha256(content).hexdigest() != expected:
        raise RuntimeError(f"Cached digest mismatch: {destination}")
    return content


def extract(archive, destination):
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as bundle:
        for item in bundle.infolist():
            relative = Path(item.filename)
            target = (destination / relative).resolve()
            if relative.is_absolute() or not target.is_relative_to(destination):
                raise RuntimeError("Unsafe archive entry")
            if item.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            content = bundle.read(item)
            if stat.S_ISLNK(item.external_attr >> 16) and sys.platform != "win32":
                link = content.decode("utf-8")
                if not (target.parent / link).resolve().is_relative_to(destination):
                    raise RuntimeError("Unsafe archive link")
                if not target.exists() and not target.is_symlink():
                    target.symlink_to(link)
            else:
                target.write_bytes(content)


def main():
    cache = ROOT / ".build" / "downloads"
    resources = ROOT / "Resources"
    notices = resources / "Notices"
    source = ROOT / ".build" / "RimeSource"
    for directory in (cache, notices, source):
        directory.mkdir(parents=True, exist_ok=True)
    rime = LOCK["librime"]
    release = f"https://github.com/{rime['repository']}/releases/download/{rime['tag']}/"
    framework_zip = cache / rime["asset"]
    fetch(release + rime["asset"], framework_zip, rime["sha256"])
    extract(framework_zip, ROOT / "Vendor")
    framework = ROOT / "Vendor" / "librime-static.xcframework"
    info = __import__("plistlib").loads((framework / "Info.plist").read_bytes())
    ios = next(item for item in info["AvailableLibraries"]
               if item["LibraryIdentifier"] == "ios-arm64")
    header_root = framework / ios["LibraryIdentifier"] / ios.get("HeadersPath", "Headers")
    shutil.copytree(header_root, ROOT / "Vendor" / "RimeHeaders", dirs_exist_ok=True)
    notice_zip = cache / "third-party-notices.zip"
    fetch(release + notice_zip.name, notice_zip, rime["notices_sha256"])
    extract(notice_zip, notices / "NativeDependencies")
    for filename in ("LICENSE.txt", "THIRD_PARTY_NOTICES.md", "build-metadata.json"):
        fetch(release + filename, notices / ("librime-" + filename))
    for name in ("luna_pinyin", "essay"):
        item = LOCK[name]
        raw = f"https://raw.githubusercontent.com/{item['repository']}/{item['commit']}/"
        fetch(raw + item["file"], source / item["file"], item["sha256"])
        fetch(raw + "LICENSE", notices / (name + "-LICENSE"))
    package = f"https://raw.githubusercontent.com/{rime['repository']}/{rime['commit']}/"
    manifest = json.loads(fetch(package + "Sources/RimeOpenCCData/manifest.json",
                                cache / "opencc-manifest.json",
                                LOCK["opencc_manifest_sha256"]))
    for filename in ("t2s.json", "TSCharacters.ocd2", "TSPhrases.ocd2", "copyright"):
        fetch(package + "Sources/RimeOpenCCData/opencc/" + filename,
              source / "opencc" / filename, manifest["files"][filename])
    configs = ("default.yaml", "leaves_stone.schema.yaml", "leaves_stone.dict.yaml",
               "leaves_stone_english.schema.yaml", "leaves_stone_english.dict.yaml")
    for filename in configs:
        shutil.copy2(REPO / "output" / "data" / filename, source / filename)
    # Runtime has no need for raw dictionary text; it receives only configs,
    # OpenCC data and the separately generated prebuilt directory.
    runtime = resources / "Rime"
    runtime.mkdir(exist_ok=True)
    for filename in ("default.yaml", "leaves_stone.schema.yaml",
                     "leaves_stone_english.schema.yaml"):
        shutil.copy2(source / filename, runtime / filename)
    shutil.copytree(source / "opencc", runtime / "opencc", dirs_exist_ok=True)
    for state in ("awake", "sleep"):
        shutil.copy2(REPO / "resource" / "doggy" / "rest" / f"mascot-{state}.png",
                     resources / f"mascot-{state}.png")
    shutil.copy2(REPO / "resource" / "doggy" / "textures" / "panel-plush.png",
                 resources / "panel-plush.png")
    shutil.copy2(REPO / "LICENSE.txt", notices / "Xiaomaodou-GPL-3.0.txt")
    shutil.copy2(REPO / "output" / "data" / "LEAVES_STONE_DATA_LICENSES.md",
                 notices / "Pinyin-data-attribution.md")
    (notices / "build-inputs.json").write_text(json.dumps(LOCK, indent=2) + "\n")
    print("Pinned Rime framework, full dictionary, conversion data and notices: ready")


if __name__ == "__main__":
    main()
