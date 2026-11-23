#!/usr/bin/env python3
"""XML files for Qualcomm fh_loader (QPST) over EDL, for UFS devices.

Used for the Anbernic RG 55G1 (see docs chapter 5): back up partitions and
write OpenABL to abl_a / abl_b. Every XML uses <program> entries; with
fh_loader --convertprogram2read they read into <filename> instead of writing.

  edl_xml.py gpt    <dir> [luns]            read_gpt.xml: first 6 sectors of each LUN
  edl_xml.py backup <dir> <names> [luns]    read_parts.xml + parts.txt from gpt_lun*.bin
  edl_xml.py flash  <dir> <file> <names> [luns]
                                            write_<file>.xml (write <file> to each
                                            partition) and readback_<file>.xml

<dir> holds gpt_lun<N>.bin (from read_gpt.xml) and receives the XML files.
<names> is a comma-separated list of partition labels; [luns] defaults to
0,1,2,3,4,5,6. The sector size is 4096 (UFS).
"""
import os
import struct
import sys

SS = 4096


def entry(fn, lun, start, count, label):
    return (f'  <program SECTOR_SIZE_IN_BYTES="{SS}" filename="{fn}" label="{label}" '
            f'physical_partition_number="{lun}" start_sector="{start}" '
            f'num_partition_sectors="{count}" file_sector_offset="0" sparse="false" '
            f'partofsingleimage="false" readbackverify="false" '
            f'size_in_KB="{count * SS // 1024}.0" start_byte_hex="0x{start * SS:x}"/>')


def write_xml(path, lines):
    with open(path, 'w') as f:
        f.write('<?xml version="1.0" ?>\n<data>\n' + '\n'.join(lines) + '\n</data>\n')
    print('wrote', path)


def parse_gpt(path):
    b = open(path, 'rb').read()
    hdr = b[SS:SS + 92]
    if hdr[:8] != b'EFI PART':
        raise SystemExit(f'{path}: no GPT header in sector 1')
    lba, n, esz = struct.unpack_from('<QII', hdr, 72)
    out = []
    for i in range(n):
        off = lba * SS + i * esz
        if off + esz > len(b):
            break
        e = b[off:off + esz]
        if e[:16] == b'\0' * 16:
            continue
        first, last = struct.unpack_from('<QQ', e, 32)
        out.append((e[56:128].decode('utf-16le').rstrip('\0'), first, last - first + 1))
    return out


def table(d, luns):
    rows = []
    for lun in luns:
        p = os.path.join(d, f'gpt_lun{lun}.bin')
        if os.path.exists(p):
            rows += [(lun, *r) for r in parse_gpt(p)]
    if not rows:
        raise SystemExit(f'no gpt_lun*.bin in {d} (run fh_loader with read_gpt.xml first)')
    return rows


def find(rows, names):
    found = {}
    for lun, name, start, cnt in rows:
        if name in names and name not in found:
            found[name] = (lun, start, cnt)
    missing = [n for n in names if n not in found]
    if missing:
        print('not in the GPT:', ', '.join(missing))
    return found


def main(a):
    if len(a) < 2:
        raise SystemExit(__doc__)
    cmd, d = a[0], a[1]
    if cmd == 'gpt':
        luns = [int(x) for x in (a[2] if len(a) > 2 else '0,1,2,3,4,5,6').split(',')]
        write_xml(os.path.join(d, 'read_gpt.xml'),
                  [entry(f'gpt_lun{l}.bin', l, 0, 6, f'gpt{l}') for l in luns])
    elif cmd == 'backup':
        names = a[2].split(',')
        luns = [int(x) for x in (a[3] if len(a) > 3 else '0,1,2,3,4,5,6').split(',')]
        rows = table(d, luns)
        with open(os.path.join(d, 'parts.txt'), 'w') as f:
            for lun, name, start, cnt in rows:
                f.write(f'lun{lun} {name} start={start} sectors={cnt} ({cnt * SS // 1024} KB)\n')
        found = find(rows, names)
        write_xml(os.path.join(d, 'read_parts.xml'),
                  [entry(f'{n}.img', *found[n], n) for n in names if n in found])
        print(f'{sum(c for _, _, c in found.values()) * SS / 1048576:.1f} MB')
    elif cmd == 'flash':
        f, names = a[2], a[3].split(',')
        luns = [int(x) for x in (a[4] if len(a) > 4 else '0,1,2,3,4,5,6').split(',')]
        size = os.path.getsize(os.path.join(d, f))
        if size % SS:
            raise SystemExit(f'{f}: size {size} is not a multiple of {SS}')
        cnt = size // SS
        found = find(table(d, luns), names)
        if len(found) != len(names):
            raise SystemExit('stopped: a partition is missing')
        for n, (lun, start, pcnt) in found.items():
            if cnt > pcnt:
                raise SystemExit(f'{f} ({cnt} sectors) does not fit {n} ({pcnt} sectors)')
        base = os.path.splitext(os.path.basename(f))[0]
        write_xml(os.path.join(d, f'write_{base}.xml'),
                  [entry(f, found[n][0], found[n][1], cnt, n) for n in names])
        write_xml(os.path.join(d, f'readback_{base}.xml'),
                  [entry(f'readback_{n}.bin', found[n][0], found[n][1], cnt, n) for n in names])
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
