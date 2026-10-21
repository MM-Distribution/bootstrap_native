#!/usr/bin/env python3
"""The Bootstrap updates page in a browser (run by tests/update_test.sh).

  update_ui.py OUT_DIR      (env: BASE, LOGIN = a one-time login link)

With the test releases in place: checks 6.0.1-test.1 through the page (the
batch), previews it, applies it from the preview bar, rolls back; checks
6.0.2-test.1 and tries to apply it without and with "I understand"; then
looks at every page at phone width. Fails on JavaScript errors, failed
requests and sideways scrolling.
"""
import os
import sys

from playwright.sync_api import sync_playwright

out, base = sys.argv[1], os.environ['BASE'].rstrip('/')
page_url = base + '/admin/appearance/bootstrap-updates'
failures = []
problems = []


def ok(condition, label):
    print(('ok    ' if condition else 'FAIL  ') + label)
    if not condition:
        failures.append(label)


def check_release(page, version):
    page.goto(page_url)
    page.select_option('#bnu-older select', version)
    page.click('#bnu-older input[type=submit]')
    page.wait_for_url('**/result/' + version, timeout=120000)


with sync_playwright() as p:
    browser = p.chromium.launch()
    context = browser.new_context(viewport={'width': 1280, 'height': 900})
    page = context.new_page()
    page.on('pageerror', lambda e: problems.append('js: %s' % e))
    page.on('console', lambda m: m.type == 'error' and problems.append('console: %s' % m.text))
    page.on('response', lambda r: r.status >= 400 and problems.append('%d %s' % (r.status, r.url)))
    page.goto(os.environ['LOGIN'])

    page.goto(page_url)
    ok(page.get_by_text('Found: Dart Sass').count() == 1, 'overview: compiler found')
    ok(page.locator('.bnu-release').get_by_text('6.0.5-test.1').count() == 1, 'overview: newest release offered')
    ok('6.0.0-alpha.1 · active' in page.inner_text('#bnu-older select'), 'overview: the active release is marked in Older versions')
    page.screenshot(path=out + '/1-overview.png', full_page=True)

    # Check through the page: the batch, then the results.
    page.goto(page_url)
    page.select_option('#bnu-older select', '6.0.1-test.1')
    page.click('#bnu-older input[type=submit]')
    page.wait_for_selector('.bnu-steps', timeout=30000)
    page.screenshot(path=out + '/2-checking.png')
    page.wait_for_url('**/result/6.0.1-test.1', timeout=120000)
    ok(page.get_by_text('Passed every check.').count() == 1, 'check 6.0.1-test.1: passed')
    page.screenshot(path=out + '/3-passed.png', full_page=True)

    # Preview: only this admin sees it.
    page.click('input[value="Preview on the site"]')
    page.wait_for_load_state()
    ok(page.locator('.bnu-preview').count() == 1, 'preview: bar on the site')
    ok('builds/6.0.1-test.1~default/bootstrap_native.css' in page.content(), 'preview: the build\'s stylesheet')
    page.wait_for_function('window.bootstrap !== undefined', timeout=5000)
    ok(page.evaluate('typeof window.bootstrap.Drawer') == 'function', 'preview: the build\'s script loads')
    page.screenshot(path=out + '/5-preview.png')
    visitor = browser.new_page()
    visitor.goto(base + '/user/login')
    ok('builds/6.0.1-test.1' not in visitor.content() and visitor.locator('.bnu-preview').count() == 0, 'preview: visitors still get the shipped release')

    # Apply from the bar, then roll back.
    page.click('.bnu-preview input[value="Apply"]')
    page.wait_for_url(page_url)
    ok(page.get_by_text("Bootstrap 6.0.1-test.1 (the theme's parts) is now active.").count() == 1, 'apply: message')
    ok(page.locator('tr', has_text='6.0.1-test.1').get_by_text('Active').count() == 1, 'apply: table shows it active')
    page.screenshot(path=out + '/6-applied.png', full_page=True)
    visitor.goto(base + '/user/login')
    ok('builds/6.0.1-test.1~default/bootstrap_native.css' in visitor.content(), 'apply: visitors get it')
    page.locator('tr', has_text='Shipped with the theme').get_by_text('Roll back to this version').click()
    page.wait_for_url(page_url)
    ok(page.get_by_text("Rolled back to Bootstrap 6.0.0-alpha.1 (the theme's parts).").count() == 1, 'roll back: message')
    visitor.goto(base + '/user/login')
    ok('bootstrap-native/builds/' not in visitor.content(), 'roll back: visitors get the shipped release')
    ok(page.locator('tr', has_text='6.0.1-test.1').get_by_text('Roll back to this version').count() == 1, 'roll back: the applied release can be rolled back to')

    # A release that breaks things: locked until "I understand".
    check_release(page, '6.0.2-test.1')
    ok(page.get_by_text('Updating will break parts of the theme.').count() == 1, 'check 6.0.2-test.1: breaks')
    apply = page.locator('input[value="Apply anyway"]')
    ok(apply.is_disabled(), 'breaks: Apply anyway locked')
    page.check('input[name="understood"]')
    ok(apply.is_enabled(), 'breaks: unlocked by I understand')
    page.screenshot(path=out + '/4-breaks.png', full_page=True)
    page.uncheck('input[name="understood"]')
    # Posting without the box (as a forged request would) still refuses.
    page.evaluate('document.querySelector(\'input[value="Apply anyway"]\').disabled = false')
    apply.click()
    page.wait_for_load_state()
    ok(page.get_by_text('Tick "I understand"').count() == 1, 'breaks: the server refuses without I understand')
    page.check('input[name="understood"]')
    page.click('input[value="Apply anyway"]')
    page.wait_for_url(page_url)
    ok(page.get_by_text("Bootstrap 6.0.2-test.1 (the theme's parts) is now active.").count() == 1, 'breaks: applied once confirmed')
    page.locator('tr', has_text='Shipped with the theme').get_by_text('Roll back to this version').click()
    page.wait_for_url(page_url)

    # Components: measured on the first visit, then chosen, built, checked.
    page.goto(base + '/admin/appearance/bootstrap-components')
    page.wait_for_url('**/admin/appearance/bootstrap-components', timeout=180000)
    page.wait_for_selector('.bnu-parts', timeout=30000)
    otp = page.locator('tr', has_text='OTP input')
    ok(otp.locator('a.bnu-docs').get_attribute('href') == 'https://getbootstrap.com/docs/6.0/forms/otp-input/', 'components: docs link for OTP input')
    ok(otp.locator('a.bnu-docs').get_attribute('title') == 'Docs and examples', 'components: docs icon has its tooltip')
    ok(page.locator('tr', has_text='Datepicker').locator('a.bnu-docs').get_attribute('href').endswith('/forms/datepicker/'), 'components: datepicker docs found under forms')
    ok(page.locator('tr', has_text='Alert').first.locator('input[type=checkbox]').is_disabled(), 'components: a required part is locked')
    placeholder = page.locator('tr', has_text='Placeholder').locator('input[type=checkbox]')
    ok(placeholder.is_disabled() and not placeholder.is_checked(), 'components: a clashing part is locked off')
    accordion = page.locator('tr', has_text='Accordion').locator('td.bnu-size')
    ok('KB' in accordion.inner_text(), 'components: sizes measured (%s)' % accordion.inner_text())
    ok(page.get_by_text('Measure again').count() == 1, 'components: Measure again link')
    before = page.inner_text('.bnu-size-new')
    page.locator('tr', has_text='Accordion').locator('input[type=checkbox]').uncheck()
    page.locator('tr.bnu-group ~ tr', has_text='Margin').first.locator('input[type=checkbox]').uncheck()
    ok(page.inner_text('.bnu-size-new') != before, 'components: the size estimate follows the boxes (%s -> %s KB)' % (before, page.inner_text('.bnu-size-new')))
    page.screenshot(path=out + '/10-components.png', full_page=True)
    page.click('input[value="Build and check"]')
    page.wait_for_url('**/result/6.0.0-alpha.1~*', timeout=120000)
    ok('with chosen parts' in page.inner_text('h1'), 'components: results for the chosen parts')
    if page.locator('input[name="understood"]').count():
        page.check('input[name="understood"]')
        page.click('input[value="Apply anyway"]')
    else:
        page.click('input[value="Apply update"]')
    page.wait_for_url(page_url)
    ok(page.get_by_text('(chosen parts) is now active.').count() == 1, 'components: applied')
    visitor.goto(base + '/user/login')
    ok('builds/6.0.0-alpha.1~' in visitor.content(), 'components: visitors get the chosen parts')
    with page.expect_download() as download:
        page.goto(base + '/admin/appearance/bootstrap-components')
        page.click('a.bnu-download')
    ok(download.value.suggested_filename.startswith('bootstrap-native-6.0.0-alpha.1-') and download.value.suggested_filename.endswith('.tar.gz'), 'components: download the built files (%s)' % download.value.suggested_filename)
    page.goto(page_url)
    page.locator('tr', has_text='Shipped with the theme').get_by_text('Roll back to this version').click()
    page.wait_for_url(page_url)

    # Sub-theme form (board 9).
    page.goto(base + '/admin/appearance')
    page.click('text=Create a Bootstrap Native sub-theme')
    page.fill('#edit-name', 'Bnu UI Sub')
    page.wait_for_function('document.querySelector("#edit-machine").value === "bnu_ui_sub"', timeout=5000)
    page.screenshot(path=out + '/9-subtheme.png', full_page=True)
    with page.expect_download() as download:
        page.click('input[value="Download as .tar.gz instead"]')
    ok(download.value.suggested_filename == 'bnu_ui_sub.tar.gz', 'sub-theme: download as .tar.gz')
    page.goto(base + '/admin/appearance/bootstrap-native-subtheme')
    page.fill('#edit-name', 'Bnu UI Sub')
    page.wait_for_function('document.querySelector("#edit-machine").value === "bnu_ui_sub"', timeout=5000)
    page.click('input[value="Create sub-theme"]')
    page.wait_for_url('**/admin/appearance')
    ok(page.get_by_text('Created sites/all/themes/bnu_ui_sub and enabled it.').count() == 1, 'sub-theme: created and enabled')
    page.goto(base + '/admin/appearance/bootstrap-native-subtheme')
    page.fill('#edit-name', 'Bnu UI Sub')
    page.wait_for_function('document.querySelector("#edit-machine").value === "bnu_ui_sub"', timeout=5000)
    page.click('input[value="Create sub-theme"]')
    page.wait_for_load_state()
    ok(page.locator('.messages.error').count() == 1, 'sub-theme: refuses a name that is taken')

    # Tokens: an action link without its token does nothing.
    seen = len(problems)
    response = page.goto(page_url + '/action/discard/6.0.1-test.1')
    ok(response.status == 403, 'actions need their token (%d)' % response.status)
    del problems[seen:]

    # Phone width.
    page.set_viewport_size({'width': 390, 'height': 844})
    for path, name in [('/admin/appearance/bootstrap-updates', 'overview'), ('/admin/appearance/bootstrap-updates/result/6.0.1-test.1', 'passed'), ('/admin/appearance/bootstrap-updates/result/6.0.2-test.1', 'breaks'), ('/admin/appearance/bootstrap-components', 'components'), ('/admin/appearance/bootstrap-native-subtheme', 'subtheme')]:
        page.goto(base + path)
        width = page.evaluate('document.documentElement.scrollWidth')
        ok(width <= 390, 'phone: %s fits (%d px)' % (name, width))
        page.screenshot(path='%s/phone-%s.png' % (out, name), full_page=True)
    browser.close()

ok(not problems, 'no JavaScript errors or failed requests' + (': ' + '; '.join(problems[:5]) if problems else ''))
print('all passed' if not failures else '%d failed' % len(failures))
sys.exit(1 if failures else 0)
