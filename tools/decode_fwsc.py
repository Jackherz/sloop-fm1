#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read an FM-1 update package (.fwsc) back: the image size, the free flash, the strings.

The inverse (enough of it) of `tools/fm1pkg_make.py`, and it needs no JieLi toolchain — so it is how
you answer "does the kid build fit?" on any machine, from a package you already have.

  tools/decode_fwsc.py docs/firmware/sloop-2.8.5.fwsc
  tools/decode_fwsc.py PKG.fwsc --app out/app.bin        # write the decrypted app image
  tools/decode_fwsc.py PKG.fwsc --strings 4              # printable runs, with offsets
  tools/decode_fwsc.py PKG.fwsc --strings 4 --grep LOFI  # only runs containing LOFI

The format, in the order it has to be undone:

  1. .fwsc    the first 20 blocks of 0x2F payload bytes each carry ONE identity marker byte after them
              (`(ord(product[i]) + i + 1) & 0xFF`, or 0x7D past the name). Drop those to get the
              "logical" image the packer wrote.
  2. header   logical[0:0x40], ENC stream cipher with key 0xFFFF: crc16, total size, file count.
  3. entries  0x50 bytes each at 0x40, each ENC 0xFFFF with the key restarted per entry:
              `<HHHHIII` type, index, crc16, reserved, offset, length, aligned length, then the name.
              Entry 0 is `flash.bin`; entry 1 is `ota.bin` (the update loader).
  4. app area flash.bin[0x4000:], SFC-encrypted: ENC per 32-byte block, key 0x980F mixed with
              `(offset - base) >> 2`, base 0. Inside it, at 0x120, is the app image, 0xFF-padded to
              APP_SLOT. So "used" = the last non-0xFF byte + 1, which is the number the linker printed.
"""
import argparse
import struct
import sys
from pathlib import Path

APP_SLOT = 0x8DFBC          # tools/fm1pkg_make.py
APP_OFF = 0x120             # app.bin inside the app area
APP_AREA = 0x4000           # flash.bin offset of the app area
KEY = 0x980F                # the package's chip key, as build.py passes it
BLOCKS = 20                 # identity marker bytes: one after each of the first 20 blocks
BLK = 0x30                  #   those blocks are 0x30 raw bytes ...
KEEP = 0x2F                 #   ... of which 0x2F are payload
MARKER = 0x7D               # the filler used past the end of the product name
STUB = bytes.fromhex("04818000")    # first 4 bytes of every FM-1 app image (build.py asserts this)


def _crc_table():
    t = []
    for i in range(256):
        c = i << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else (c << 1)
        t.append(c & 0xFFFF)
    return t


_T = _crc_table()


def crc16(data, crc=0):
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ _T[((crc >> 8) ^ b) & 0xFF]
    return crc


def enc(buf, off, size, key=0xFFFF):
    """JieLi ENC stream cipher, in place (the same call encrypts and decrypts)"""
    for i in range(size):
        buf[off + i] ^= key & 0xFF
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF


def sfc(buf, off, size, base, key):
    """SFC cipher: ENC per 32-byte block, key mixed with the block address"""
    for i in range(0, size, 32):
        enc(buf, off + i, min(size - i, 32), key ^ ((off + i - base) >> 2))


def strip_markers(raw):
    """the logical image, and the product name recovered from the marker bytes"""
    if len(raw) < BLOCKS * BLK + 0x40:
        sys.exit(f"{len(raw)} B is too small to be an FM-1 package (the header alone needs "
                 f"{BLOCKS * BLK + 0x40} B)")
    log = bytearray(len(raw) - BLOCKS)
    name = ""
    for i in range(BLOCKS):
        log[i * KEEP:(i + 1) * KEEP] = raw[i * BLK:i * BLK + KEEP]
        m = raw[i * BLK + KEEP]
        if m != MARKER:
            name += chr((m - i - 1) & 0xFF)
    log[BLOCKS * KEEP:] = raw[BLOCKS * BLK:]
    return log, name


def entries(log):
    """the ufw entry list: [(name, offset, length, crc_ok)]"""
    hdr = bytearray(log[0:0x40])
    enc(hdr, 0, 0x40)
    total, count = struct.unpack_from("<IH", hdr, 4)
    if not 1 <= count <= 8 or not 0x40 < total <= len(log):
        sys.exit(f"not an FM-1 package: the header decodes to {count} file(s), {total:,} B "
                 f"(the package is {len(log):,} B) — wrong marker layout, or not a .fwsc")
    out = []
    for i in range(count):
        at = 0x40 + i * 0x50
        if at + 0x50 > len(log):
            sys.exit(f"truncated package: entry {i} starts past the end")
        e = bytearray(log[at:at + 0x50])
        enc(e, 0, 0x50)
        typ, idx, crc, _rsvd, off, length, _aligned = struct.unpack_from("<HHHHIII", e, 0)
        nm = bytes(e[0x40:0x50]).split(b"\0")[0].decode("ascii", "replace")
        if off + length > len(log):
            sys.exit(f"truncated package: {nm or f'entry {i}'} claims {length:,} B at 0x{off:x}, "
                     f"past the end of the {len(log):,} B image")
        data = bytes(log[off:off + length])
        out.append((nm, typ, off, length, crc16(data) == crc))
    return total, out


def decode(path):
    raw = Path(path).read_bytes()
    log, product = strip_markers(raw)
    total, ents = entries(log)
    flash = None
    for nm, _typ, off, length, ok in ents:
        if nm == "flash.bin":
            flash = bytearray(log[off:off + length])
            if not ok:
                sys.exit(f"{path}: flash.bin fails its CRC16 — wrong key, or a truncated package")
    if flash is None:
        sys.exit(f"{path}: no flash.bin entry (is this an FM-1 package?)")
    need = APP_AREA + APP_OFF + APP_SLOT
    if len(flash) < need:
        sys.exit(f"{path}: flash.bin is {len(flash)} B, too short to hold a {APP_SLOT} B app slot")
    region = bytearray(flash[APP_AREA:APP_AREA + APP_OFF + APP_SLOT])
    sfc(region, 0, len(region), 0, KEY)
    app = region[APP_OFF:APP_OFF + APP_SLOT]
    used = len(app.rstrip(b"\xFF"))
    return {
        "product": product, "package": len(raw), "total": total, "entries": ents,
        "flash": flash, "app": bytes(app[:used]), "used": used,
    }


def printable_runs(buf, min_len):
    """(offset, text) for each run of >= min_len printable ASCII bytes"""
    out, start = [], None
    for i, b in enumerate(buf + b"\0"):
        if 0x20 <= b <= 0x7E:
            if start is None:
                start = i
        elif start is not None:
            if i - start >= min_len:
                out.append((start, buf[start:i].decode("ascii")))
            start = None
    return out


def main():
    ap = argparse.ArgumentParser(description="decode an FM-1 .fwsc package (no SDK needed)")
    ap.add_argument("package", type=Path)
    ap.add_argument("--app", type=Path, help="write the decrypted app image here")
    ap.add_argument("--strings", type=int, default=0, metavar="N",
                    help="list printable runs of at least N bytes, with offsets")
    ap.add_argument("--grep", help="with --strings: only runs containing this text")
    a = ap.parse_args()

    d = decode(a.package)
    slot = APP_SLOT
    used = d["used"]

    print(f"package   {a.package}: {d['package']:,} B")
    if d["product"]:
        print(f"identity  {d['product']}")
    for nm, typ, off, length, ok in d["entries"]:
        print(f"entry {typ:>3}  {nm:<10} {length:>9,} B at 0x{off:x}  crc {'ok' if ok else 'BAD'}")
    ok = d["app"][:4] == STUB
    print(f"stub      {d['app'][:4].hex()} {'= FM-1 app' if ok else '(NOT an FM-1 app) '}")
    print()
    print(f"app slot  {slot:>9,} B")
    print(f"image     {used:>9,} B   {100.0 * used / slot:5.1f} % of the slot")
    print(f"free      {slot - used:>9,} B   {100.0 * (slot - used) / slot:5.1f} %")
    if not ok:
        print("\nwarning: the stub does not match — the decode is probably wrong (check the key)")

    if a.app:
        a.app.parent.mkdir(parents=True, exist_ok=True)
        a.app.write_bytes(d["app"])
        print(f"\nwrote     {a.app} ({used:,} B)")

    if a.strings:
        runs = printable_runs(d["app"], a.strings)
        total = sum(len(t) for _, t in runs)
        print(f"\nstrings   {len(runs):,} runs >= {a.strings} B = {total:,} B "
              f"({100.0 * total / used:.1f} % of the image)")
        for off, text in runs:
            if a.grep and a.grep not in text:
                continue
            print(f"  {off:>9,}  {text}")


if __name__ == "__main__":
    main()
