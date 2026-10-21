#!/usr/bin/env python3
"""Verify the packaged program and resources; this is not a console runtime test."""
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

ROOT = Path(__file__).resolve().parent.parent


def tool(name, variable, candidates=()):
    override = os.environ.get(variable)
    if override:
        resolved = shutil.which(override)
        if resolved:
            return resolved
        raise RuntimeError("Cannot execute " + variable + "=" + override)
    for candidate in [*candidates, name]:
        resolved = shutil.which(str(candidate))
        if resolved:
            return resolved
    raise RuntimeError("Missing tool: " + name)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        help="Atomically write the report to this path instead of stdout")
    args = parser.parse_args()
    ctrtool = tool("ctrtool", "CTRTOOL")
    dkp = Path(os.environ.get("DEVKITPRO") or "/opt/devkitpro")
    arm = Path(os.environ.get("DEVKITARM") or dkp / "devkitARM")
    objdump = tool("arm-none-eabi-objdump", "OBJDUMP", [arm / "bin/arm-none-eabi-objdump"])
    cia = ROOT / "many-bricks-3ds.cia"
    elf = ROOT / "many-bricks-3ds.elf"
    with tempfile.TemporaryDirectory(prefix="many-bricks-cia-check-") as temp:
        base = Path(temp)
        result = subprocess.run([ctrtool, "-y", "--decompresscode",
                                 "--exefsdir=" + str(base / "exefs"),
                                 "--romfsdir=" + str(base / "romfs"), str(cia)],
                                capture_output=True, text=True, check=True)
        log = result.stdout
        for label in ["Logo hash", "Exheader hash", "ExeFS hash"]:
            require(re.search(re.escape(label) + r":\s*\(GOOD\)", log), label)
        require(len(re.findall(r"Section hash:\s*\(GOOD\)", log)) == 3, "ExeFS section hashes")
        require(re.search(r"\\- Hash:\s*\(GOOD\)", log), "CIA content hash")
        require(log.count("(FAIL)") == 3, "Unexpected verification failures")
        expected_warnings = [
            "[ctrtool::CiaProcess ERROR] Signature for Ticket was invalid.",
            "[ctrtool::CiaProcess ERROR] Signature for TitleMetaData was invalid.",
            "[ctrtool::ExHeaderProcess ERROR] Signature for AccessDescriptor was invalid.",
        ]
        require(result.stderr.splitlines() == expected_warnings, "Unexpected ctrtool warnings")
        require(re.search(r"TitleId:\s+000400000f7a3300", log), "Title ID")
        for name in ["TicketVersion", "TitleVersion"]:
            require(re.search(name + r":\s+1\.1\.1 \(1041\)", log), name)
        require("CTR-P-XGBB" in log, "Product code")
        for service in ["APT:U", "fs:USER", "gsp::Gpu", "hid:USER", "ir:rst", "dsp::DSP"]:
            require("> " + service in log, "Service: " + service)
        for mapping in ["0x1FF50000-0x1FF57FFF", "0x1FF70000-0x1FF77FFF"]:
            require(re.search(r"IoMapping:\s+" + re.escape(mapping), log),
                    "NDSP writable memory mapping: " + mapping)

        raw = elf.read_bytes()
        require(raw[:7] == b"\x7fELF\x01\x01\x01", "ELF32 little-endian format")
        require(struct.unpack_from("<H", raw, 18)[0] == 40, "ARM architecture")
        phoff = struct.unpack_from("<I", raw, 28)[0]
        phsize, phcount = struct.unpack_from("<HH", raw, 42)
        loads = []
        for i in range(phcount):
            ptype, offset, va, _, size, _, _, _ = struct.unpack_from("<8I", raw, phoff + i * phsize)
            if ptype == 1:
                loads.append((offset, va, size))
        require(len(loads) == 3, "ELF LOAD segment count")
        code = (base / "exefs/code.bin").read_bytes()
        origin = min(va for _, va, _ in loads)
        for offset, va, size in loads:
            require(raw[offset:offset + size] == code[va - origin:va - origin + size],
                    "ELF and CIA code differ at " + hex(va))
        icon = (base / "exefs/icon.bin").read_bytes()
        require(icon[:4] == b"SMDH", "SMDH magic")
        require(icon[8:136].decode("utf-16-le").split("\0")[0] == "万砖齐发", "HOME title")
        require(struct.unpack_from("<I", icon, 0x2018)[0] == 0x7fffffff, "Region-free SMDH")
        require(struct.unpack_from("<I", icon, 0x2028)[0] == 0x401, "Visible HOME icon / no save backups")

    disassembly = subprocess.run([objdump, "-d", str(elf)], capture_output=True, text=True, check=True).stdout
    svc = {int(match, 0) for match in re.findall(r"\bsvc\s+(0x[0-9a-fA-F]+|[0-9]+)", disassembly)}
    rsf = (ROOT / "packaging/app.rsf").read_text()
    section = rsf.split("SystemCallAccess:", 1)[1].split("ServiceAccessControl:", 1)[0]
    permitted = {int(n) for n in re.findall(r"^\s+\w+:\s+(\d+)\s*$", section, re.M)}
    require(svc and svc <= permitted, "ELF uses an undeclared SVC: " + str(sorted(svc - permitted)))
    report = {
        "title_id": "000400000F7A3300", "version": "1.1.1", "arm_load_segments_matched": len(loads),
        "declared_svc_checks": len(svc),
        "content_and_section_hashes": "passed", "home_title": "万砖齐发",
        "ndsp_writable_mappings": ["0x1FF50000-0x1FF57FFF", "0x1FF70000-0x1FF77FFF"],
        "official_signature": "not present; 3 expected homebrew signature warnings",
        "device_or_emulator_test": "not performed",
        "artifacts": {name: {"bytes": (ROOT / name).stat().st_size,
                            "sha256": hashlib.sha256((ROOT / name).read_bytes()).hexdigest()}
                      for name in ["many-bricks-3ds.cia", "many-bricks-3ds.3dsx",
                                   "many-bricks-3ds.smdh", "many-bricks-3ds.elf"]},
    }
    rendered = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=args.output.parent,
                                             prefix=args.output.name + ".", suffix=".tmp",
                                             delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(rendered)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, args.output)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()
