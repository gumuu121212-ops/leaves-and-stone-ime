#!/usr/bin/env python3
"""Validate and package an unsigned arm64 app. This does not sign or install it."""
import hashlib
import json
from pathlib import Path
import plistlib
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def package(app, destination):
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(app.rglob("*")):
            if path.is_file():
                archive.write(path, "Payload/" + str(path.relative_to(app.parent)))


def main():
    app = ROOT / ".build/Xiaomaodou.xcarchive/Products/Applications/Xiaomaodou.app"
    extension = app / "PlugIns/XiaomaodouKeyboard.appex"
    info = plistlib.loads((extension / "Info.plist").read_bytes())
    attrs = info["NSExtension"]["NSExtensionAttributes"]
    assert attrs["RequestsOpenAccess"] is False
    assert info["NSExtension"]["NSExtensionPointIdentifier"] == "com.apple.keyboard-service"
    assert (extension / "Rime/build/leaves_stone.table.bin").stat().st_size > 1_000_000
    assert (extension / "Notices/Xiaomaodou-GPL-3.0.txt").exists()
    for bundle in (app, extension):
        binary = bundle / plistlib.loads((bundle / "Info.plist").read_bytes())["CFBundleExecutable"]
        arch = subprocess.check_output(["lipo", "-archs", str(binary)], text=True).strip()
        assert arch == "arm64", (binary, arch)
    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    ipa = dist / "Xiaomaodou-iPad-UNSIGNED.ipa"
    package(app, ipa)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    source = dist / "Xiaomaodou-iPad-source.zip"
    repo = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], cwd=ROOT, text=True).strip())
    subprocess.run(["git", "archive", "--format=zip", "--output=" + str(source), "HEAD"], cwd=repo, check=True)
    # Include the exact editable dictionary sources used to compile the binary
    # tables and their full notices, alongside the complete project sources.
    with zipfile.ZipFile(source, "a", zipfile.ZIP_DEFLATED) as archive:
        for directory in (ROOT / ".build/RimeSource", ROOT / "Resources/Notices"):
            for path in sorted(directory.rglob("*")):
                if path.is_file():
                    archive.write(path, "platforms/ipados/" + str(path.relative_to(ROOT)))
    manifest = dict(source_commit=commit, product="小毛豆 iPad touch keyboard", signed=False,
                    minimum_ipados="15.0", architecture="arm64", full_access_required=False,
                    device_install_verified=False, ipa_sha256=hashlib.sha256(ipa.read_bytes()).hexdigest(),
                    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
    (dist / "build-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (dist / "README.md").write_bytes((ROOT / "README.md").read_bytes())
    print("Validated unsigned device app and keyboard extension: " + ipa.name)


if __name__ == "__main__":
    main()
