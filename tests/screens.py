#!/usr/bin/env python3
"""Screenshots of theme pages at desktop and phone widths, plus checks.

  screens.py OUTDIR PATH [PATH...]

Env: BASE (site URL), LOGIN (optional one-time login URL, e.g. from
`drush uli --no-browser`; never printed), WIDTHS (default "1280,390").

Fails (exit 1) on JavaScript errors, failed or 4xx/5xx requests, or
pages that scroll sideways at phone width.
"""
import os
import sys
from playwright.sync_api import sync_playwright

base = os.environ['BASE'].rstrip('/')
out = sys.argv[1]
paths = sys.argv[2:]
widths = [int(w) for w in os.environ.get('WIDTHS', '1280,390').split(',')]
os.makedirs(out, exist_ok=True)
problems = []

with sync_playwright() as p:
    browser = p.chromium.launch()
    state = None
    if os.environ.get('LOGIN'):
        # A one-time link works once: sign in once, share the session.
        ctx = browser.new_context()
        ctx.new_page().goto(os.environ['LOGIN'])
        state = ctx.storage_state()
        ctx.close()
    for width in widths:
        ctx = browser.new_context(viewport={'width': width, 'height': 900}, device_scale_factor=1, storage_state=state)
        page = ctx.new_page()
        errors = []
        page.on('pageerror', lambda e: errors.append('pageerror: %s' % e))
        # Failed loads are reported with their URL below, not as console text.
        page.on('console', lambda m: m.type == 'error' and 'Failed to load resource' not in m.text and errors.append('console: %s' % m.text))
        page.on('requestfailed', lambda r: errors.append('failed: %s' % r.url.split('?')[0]))
        page.on('response', lambda r: r.status >= 400 and errors.append('%d: %s' % (r.status, r.url.split('?')[0])))
        for path in paths:
            del errors[:]
            page.goto(base + path, wait_until='networkidle')
            name = (path.strip('/').replace('/', '_') or 'front') + '-%d.png' % width
            page.screenshot(path=os.path.join(out, name), full_page=True)
            sideways = page.evaluate('document.documentElement.scrollWidth - document.documentElement.clientWidth')
            if width < 600 and sideways > 0:
                errors.append('scrolls sideways by %dpx' % sideways)
            # Module scripts run after parsing; give Bootstrap a moment.
            bootstrap = page.evaluate('''() => window.bootstrap ? 'object' : new Promise(done => {
                document.addEventListener('bootstrap-native:ready', () => done('object'));
                setTimeout(() => done(typeof window.bootstrap), 3000);
            })''')
            if bootstrap != 'object':
                errors.append('window.bootstrap is %s' % bootstrap)
            status = 'ok' if not errors else '; '.join(errors)
            print('%-40s %4d  %s' % (path, width, status))
            if errors:
                problems.append(path)
        ctx.close()
    browser.close()
sys.exit(1 if problems else 0)
