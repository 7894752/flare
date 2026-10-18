"""Inspect or normalize flare's link metadata without executing the image.

Uses Python 3's standard library. The apply command writes the recorded COFF/debug
timestamp, PDB GUID and age to a separate copy. Code, data, layout, Rich counts and
the embedded PDB path are left intact. Matching metadata alone does not establish
byte identity; build.py reports the complete file's SHA-256.

Examples (run from the repository root):
    python -I build/stamp_meta.py apply --input build/out/flare.exe
    python -I build/stamp_meta.py check --input <built-exe> --original <original-exe>
    python -I build/stamp_meta.py extract --original <original-exe>

extract replaces the meta_stamp.json beside this script. apply keeps its input
and defaults to an output named <input-stem>_identical<input-suffix>.
check reports metadata differences; its exit status does not indicate equality.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys

POLICY = Path(__file__).resolve().parent / 'meta_stamp.json'


def rol(v, n):
    """Rotate a 32-bit word left for the Rich checksum."""
    n &= 31
    return ((v << n) | (v >> (32 - n))) & 0xffffffff if n else v


def parse_rich(data):
    """Return (start, end, key, entries) of the Rich header, or None."""
    rich = data.find(b'Rich', 0x40, 0x400)
    if rich < 0:
        return None
    key = struct.unpack_from('<I', data, rich + 4)[0]
    dans = None
    for off in range(0x40, rich, 4):
        if struct.unpack_from('<I', data, off)[0] ^ key == 0x536e6144:  # 'DanS'
            dans = off
            break
    if dans is None:
        return None
    words = [struct.unpack_from('<I', data, o)[0] ^ key for o in range(dans + 16, rich, 4)]
    entries = [(words[i], words[i + 1]) for i in range(0, len(words), 2)]
    return dans, rich + 8, key, entries


def rich_key(data, dans, entries):
    """Calculate the Rich XOR key from the DOS stub and tool/count entries."""
    cs = dans
    for i in range(dans):
        if 0x3c <= i < 0x40:
            continue  # e_lfanew is excluded from the checksum
        cs = (cs + rol(data[i], i)) & 0xffffffff
    for comp, count in entries:
        cs = (cs + rol(comp, count)) & 0xffffffff
    return cs


def build_rich(data, dans, entries):
    """Encode Rich entries and return (header bytes, key); unused by stamping."""
    key = rich_key(data, dans, entries)
    words = [0x536e6144, 0, 0, 0]
    for comp, count in entries:
        words += [comp, count]
    blob = b''.join(struct.pack('<I', w ^ key) for w in words) + b'Rich' + struct.pack('<I', key)
    return blob, key


def locate(data):
    """Locate the COFF timestamp and debug entries in a PE32 file."""
    pe_offset = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe_offset:pe_offset + 4] == b'PE\0\0', 'Expected a PE executable; pass a linked flare.exe.'
    section_count, = struct.unpack_from('<H', data, pe_offset + 6)
    optional_size, = struct.unpack_from('<H', data, pe_offset + 20)
    optional = pe_offset + 24
    sections = []
    for i in range(section_count):
        offset = optional + optional_size + 40 * i
        virtual_size, address, raw_size, raw_ptr = struct.unpack_from('<IIII', data, offset + 8)
        sections.append((address, max(virtual_size, raw_size), raw_ptr))
    debug_rva, debug_size = struct.unpack_from('<II', data, optional + 96 + 6 * 8)

    def fileoff(rva):
        for address, size, raw in sections:
            if address <= rva < address + size:
                return raw + rva - address
        raise ValueError('RVA %s is outside the PE sections; check the input executable.' % hex(rva))
    debug = []
    for i in range(debug_size // 28):
        offset = fileoff(debug_rva) + 28 * i
        timestamp, major, minor, kind, size, rva, ptr = struct.unpack_from('<IHHIIII', data, offset + 4)
        debug.append({'entry_offset': offset, 'type': kind, 'size': size, 'raw_ptr': ptr})
    return {'pe': pe_offset, 'coff_timestamp': pe_offset + 8, 'debug': debug}


def extract_policy(path):
    """Read recorded metadata values from an original image without copying code."""
    data = path.read_bytes()
    loc = locate(data)
    cv = [d for d in loc['debug'] if d['type'] == 2]
    assert len(cv) == 1, 'Expected one CodeView entry; use the original 2003-10-17 flare.exe.'
    p = cv[0]['raw_ptr']
    assert data[p:p + 4] == b'RSDS', 'Expected RSDS debug metadata; use the original 2003-10-17 flare.exe.'
    guid = data[p + 4:p + 20]
    age, = struct.unpack_from('<I', data, p + 20)
    ts, = struct.unpack_from('<I', data, loc['coff_timestamp'])
    policy = {
        'source': 'original flare.exe (values only)',
        'timestamp': ts,
        'rsds_guid_hex': guid.hex(),
        'rsds_age': age,
        'notes': 'timestamp/GUID/age cannot be produced by link.exe 7.00.9466; stamped by stamp_meta.py apply',
    }
    return policy


def apply_policy(data, policy):
    """Return a new image with only timestamp/GUID/age fields normalized."""
    data = bytearray(data)
    loc = locate(data)
    codeview = [d for d in loc['debug'] if d['type'] == 2]
    if len(codeview) != 1:
        raise ValueError('The stamp requires exactly one CodeView entry; use the output linked by build.py.')
    guid = bytes.fromhex(policy['rsds_guid_hex'])
    if len(guid) != 16:
        raise ValueError('RSDS GUID must contain 16 bytes; check rsds_guid_hex in meta_stamp.json.')
    if data[codeview[0]['raw_ptr']:codeview[0]['raw_ptr'] + 4] != b'RSDS':
        raise ValueError('The stamp requires an RSDS record; use the output linked by build.py.')
    struct.pack_into('<I', data, loc['coff_timestamp'], policy['timestamp'])
    for d in loc['debug']:
        struct.pack_into('<I', data, d['entry_offset'] + 4, policy['timestamp'])
        if d['type'] == 2:
            p = d['raw_ptr']
            assert data[p:p + 4] == b'RSDS'
            data[p + 4:p + 20] = guid
            struct.pack_into('<I', data, p + 20, policy['rsds_age'])
    return bytes(data)


def diff_report(a, b):
    """Compare Rich and debug metadata; a is the original, b is the built image."""
    out = []
    la, lb = locate(a), locate(b)
    ra, rb = parse_rich(a), parse_rich(b)
    if bool(ra) != bool(rb):
        out.append('Rich header presence differs')
    if ra and rb:
        ea, eb = dict((c, n) for c, n in ra[3]), dict((c, n) for c, n in rb[3])
        for c in sorted(set(ea) | set(eb)):
            if ea.get(c) != eb.get(c):
                out.append('rich %#x prod=%#x build=%d: target=%s candidate=%s' % (c, c >> 16, c & 0xffff, ea.get(c), eb.get(c)))
        if ra[2] != rb[2]:
            out.append('rich key target=%#x candidate=%#x' % (ra[2], rb[2]))
    for name, off in (('coff_timestamp', 8),):
        va = struct.unpack_from('<I', a, la['pe'] + off)[0]
        vb = struct.unpack_from('<I', b, lb['pe'] + off)[0]
        if va != vb:
            out.append('%s target=%#x candidate=%#x' % (name, va, vb))
    if len(la['debug']) != len(lb['debug']):
        out.append('debug entry count target=%d candidate=%d' % (len(la['debug']), len(lb['debug'])))
    for da, db in zip(la['debug'], lb['debug']):
        if (da['type'], da['size']) != (db['type'], db['size']):
            out.append('debug type/size target=%s/%s candidate=%s/%s' %
                       (da['type'], da['size'], db['type'], db['size']))
        ta = struct.unpack_from('<I', a, da['entry_offset'] + 4)[0]
        tb = struct.unpack_from('<I', b, db['entry_offset'] + 4)[0]
        if ta != tb:
            out.append('debug timestamp target=%#x candidate=%#x' % (ta, tb))
        pa, pb = da['raw_ptr'], db['raw_ptr']
        if a[pa:pa + da['size']] != b[pb:pb + db['size']]:
            out.append('debug type %d record differs: %r vs %r' % (da['type'], a[pa:pa + da['size']], b[pb:pb + db['size']]))
    return out


def main():
    """Apply the stamp, inspect metadata or regenerate the recorded policy."""
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['extract', 'apply', 'check'], help='extract policy, apply it to a copy, or compare metadata')
    ap.add_argument('--input', metavar='FILE', type=Path, help='built PE image (required for apply and check)')
    ap.add_argument('--output', metavar='FILE', type=Path, help='stamped copy for apply (default: <input-stem>_identical<input-suffix>)')
    ap.add_argument('--original', metavar='FILE', type=Path, help='original flare.exe (required for extract and check; read only)')
    args = ap.parse_args()
    if args.cmd == 'extract':
        if not args.original:
            raise SystemExit('extract requires --original <original-exe>; it replaces the recorded meta_stamp.json.')
        POLICY.write_text(json.dumps(extract_policy(args.original), indent=2) + chr(10), encoding='utf-8')
        print('wrote', POLICY)
        return 0
    if not args.input:
        raise SystemExit('%s requires --input <built-exe>. See --help for examples.' % args.cmd)
    if args.cmd == 'apply':
        dst = args.output or args.input.with_name(args.input.stem + '_identical' + args.input.suffix)
        if dst.resolve() == args.input.resolve():
            raise SystemExit('Refusing to overwrite the unstamped input. Pass a different --output <file>.')
        dst.write_bytes(apply_policy(args.input.read_bytes(), json.loads(POLICY.read_text(encoding='utf-8'))))
        print('stamped', dst)
        return 0
    if not args.original:
        raise SystemExit('check requires --original <original-exe> and --input <built-exe>.')
    rep = diff_report(args.original.read_bytes(), args.input.read_bytes())
    print(chr(10).join(rep) if rep else 'stamp fields/CodeView/Rich equal to the original (other PE fields are not checked)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
