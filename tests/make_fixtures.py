#!/usr/bin/env python3
"""Test releases of Bootstrap for the update checks, and a registry for them.

  make_fixtures.py VENDOR_DIR OUT_DIR BASE_URL

Writes OUT_DIR/registry.json (an npm registry document) and one .tgz per
release, made from the shipped Bootstrap in VENDOR_DIR (the theme's
vendor/bootstrap). BASE_URL is where OUT_DIR is served (the tarballs must
come from the registry's own host).

  6.0.1-test.1  the shipped release under a new number: passes
  6.0.2-test.1  .navbar-toggler-icon renamed, --link-color no longer read: breaks
  6.0.3-test.1  scss/_badge.scss removed: doesn't build
  6.0.4-test.1  a file outside package/: refused when unpacking
  6.0.5-test.1  a checksum that doesn't match: refused before unpacking
"""
import base64
import hashlib
import io
import json
import pathlib
import sys
import tarfile

vendor, out, base = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3].rstrip('/')
out.mkdir(parents=True, exist_ok=True)


def files():
    """The shipped release as {path in package: bytes}."""
    result = {}
    for path in sorted((vendor / 'scss').rglob('*')):
        if path.is_file():
            result['scss/' + str(path.relative_to(vendor / 'scss'))] = path.read_bytes()
    result['dist/js/bootstrap.bundle.min.js'] = (vendor / 'dist/js/bootstrap.bundle.min.js').read_bytes()
    result['LICENSE'] = (vendor / 'LICENSE').read_bytes()
    return result


def tgz(version, content, extra=None):
    content = dict(content)
    content['package.json'] = json.dumps({'name': 'bootstrap', 'version': version}).encode()
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode='w:gz') as tar:
        for name, data in sorted(content.items()):
            info = tarfile.TarInfo('package/' + name)
            info.size = len(data)
            info.mode = 0o644
            tar.addfile(info, io.BytesIO(data))
        for name, data in (extra or {}).items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
    return buf.getvalue()


def replace(content, path, old, new, count_min=1):
    text = content[path].decode()
    assert text.count(old) >= count_min, (path, old)
    content[path] = text.replace(old, new).encode()


shipped = files()
releases = {}

releases['6.0.1-test.1'] = tgz('6.0.1-test.1', shipped)

breaks = dict(shipped)
replace(breaks, 'scss/_navbar.scss', '.navbar-toggler-icon {', '.navbar-toggler-glyph {')
for path in list(breaks):
    if path.endswith('.scss') and b'var(--link-color' in breaks[path]:
        replace(breaks, path, 'var(--link-color', 'var(--link-fg')
releases['6.0.2-test.1'] = tgz('6.0.2-test.1', breaks)

failed = dict(shipped)
del failed['scss/_badge.scss']
releases['6.0.3-test.1'] = tgz('6.0.3-test.1', failed)

releases['6.0.4-test.1'] = tgz('6.0.4-test.1', shipped, {'package/../../escaped.txt': b'outside'})

releases['6.0.5-test.1'] = tgz('6.0.5-test.1', shipped)

doc = {'name': 'bootstrap', 'dist-tags': {'next': '6.0.5-test.1'}, 'versions': {}, 'time': {}}
for day, (version, data) in enumerate(sorted(releases.items()), start=10):
    (out / ('bootstrap-%s.tgz' % version)).write_bytes(data)
    digest = data if version != '6.0.5-test.1' else data + b'tampered'
    integrity = 'sha512-' + base64.b64encode(hashlib.sha512(digest).digest()).decode()
    doc['versions'][version] = {'dist': {'tarball': '%s/bootstrap-%s.tgz' % (base, version), 'integrity': integrity}}
    doc['time'][version] = '2026-10-%02dT12:00:00.000Z' % day
# The shipped release too (it is active: the page lists it, can't check it).
doc['versions']['6.0.0-alpha.1'] = {'dist': {'tarball': base + '/bootstrap-6.0.1-test.1.tgz', 'integrity': 'sha512-AAAA'}}
doc['time']['6.0.0-alpha.1'] = '2026-10-08T16:47:50.250Z'
(out / 'registry.json').write_text(json.dumps(doc, indent=1))
print('fixtures:', ', '.join(sorted(releases)))
