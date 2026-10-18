#!/usr/bin/env python3
"""Fetch and verify the Visual C++ 7.0 files used by build.py.

Uses Python 3's standard library and an installed 7-Zip executable. Archive names,
file selections and SHA-256 values are pinned in toolchain_manifest.json.
The fetcher extracts files from disc images; it never runs their installers.
Microsoft files are not distributed with this repository.

Examples (run from the repository root):
    python -I build/fetch_toolchain.py
    python -I build/fetch_toolchain.py --7z "C:\\Program Files\\7-Zip\\7z.exe" --keep
    python -I build/fetch_toolchain.py --verify-only

The default installation is build/toolchain/vc7, which build.py finds automatically.
Downloads are removed unless --keep is supplied. See build/README.md for details.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

HERE = Path(__file__).resolve().parent
MANIFEST = HERE / 'toolchain_manifest.json'
DEFAULT_DEST = HERE / 'toolchain'
URL = 'https://archive.org/download/%s/%s'


def log(*values):
    """Flush progress messages before starting a download or extraction."""
    print(*values, flush=True)


def sha256(path):
    """Hash a file without loading the whole archive into memory."""
    digest = hashlib.sha256()
    with open(path, 'rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def remove(path):
    """Delete a temporary file; on Windows a scanner may hold it briefly, so retry and never fail the fetch."""
    for _ in range(10):
        try:
            os.chmod(path, 0o666)
            Path(path).unlink()
            return
        except FileNotFoundError:
            return
        except OSError:
            time.sleep(2)
    log('  warning: could not delete %s; remove it manually when it is no longer in use.' % path)


def find_7z(arg):
    """Find 7-Zip from the CLI, environment, PATH or common Windows locations."""
    candidates = [arg] if arg else []
    candidates += [os.environ.get('SEVENZIP'), shutil.which('7z'), shutil.which('7za'), shutil.which('7zz'),
                   r'C:\Program Files\7-Zip\7z.exe', r'C:\Program Files (x86)\7-Zip\7z.exe']
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return str(Path(candidate))
    raise SystemExit('7-Zip not found. Install it (https://www.7-zip.org/), put 7z on PATH or pass --7z <path to 7z.exe>.')


def run7z(sevenzip, args, cwd):
    """Extract with 7-Zip, allowing warnings because file hashes are checked later."""
    result = subprocess.run([sevenzip, *args], cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode not in (0, 1):
        raise SystemExit('7-Zip failed (exit %d). Check the archive and free disk space, then retry:\n%s\n%s' %
                         (result.returncode, ' '.join(args), result.stdout.decode('mbcs', 'replace')[-2000:]))


def download(src, target):
    """Download a pinned archive with resume/retries; verify its size and SHA-256."""
    url = URL % (src['item'], urllib.parse.quote(src['filename']))
    if target.exists() and target.stat().st_size == src['size'] and sha256(target) == src['sha256']:
        log('  already downloaded:', target.name)
        return
    part = target.with_suffix(target.suffix + '.part')
    for attempt in range(1, 7):
        have = part.stat().st_size if part.exists() else 0
        if have >= src['size']:
            break
        request = urllib.request.Request(url, headers={'Range': 'bytes=%d-' % have} if have else {})
        last = -10
        try:
            with urllib.request.urlopen(request, timeout=60) as response, open(part, 'ab' if have else 'wb') as stream:
                if have and response.status != 206:
                    stream.seek(0)
                    stream.truncate()
                while True:
                    chunk = response.read(1 << 20)
                    if not chunk:
                        break
                    stream.write(chunk)
                    pct = int(100 * stream.tell() / src['size'])
                    if pct >= last + 10:
                        last = pct - pct % 10
                        print(' %d%%' % last, end='', flush=True)
            print()
        except (urllib.error.URLError, OSError, TimeoutError) as e:
            print()
            log('  download problem (%s), retry %d' % (e, attempt))
            time.sleep(5 * attempt)
    if not part.exists() or part.stat().st_size != src['size']:
        raise SystemExit('Download incomplete: %s. Retry to resume the .part file in %s.' % (url, part.parent))
    part.replace(target)
    got = sha256(target)
    if got != src['sha256']:
        target.unlink()
        raise SystemExit('SHA-256 mismatch for %s: got %s, expected %s. The archive was removed; retry the download.' %
                         (src['filename'], got, src['sha256']))
    log('  sha256 ok:', src['filename'])


def mdf_to_iso(mdf, iso):
    """Strip the raw CD sector framing (MDF/BIN, 2352 or 2448 bytes per sector) to a plain 2048-byte-sector ISO."""
    with open(mdf, 'rb') as stream:
        for sector_size in (2448, 2352):
            for data_offset in (16, 24):            # mode 1 / mode 2 form 1 user data
                stream.seek(16 * sector_size + data_offset)
                if stream.read(6)[1:] == b'CD001':
                    break
            else:
                continue
            break
        else:
            raise SystemExit('Unrecognised CD image: %s. Check that the archive matches toolchain_manifest.json.' % mdf)
        stream.seek(0)
        with open(iso, 'wb') as out:
            while True:
                buf = stream.read(sector_size * 2048)
                if not buf:
                    break
                for i in range(0, len(buf) - sector_size + 1, sector_size):
                    out.write(buf[i + data_offset:i + data_offset + 2048])


def extract_list(sevenzip, archive, names, outdir):
    """Extract the selected archive members using a UTF-8 7-Zip list file."""
    lst = outdir.parent / (outdir.name + '.list')
    lst.write_text('\n'.join(names) + '\n', encoding='utf-8')
    run7z(sevenzip, ['x', '-y', '-scsUTF-8', '-o' + str(outdir), str(archive), '@' + str(lst)], outdir.parent)
    lst.unlink()


def fetch_source(name, src, files, sevenzip, work, keep):
    """Download a source disc and extract the requested files or SDK cabinets."""
    log('[%s] %s  (%.0f MB)' % (name, src['title'], src['size'] / 1e6))
    arc = work / src['filename']
    download(src, arc)
    img = work / (name + '_img')
    img.mkdir(parents=True, exist_ok=True)
    run7z(sevenzip, ['x', '-y', '-r', '-o' + str(img), str(arc), src['image']], work)
    mdfs = [p for p in img.rglob('*') if p.is_file() and p.suffix.lower() in ('.mdf', '.bin', '.iso')]
    if len(mdfs) != 1:
        raise SystemExit('Expected exactly one disc image in %s. Check the archive against toolchain_manifest.json.' % src['filename'])
    iso = work / (name + '.iso')
    if mdfs[0].suffix.lower() == '.iso':
        iso = mdfs[0]
    else:
        mdf_to_iso(mdfs[0], iso)
    if iso != mdfs[0]:
        shutil.rmtree(img, ignore_errors=True)
    if not keep:
        remove(arc)
    out = work / (name + '_out')
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    if name == 'psdk':
        cabs = sorted({f['cab'] for f in files})
        extract_list(sevenzip, iso, cabs, out)
        for cab in cabs:
            members = [f['member'] for f in files if f['cab'] == cab]
            cdir = out / ('m_' + Path(cab).stem)
            cdir.mkdir()
            extract_list(sevenzip, out / cab, members, cdir)
    else:
        extract_list(sevenzip, iso, sorted({f['path'] for f in files}), out)
    return iso, out


def install(files, name, out, dest):
    """Verify each extracted file before copying it beneath the destination."""
    for entry in files:
        if name == 'psdk':
            source = out / ('m_' + Path(entry['cab']).stem) / entry['member']
        else:
            source = out / Path(*entry['path'].split('/'))
        if not source.is_file():
            raise SystemExit('Missing in image: %s. Check the extraction output and retry.' % (entry.get('member') or entry['path']))
        got = sha256(source)
        if got != entry['sha256']:
            raise SystemExit('SHA-256 mismatch for %s (from %s): got %s, expected %s. Check the archive and retry extraction.' %
                             (entry['dest'], name, got, entry['sha256']))
        target = (dest / entry['dest']).resolve()
        if dest.resolve() not in target.parents:
            raise SystemExit('Refusing to write outside the toolchain folder: %s. Correct the destination in toolchain_manifest.json.' % entry['dest'])
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)


def verify(files, dest):
    """Return manifest paths that are missing or have the wrong size or hash."""
    bad = []
    for entry in files:
        path = dest / entry['dest']
        if not path.is_file() or path.stat().st_size != entry['size'] or sha256(path) != entry['sha256']:
            bad.append(entry['dest'])
    return bad


def main(argv=None):
    """Verify an installation, fetching only files that are missing or differ."""
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--dest', metavar='DIR', default=str(DEFAULT_DEST), help='installation parent; contains vc7/ (default: build/toolchain)')
    ap.add_argument('--7z', dest='sevenzip', metavar='FILE', help='7-Zip executable (default: %%SEVENZIP%%, PATH, then Program Files/7-Zip)')
    ap.add_argument('--keep', action='store_true', help='keep downloaded .7z archives (default: delete them)')
    ap.add_argument('--verify-only', action='store_true', help='only check the files already in --dest')
    ap.add_argument('--work', metavar='DIR', help='temporary download/extraction directory, removed unless --keep (default: <dest>/_download)')
    args = ap.parse_args(argv)

    man = json.loads(MANIFEST.read_text(encoding='utf-8'))
    dest = Path(args.dest).resolve()
    files = man['files']
    bad = verify(files, dest)
    if not bad:
        log('toolchain in %s is complete: %d files verified' % (dest, len(files)))
        return 0
    if args.verify_only:
        log('%d of %d files missing or differing, e.g. %s' % (len(bad), len(files), bad[:3]))
        return 1

    sevenzip = find_7z(args.sevenzip)
    work = Path(args.work).resolve() if args.work else dest / '_download'
    work.mkdir(parents=True, exist_ok=True)
    free = shutil.disk_usage(work).free
    log('7-Zip: %s; free disk space: %.1f GB (about 2.5 GB needed temporarily)' % (sevenzip, free / 1e9))
    if free < 3e9:
        raise SystemExit('Not enough free disk space on %s. Free at least 3 GB or pass --work <directory on another drive>.' % work)
    todo = sorted({f['source'] for f in files if f['dest'] in set(bad)})
    for name in todo:
        sel = [f for f in files if f['source'] == name and f['dest'] in set(bad)]
        iso, out = fetch_source(name, man['sources'][name], sel, sevenzip, work, args.keep)
        install(sel, name, out, dest)
        shutil.rmtree(out, ignore_errors=True)
        if iso.exists() and work in iso.parents:
            remove(iso)
        log('  installed %d files from %s' % (len(sel), name))
    if not args.keep:
        shutil.rmtree(work, ignore_errors=True)
    bad = verify(files, dest)
    if bad:
        raise SystemExit('Verification failed: %s. Retry the fetch to replace missing or differing files.' % bad[:5])
    log('toolchain ready: %s (%d files, all SHA-256 verified)' % (dest / 'vc7', len(files)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
