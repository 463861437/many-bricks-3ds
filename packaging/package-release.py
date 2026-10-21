#!/usr/bin/env python3
"""Package verified binaries and a clean source snapshot in dist/.

Run verify-cia.py --output packaging/VERIFIED-BUILD.json after building.
Neither archive includes local build directories or ELF/debug symbols.
"""
import hashlib
import json
import os
from pathlib import Path
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "dist"
EXCLUDED = {".git", "build", "build-host", "build-cia", "dist", ".venv", "venv",
            "__pycache__", ".DS_Store", "FTP-TRANSFER.json", ".pytest_cache"}
EXCLUDED_SUFFIXES = {".elf", ".map", ".o", ".obj", ".a", ".d", ".pyc", ".pyo",
                     ".zip", ".cia", ".3dsx", ".smdh", ".tmp", ".log"}
SOURCE_DIRECTORIES = {"source", "include", "tests", "packaging", "docs", ".github"}
SOURCE_ROOT_NAMES = {"Makefile", "LICENSE", "COPYING", ".gitignore", ".gitattributes"}
SOURCE_ROOT_SUFFIXES = {".md", ".txt", ".sh", ".py", ".png"}
BINARIES = {
    "many-bricks-3ds.cia": "cias/many-bricks-3ds.cia",
    "many-bricks-3ds.3dsx": "3ds/many-bricks-3ds/many-bricks-3ds.3dsx",
    "many-bricks-3ds.smdh": "3ds/many-bricks-3ds/many-bricks-3ds.smdh",
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_files():
    """Use a whitelist so an initial checkout needs no Git metadata."""
    selected = []
    for path in ROOT.rglob("*"):
        rel = path.relative_to(ROOT)
        if not path.is_file() or path.is_symlink():
            continue
        if EXCLUDED.intersection(rel.parts) or path.suffix.lower() in EXCLUDED_SUFFIXES:
            continue
        if any((ROOT / Path(*rel.parts[:i])).is_symlink() for i in range(1, len(rel.parts))):
            continue
        if len(rel.parts) > 1:
            allowed = rel.parts[0] in SOURCE_DIRECTORIES
        else:
            allowed = path.name in SOURCE_ROOT_NAMES or path.suffix.lower() in SOURCE_ROOT_SUFFIXES
        if allowed:
            selected.append(path)
    return sorted(selected)


def write_archive(destination, entries):
    """Write and CRC-check a temporary archive before publishing it."""
    with tempfile.NamedTemporaryFile(dir=OUTPUT, suffix=".tmp", delete=False) as stream:
        temporary = Path(stream.name)
    try:
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name, path in sorted(entries.items()):
                archive.write(path, name)
            sums = "".join(digest(path) + "  " + name + "\n"
                           for name, path in sorted(entries.items()))
            archive.writestr("SHA256SUMS", sums)
        with zipfile.ZipFile(temporary) as archive:
            if archive.testzip() is not None:
                raise RuntimeError("Archive CRC check failed: " + destination.name)
        os.replace(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    report = json.loads((ROOT / "packaging/VERIFIED-BUILD.json").read_text(encoding="utf-8"))
    if report.get("title_id") != "000400000F7A3300" or report.get("version") != "1.1.1":
        raise SystemExit("Unexpected title/version; regenerate the build verification report")
    for name in BINARIES:
        if name not in report["artifacts"]:
            raise SystemExit("Re-run verify-cia.py to verify all release files: " + name)
    for name, record in report["artifacts"].items():
        if Path(name).name != name or name in {".", ".."}:
            raise SystemExit("Invalid artifact name in build report")
        path = ROOT / name
        if path.stat().st_size != record["bytes"] or digest(path) != record["sha256"]:
            raise SystemExit("Re-run verify-cia.py before packaging: " + name)

    OUTPUT.mkdir(exist_ok=True)
    files = source_files()
    source_entries = {"many-bricks-3ds/" + path.relative_to(ROOT).as_posix(): path for path in files}
    installer_entries = {target: ROOT / name for name, target in BINARIES.items()}
    for path in files:
        rel = path.relative_to(ROOT)
        # Installation instructions, full docs, licenses, the verification
        # report and screenshots accompany the SD-card layout.
        if ((len(rel.parts) == 1 and (path.suffix.lower() in {".md", ".txt", ".png"}
                                     or path.name in {"LICENSE", "COPYING"}))
                or rel.parts[0] == "docs"
                or rel.as_posix() in {"packaging/FONT-LICENSE.txt", "packaging/VERIFIED-BUILD.json"}
                or rel.parts[:2] == ("packaging", "licenses")):
            installer_entries[rel.as_posix()] = path

    archives = [OUTPUT / "many-bricks-3ds-project.zip", OUTPUT / "many-bricks-3ds-install.zip"]
    for destination, entries in zip(archives, [source_entries, installer_entries]):
        write_archive(destination, entries)
        print(destination.relative_to(ROOT), destination.stat().st_size, "bytes")
    checksums = OUTPUT / "SHA256SUMS"
    checksums.write_text("".join(digest(path) + "  " + path.name + "\n" for path in archives),
                         encoding="utf-8")
    print(checksums.relative_to(ROOT))


if __name__ == "__main__":
    main()
