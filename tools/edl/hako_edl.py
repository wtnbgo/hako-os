#!/usr/bin/env python3
"""Back up a handheld and write OpenABL over Qualcomm EDL (9008 mode).

Works on Windows (QPST: QSaharaServer + fh_loader) and on Linux (qdl).
Only the Python standard library is used. See docs chapter 1.

  hako_edl.py <device> backup  <dir>              GPT + device-specific partitions -> <dir>
  hako_edl.py <device> check   <dir> <openabl.elf>  read-only: what is in abl_a / abl_b now
  hako_edl.py <device> flash   <dir> <openabl.elf> [--stock F]  write OpenABL to abl_a and abl_b
  hako_edl.py <device> restore <dir> [--stock F]  write the backed-up abl_a / abl_b back
                                                  (or F, e.g. abl.elf from the maker's package)
  hako_edl.py <device> reset                      Windows: leave EDL after a failed step (reboot)

  <device>  rpmini (Retroid Pocket Mini V1, SM8250) or rg55g1 (Anbernic RG 55G1, SM4450)
  <dir>     the backup folder. backup creates it; the others read gpt_lun*.bin,
            abl_a.img, abl_b.img and SHA256SUMS from it.

Options:
  --loader FILE   the firehose loader (default: <dir>/<the device's loader name>)
  --port COMn     Windows: the EDL COM port (default: found automatically)
  --qpst DIR      Windows: the QPST bin folder (default: C:\\Program Files (x86)\\Qualcomm\\QPST\\bin)
  --qdl FILE      Linux: the qdl program (default: qdl)
  --stock FILE    the stock ABL (restore writes it instead of the backup abl_a.img; check and
                  flash compare with it when the backup was taken after flashing)
  --yes           do not ask before writing

Safety: flash writes only when the loader and the elf match the known sha256, the
backup matches its SHA256SUMS, and both abl partitions hold either the backed-up
(stock) ABL or OpenABL already. Every write is read back and compared.
On Linux, qdl reboots the device at the end of each run, so flash and restore
need the device to enter EDL twice (once to check, once to write); the script waits.
"""
import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import time

SS = 4096  # UFS sector size

DEVICES = {
    'rpmini': {
        'name': 'Retroid Pocket Mini (V1, SM8250)',
        'luns': 6,
        'loader': 'prog_firehose_ddr.elf',
        'loader_sha256': {
            # Retroid's Android 13 recovery package (RPMini_A13_20250121)
            '690bd3920e637d66584314929f1198818b8163a1f84a4d3200c60922defb9d16',
        },
        'openabl_sha256': '628dbff74f5b570b897c14725f2c36ff2ca5fe018998e2c248a3d15d906ccef9',  # OpenABL v1.2 abl_signed-SM8250.elf
        'openabl_size': 532480,
        # the stock ABL is also abl.elf in Retroid's Android 13 recovery package (RPMini_A13_20250121)
        'backup': 'abl_a,abl_b,apdp,cdt,ddr,devinfo,dip,frp,fsc,fsg,keystore,limits,limits-cdsp,logfs,misc,'
                  'modemst1,modemst2,msadp,nvdata1,nvdata2,persist,qpdata1,qpdata2,secdata,splash,spunvm,ssd,'
                  'storsec,uefivarstore',
    },
    'rg55g1': {
        'name': 'Anbernic RG 55G1 (SM4450)',
        'luns': 7,
        'loader': 'xbl_s_devprg_ns.melf',
        'loader_sha256': {
            # TheGammaSqueeze/sd-tools stock/firmware/
            '4268562be8c612b3036fde519a64edac3afd65b8b31c483dcd345b24cc5ee19c',
        },
        'openabl_sha256': '8d73232614bdf50d602dfb934157328b95fc90f1ea22ba71dbc8836bc7aa8a3b',  # OpenABL v1.2 abl_signed-SM4450.elf
        'openabl_size': 532480,
        'backup': 'abl_a,abl_b,apdp,cdt,ddr,devinfo,dip,dtbo_a,frp,fsc,fsg,keystore,limits,limits-cdsp,logfs,'
                  'misc,modemst1,modemst2,persist,secdata,splash,spunvm,ssd,storsec,uefivarstore,vendor_boot_a',
    },
}


def say(*a):
    print(*a, flush=True)


def die(msg):
    say('ABORT:', msg)
    say('RESULT=ABORTED (nothing more was written)')
    sys.exit(1)


def sha(path, size=None):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        left = size if size is not None else -1
        while left:
            b = f.read(1 << 20 if left < 0 else min(1 << 20, left))
            if not b:
                break
            h.update(b)
            if left > 0:
                left -= len(b)
    return h.hexdigest()


# ---- GPT ----

def parse_gpt(path):
    b = open(path, 'rb').read()
    hdr = b[SS:SS + 92]
    if hdr[:8] != b'EFI PART':
        die(f'{path}: no GPT header in sector 1')
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


def gpt_table(d, luns):
    rows = []
    for lun in range(luns):
        rows += [(lun, *r) for r in parse_gpt(os.path.join(d, f'gpt_lun{lun}.bin'))]
    return rows


def find(rows, name):
    hits = [(lun, start, cnt) for lun, n, start, cnt in rows if n == name]
    if not hits:
        die(f'{name} is not in the GPT')
    return hits[0]


# ---- backends: one "session" = one EDL connection ----

class Op:
    """read or write <file> at lun/start (count sectors); name only = by partition label (qdl)."""

    def __init__(self, kind, file, lun=None, start=None, count=None, name=None):
        self.kind, self.file, self.lun, self.start, self.count, self.name = kind, file, lun, start, count, name


def clear_outputs(d, ops):
    """fh_loader overwrites an existing file in place without truncating it: remove old outputs first"""
    for o in ops:
        if o.kind == 'read':
            p = os.path.join(d, os.path.basename(o.file))
            if os.path.exists(p):
                os.remove(p)


class Windows:
    keeps_session = True  # fh_loader can be run many times after one QSaharaServer

    def __init__(self, a):
        self.bin = a.qpst or r'C:\Program Files (x86)\Qualcomm\QPST\bin'
        for exe in ('QSaharaServer.exe', 'fh_loader.exe'):
            if not os.path.exists(os.path.join(self.bin, exe)):
                die(f'{exe} not found in {self.bin} (install QPST, or pass --qpst)')
        self.port = a.port
        self.started = False

    def find_port(self):
        ps = ('Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match "QDLoader 9008" } '
              '| ForEach-Object { if ($_.Name -match "\\((COM\\d+)\\)") { $Matches[1] } }')
        r = subprocess.run(['powershell', '-NoProfile', '-Command', ps], capture_output=True, text=True)
        ports = r.stdout.split()
        return ports[0] if len(ports) == 1 else None

    def wait(self):
        if self.port:
            return
        say('waiting for the device in EDL (Qualcomm HS-USB QDLoader 9008)...')
        while True:
            p = self.find_port()
            if p:
                self.port = p
                say('EDL port:', p)
                return
            time.sleep(1)

    def fh(self, d, *args):
        cmd = [os.path.join(self.bin, 'fh_loader.exe'), f'--port=\\\\.\\{self.port}', '--memoryname=UFS',
               f'--search_path={d}', f'--mainoutputdir={d}', '--noprompt', '--zlpawarehost=1', *args]
        r = subprocess.run(cmd, capture_output=True, text=True, errors='replace')
        out = r.stdout + r.stderr
        if r.returncode != 0 or 'All Finished Successfully' not in out:
            say(out[-3000:])
            die('fh_loader failed')

    def session(self, d, loader, ops):
        clear_outputs(d, ops)
        if not self.started:
            self.wait()
            r = subprocess.run([os.path.join(self.bin, 'QSaharaServer.exe'), '-p', f'\\\\.\\{self.port}',
                                '-s', f'13:{loader}'], capture_output=True, text=True, errors='replace')
            if 'Sahara protocol completed' not in r.stdout:
                say(r.stdout[-2000:])
                die('could not send the loader (is the device in EDL, and is this the right loader?)')
            self.started = True
        for kind in ('read', 'write'):
            lines = [entry(o) for o in ops if o.kind == kind]
            if not lines:
                continue
            x = os.path.join(d, f'hako_{kind}.xml')
            with open(x, 'w') as f:
                f.write('<?xml version="1.0" ?>\n<data>\n' + '\n'.join(lines) + '\n</data>\n')
            if kind == 'read':
                self.fh(d, f'--sendxml={x}', '--convertprogram2read')
            else:
                self.fh(d, f'--sendxml={x}')

    def end(self, d):
        if self.started:
            self.fh(d, '--reset')
            self.started = False


def entry(o):
    if o.start is None:
        die('internal: fh_loader needs the position of ' + o.file)
    return (f'  <program SECTOR_SIZE_IN_BYTES="{SS}" filename="{os.path.basename(o.file)}" label="{o.name or o.file}" '
            f'physical_partition_number="{o.lun}" start_sector="{o.start}" num_partition_sectors="{o.count}" '
            f'file_sector_offset="0" sparse="false" partofsingleimage="false" readbackverify="false" '
            f'size_in_KB="{o.count * SS // 1024}.0" start_byte_hex="0x{o.start * SS:x}"/>')


class Linux:
    keeps_session = False  # qdl reboots the device when it finishes

    def __init__(self, a):
        self.qdl = a.qdl or 'qdl'
        if not shutil.which(self.qdl):
            die('qdl not found (apt install qdl)')
        self.runs = 0

    @staticmethod
    def present():
        base = '/sys/bus/usb/devices'
        for n in os.listdir(base):
            try:
                v = open(f'{base}/{n}/idVendor').read().strip()
                p = open(f'{base}/{n}/idProduct').read().strip()
            except OSError:
                continue
            if (v, p) == ('05c6', '9008'):
                return True
        return False

    def wait(self):
        if self.runs:
            say('qdl rebooted the device. Put it into EDL again for the next step.')
            while self.present():
                time.sleep(0.5)
        if not self.present():
            say('waiting for the device in EDL (05c6:9008)...')
            while not self.present():
                time.sleep(0.5)
        else:
            say('the device was already in EDL (if qdl fails, leave EDL and enter it again)')
        time.sleep(1)  # let the device settle before qdl talks to it
        say('EDL device found')

    def session(self, d, loader, ops):
        clear_outputs(d, ops)
        self.wait()
        args = []
        for o in ops:
            if o.start is None:
                addr = o.name
            elif o.kind == 'read':
                addr = f'{o.lun}/{o.start}+{o.count}'
            else:
                addr = f'{o.lun}/{o.start}'
            args += [o.kind, addr, os.path.join(d, os.path.basename(o.file))]
        r = subprocess.run([self.qdl, '--storage=ufs', loader, *args], capture_output=True, text=True,
                           errors='replace')
        self.runs += 1
        if r.returncode != 0:
            out = r.stdout + r.stderr
            say(out[-3000:])
            if 'sahara' in out:
                die('qdl got no answer from the device: leave EDL (hold power for 10-15 s), '
                    'then enter EDL again and run this again')
            die('qdl failed (permission? see the udev rule in docs chapter 1)')

    def end(self, d):
        pass


def backend(a):
    return Windows(a) if os.name == 'nt' else Linux(a)


# ---- commands ----

def check_loader(dev, path):
    if not os.path.exists(path):
        die(f'loader not found: {path} (copy {dev["loader"]} into the folder, or pass --loader)')
    h = sha(path)
    if h not in dev['loader_sha256']:
        die(f'{path}: unknown loader (sha256 {h})')
    say('loader ok')


def read_sums(d):
    p = os.path.join(d, 'SHA256SUMS')
    if not os.path.exists(p):
        die(f'{p} not found (run backup first)')
    sums = {}
    for line in open(p):
        h, f = line.split()
        sums[f.lstrip('*')] = h
    return sums


def check_backup(d, luns, names=('abl_a.img', 'abl_b.img')):
    sums = read_sums(d)
    for f in [f'gpt_lun{l}.bin' for l in range(luns)] + list(names):
        p = os.path.join(d, f)
        if f not in sums or not os.path.exists(p) or sha(p) != sums[f]:
            die(f'{f} is missing or does not match SHA256SUMS')
    say('backup ok')


def cmd_backup(dev, d, be, loader):
    os.makedirs(d, exist_ok=True)
    names = dev['backup'].split(',')
    gpts = [Op('read', f'gpt_lun{l}.bin', l, 0, 6) for l in range(dev['luns'])]
    if be.keeps_session:
        be.session(d, loader, gpts)
        rows = gpt_table(d, dev['luns'])
        be.session(d, loader, [Op('read', f'{n}.img', *find(rows, n), name=n) for n in names])
    else:
        be.session(d, loader, gpts + [Op('read', f'{n}.img', name=n) for n in names])
        rows = gpt_table(d, dev['luns'])
    be.end(d)
    with open(os.path.join(d, 'parts.txt'), 'w') as f:
        for lun, n, start, cnt in rows:
            f.write(f'lun{lun} {n} start={start} sectors={cnt} ({cnt * SS // 1024} KB)\n')
    files = [f'gpt_lun{l}.bin' for l in range(dev['luns'])] + [f'{n}.img' for n in names]
    for n in names:
        p = os.path.join(d, f'{n}.img')
        if os.path.getsize(p) != find(rows, n)[2] * SS:
            die(f'{n}.img: size does not match the GPT')
    with open(os.path.join(d, 'SHA256SUMS'), 'w', newline='\n') as f:
        for fn in files:
            f.write(f'{sha(os.path.join(d, fn))}  {fn}\n')
    say(f'wrote {len(files)} files and SHA256SUMS to {d}')
    if is_openabl(dev, os.path.join(d, 'abl_a.img')):
        say('note: abl_a is already OpenABL, so this backup does not hold the stock ABL. '
            'Keep the backup from before flashing too.')
    say('Copy this folder somewhere safe: it holds values unique to this device.')
    say('RESULT=BACKUP_OK')


def is_openabl(dev, path):
    return sha(path, dev['openabl_size']) == dev['openabl_sha256']


def abl_state(dev, path, stock):
    """'OpenABL', 'stock' (the start of the partition equals the stock file) or None"""
    if is_openabl(dev, path):
        return 'OpenABL'
    if stock and sha(path, os.path.getsize(stock)) == sha(stock):
        return 'stock'
    return None


def stock_ref(dev, d, s, stock):
    """the stock ABL to compare abl_<s> with: --stock, or the backup unless it holds OpenABL"""
    if stock:
        return stock
    p = os.path.join(d, f'abl_{s}.img')
    return None if is_openabl(dev, p) else p


def read_current(dev, d, be, loader, rows):
    ops = []
    for s in ('a', 'b'):
        lun, start, cnt = find(rows, f'abl_{s}')
        ops.append(Op('read', f'cur_abl_{s}.img', lun, start, cnt, f'abl_{s}'))
    be.session(d, loader, ops)


def cmd_check(dev, d, be, loader, elf, write=None, stock=None):
    check_backup(d, dev['luns'])
    for s in ('a', 'b'):
        if not stock_ref(dev, d, s, stock):
            say(f'note: the backup abl_{s}.img is already OpenABL (taken after flashing); '
                'it cannot tell what the stock ABL was')
    elf_size = None
    if elf:
        if not os.path.exists(elf):
            die(f'{elf} not found')
        if sha(elf) != dev['openabl_sha256']:
            die(f'{elf}: not the OpenABL v1.2 elf for this device (sha256 {sha(elf)})')
        elf_size = os.path.getsize(elf)
        if elf_size % SS:
            die(f'{elf}: size is not a multiple of {SS}')
        say('elf ok')
    rows = gpt_table(d, dev['luns'])
    read_current(dev, d, be, loader, rows)
    states = {}
    for s in ('a', 'b'):
        st = abl_state(dev, os.path.join(d, f'cur_abl_{s}.img'), stock_ref(dev, d, s, stock))
        say(f'abl_{s}: {st or "UNKNOWN (neither the stock ABL nor OpenABL)"}')
        states[s] = st
    if not write:
        be.end(d)
        say('RESULT=CHECK_OK (nothing written)' if all(states.values()) else 'RESULT=CHECK_UNKNOWN')
        return
    if not all(states.values()):
        be.end(d)
        die('abl is neither the stock ABL nor OpenABL (wrong backup folder, or another device?)')
    write(rows, states)


def write_verify(dev, d, be, loader, rows, src, size, expect, a):
    if not a.yes:
        if input(f'write {os.path.basename(src)} to abl_a and abl_b? type yes: ').strip() != 'yes':
            be.end(d)
            die('not confirmed')
    ops = []
    cnt = size // SS
    for s in ('a', 'b'):
        lun, start, pcnt = find(rows, f'abl_{s}')
        if cnt > pcnt:
            die(f'{src} does not fit abl_{s}')
        dst = os.path.join(d, f'w_abl_{s}{os.path.splitext(src)[1]}')
        if os.path.abspath(src) != os.path.abspath(dst):
            shutil.copyfile(src, dst)
        ops.append(Op('write', dst, lun, start, cnt, f'abl_{s}'))
        ops.append(Op('read', f'readback_abl_{s}.bin', lun, start, cnt, f'abl_{s}'))
    say('writing abl_a, abl_b')
    if be.keeps_session:
        be.session(d, loader, [o for o in ops if o.kind == 'write'])
        be.session(d, loader, [o for o in ops if o.kind == 'read'])
    else:
        be.session(d, loader, ops)  # qdl runs them in order: write a, read a, write b, read b
    be.end(d)
    ok = True
    for s in ('a', 'b'):
        rb = os.path.join(d, f'readback_abl_{s}.bin')
        h = sha(rb, size) if os.path.getsize(rb) == size else 'size %d' % os.path.getsize(rb)
        say(f'readback abl_{s} {"ok" if h == expect else "MISMATCH " + h}')
        ok &= h == expect
    if not ok:
        say('RESULT=VERIFY_FAILED - do not leave it like this: run restore (EDL still works).')
        sys.exit(2)
    say('RESULT=SUCCESS')


def cmd_flash(dev, d, be, loader, elf, a):
    def w(rows, states):
        if states['a'] == states['b'] == 'OpenABL':
            be.end(d)
            say('both already OpenABL; RESULT=NOTHING_TO_DO')
            return
        write_verify(dev, d, be, loader, rows, elf, os.path.getsize(elf), dev['openabl_sha256'], a)
    cmd_check(dev, d, be, loader, elf, w, os.path.abspath(a.stock) if a.stock else None)


def cmd_restore(dev, d, be, loader, a):
    stock = os.path.abspath(a.stock) if a.stock else None
    if stock:
        if not os.path.exists(stock):
            die(f'{stock} not found')
        if is_openabl(dev, stock):
            die(f'{stock} is OpenABL, not the stock ABL')
        src = stock
    else:
        for s in ('a', 'b'):
            if not stock_ref(dev, d, s, None):
                die(f'the backup abl_{s}.img is OpenABL, not the stock ABL; '
                    'pass --stock with the stock ABL (an older backup, or abl.elf from the maker package)')
        if sha(os.path.join(d, 'abl_a.img')) != sha(os.path.join(d, 'abl_b.img')):
            die('abl_a.img and abl_b.img differ; restore them one by one with fh_loader / qdl')
        src = os.path.join(d, 'abl_a.img')
    size = os.path.getsize(src)
    if size % SS:
        die(f'{src}: size is not a multiple of {SS}')

    def w(rows, states):
        if states['a'] == states['b'] == 'stock':
            be.end(d)
            say('both already the stock ABL; RESULT=NOTHING_TO_DO')
            return
        write_verify(dev, d, be, loader, rows, src, size, sha(src), a)
    cmd_check(dev, d, be, loader, None, w, stock)


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('device', choices=sorted(DEVICES))
    p.add_argument('command', choices=['backup', 'check', 'flash', 'restore', 'reset'])
    p.add_argument('dir', nargs='?')
    p.add_argument('elf', nargs='?')
    p.add_argument('--loader')
    p.add_argument('--port')
    p.add_argument('--qpst')
    p.add_argument('--qdl')
    p.add_argument('--stock')
    p.add_argument('--yes', action='store_true')
    a = p.parse_args()
    dev = DEVICES[a.device]
    be = backend(a)
    say('device profile:', dev['name'])
    if a.command == 'reset':
        if not isinstance(be, Windows):
            say('qdl reboots the device by itself after each run. To leave EDL without it, hold power for 10 s.')
            return
        be.wait()
        be.started = True  # works once the loader has been sent (after a failed step)
        be.end('.')
        return
    if not a.dir:
        p.error('<dir> is required')
    d = os.path.abspath(a.dir)
    loader = os.path.abspath(a.loader or os.path.join(d, dev['loader']))
    check_loader(dev, loader)
    if a.command in ('flash',) and not a.elf:
        p.error('<openabl.elf> is required')
    if a.command == 'backup':
        cmd_backup(dev, d, be, loader)
    elif a.command == 'check':
        cmd_check(dev, d, be, loader, a.elf, None, os.path.abspath(a.stock) if a.stock else None)
    elif a.command == 'flash':
        cmd_flash(dev, d, be, loader, os.path.abspath(a.elf), a)
    elif a.command == 'restore':
        cmd_restore(dev, d, be, loader, a)


if __name__ == '__main__':
    main()
