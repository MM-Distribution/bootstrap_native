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

    # Theme settings: the color preview's links and buttons are only for
    # show (a click used to load the whole page inside the preview).
    page.goto(base + '/admin/appearance/settings/' + os.environ.get('THEME', 'bootstrap_native'))
    preview = page.frame_locator('#bootstrap-native-preview iframe')
    for text in ('Edit', 'a link', 'Home', 'About'):
        preview.get_by_text(text, exact=True).first.click()
    page.wait_for_timeout(300)
    ok(all(f.url in ('about:srcdoc', page.url) for f in page.frames), 'settings: clicks in the preview stay in the preview')
    ok(page.locator('details.bn-details').count() == 6, "settings: six steps (the theme's five, then Bootstrap components)")
    ok(page.locator('#bn-step-header input[name="toggle_main_menu"]').count() == 1 and page.locator('#bn-step-icon input[name="default_favicon"]').count() == 1 and page.locator('#bn-step-brand input[name="files[logo_upload]"]').count() == 1, "settings: core's header, icon and logo settings are in their steps")
    page.click('.bn-view[data-view="messages"]')
    page.wait_for_timeout(200)
    ok(page.frames[1].evaluate("document.documentElement.getAttribute('data-preview')") == 'messages', 'settings: the sample switch shows the messages sample')
    secondary = page.locator('input[name="bootstrap_native_color_secondary"]')
    if page.locator('.bn-color[data-key="secondary"]').get_attribute('data-mode') == 'auto':
        page.locator('.bn-color[data-key="secondary"] .bn-mode').click()
    secondary.fill('indigo-600')
    secondary.dispatch_event('change')
    page.wait_for_function("document.querySelector('#bn-note-secondary').textContent.includes('#5d31cb')", timeout=10000)
    ok(True, 'settings: a Bootstrap color name resolves (indigo-600 = #5d31cb)')
    try:
        page.wait_for_function("(() => { const f = document.querySelector('#bootstrap-native-preview iframe'); return f && f.contentDocument && f.contentDocument.documentElement.getAttribute('data-preview') === 'messages' && f.contentDocument.documentElement.outerHTML.includes('#5d31cb'); })()", timeout=10000)
        kept = True
    except Exception:
        kept = False
    ok(kept, 'settings: the updated preview keeps its sample and uses the named color')
    ok(page.inner_text('.bn-unsaved') == 'Unsaved changes', 'settings: unsaved changes shown')
    page.locator('.bn-color[data-key="base"] .bn-mode').click()
    ok(page.input_value('input[name="bootstrap_native_color_base"]') == '#ffffff', "settings: Set starts from what the page uses (white)")
    page.locator('.bn-color[data-key="base"] .bn-mode').click()
    ok(page.input_value('input[name="bootstrap_native_color_base"]') == '', 'settings: Automatic empties it again')
    page.locator('input[name="bootstrap_native_color_text"]').evaluate("e => { e.closest('.bn-color').classList.add('is-editing'); e.closest('.bn-color').setAttribute('data-mode', 'set'); }")
    page.fill('input[name="bootstrap_native_color_text"]', 'nope')
    page.locator('input[name="bootstrap_native_color_text"]').dispatch_event('change')
    page.wait_for_function("document.querySelector('#bn-note-text').classList.contains('is-error')", timeout=10000)
    ok(True, 'settings: not a color: the note says how to write one')
    page.click('input[value="Save configuration"]')
    page.wait_for_load_state()
    ok(page.locator('.messages.error').count() == 1 and 'Text' in page.inner_text('.messages.error'), 'settings: saving refuses a value that is not a color')
    page.fill('input[name="bootstrap_native_color_text"]', '')
    page.click('input[value="Save configuration"]')
    page.wait_for_load_state()
    ok(page.locator('.messages.status').count() >= 1, 'settings: saved')
    ok(page.input_value('input[name="bootstrap_native_color_secondary"]') == 'indigo-600', 'settings: the name is kept as typed')
    visitor.goto(base + '/user/login')
    ok('--bs-accent-base:#5d31cb' in visitor.content(), 'settings: pages use the hex the name works out to')
    # Typing a color and pressing Save at once: the field's preview update
    # disables it for a moment; it must still be sent.
    page.fill('input[name="bootstrap_native_color_primary"]', '#123456')
    page.click('input[value="Save configuration"]')
    page.wait_for_load_state()
    ok(page.input_value('input[name="bootstrap_native_color_primary"]') == '#123456', 'settings: a color typed just before Save is saved')
    colors = ['primary', 'secondary', 'base', 'text', 'link', 'link_hover']
    def color(key):
        return page.input_value('input[name="bootstrap_native_color_%s"]' % key)
    def shown(sel):
        return page.locator(sel).first.is_visible()
    # Discard changes: only with unsaved changes; puts every field back.
    ok(not shown('.bn-discard'), 'settings: no Discard changes while nothing changed')
    slogan = page.locator('input[name="toggle_slogan"]')
    slogan_was = slogan.is_checked()
    page.fill('input[name="bootstrap_native_color_primary"]', '#654321')
    page.locator('input[name="bootstrap_native_color_primary"]').dispatch_event('change')
    slogan.click(force=True)
    page.wait_for_timeout(100)
    ok(shown('.bn-discard') and page.inner_text('.bn-unsaved') == 'Unsaved changes', 'settings: changes show Discard changes')
    page.click('.bn-discard')
    page.wait_for_timeout(100)
    ok(color('primary') == '#123456' and slogan.is_checked() == slogan_was, 'settings: Discard changes puts the fields back')
    ok(not shown('.bn-discard') and page.inner_text('.bn-unsaved') == 'All changes saved', 'settings: and nothing is unsaved after it')
    # Reset: empties the six colors in the page, no question asked; Undo
    # puts them back; only Save keeps it.
    dialogs = []
    def on_dialog(d):
        dialogs.append(d.type)
        d.dismiss()
    page.on('dialog', on_dialog)
    page.click('input.bn-reset')
    page.wait_for_timeout(100)
    ok(all(color(k) == '' for k in colors) and not dialogs, 'settings: Reset empties the six colors without asking')
    ok("Colors cleared to Bootstrap's own, not saved yet." in page.inner_text('.bn-reset-status') and shown('.bn-undo') and not shown('input.bn-reset'), 'settings: Reset says nothing is saved yet, with Undo')
    try:
        page.wait_for_function("(() => { const f = document.querySelector('#bootstrap-native-preview iframe'); return f && f.contentDocument && f.contentDocument.documentElement && !f.contentDocument.documentElement.outerHTML.includes('#5d31cb'); })()", timeout=10000)
        cleared = True
    except Exception:
        cleared = False
    ok(cleared, "settings: the preview shows Bootstrap's colors after Reset")
    page.click('.bn-undo')
    page.wait_for_timeout(100)
    ok(color('primary') == '#123456' and color('secondary') == 'indigo-600' and shown('input.bn-reset') and not shown('.bn-undo') and page.inner_text('.bn-unsaved') == 'All changes saved', 'settings: Undo puts the colors back')
    page.click('input.bn-reset')
    page.wait_for_timeout(100)
    ok(not shown('.bn-discard') and not shown('.bn-unsaved'), 'settings: after Reset the bar shows only Save, what Reset did, and Undo')
    page.click('input[value="Save configuration"]')
    page.wait_for_load_state()
    page.remove_listener('dialog', on_dialog)
    ok(color('primary') == '' and color('secondary') == '' and not dialogs, 'settings: Reset, then Save, clears the colors')
    # Without JavaScript, Reset shows the form again with the colors empty;
    # nothing is saved.
    page.locator('.bn-color[data-key="primary"] .bn-mode').click()
    page.fill('input[name="bootstrap_native_color_primary"]', '#123456')
    page.click('input[value="Save configuration"]')
    page.wait_for_load_state()
    nojs = browser.new_context(java_script_enabled=False, storage_state=context.storage_state())
    plain = nojs.new_page()
    plain.goto(base + '/admin/appearance/settings/' + os.environ.get('THEME', 'bootstrap_native'))
    plain.click('input.bn-reset')
    plain.wait_for_load_state()
    ok(plain.input_value('input[name="bootstrap_native_color_primary"]') == '' and plain.get_by_text("Colors cleared to Bootstrap's own, not saved yet.", exact=False).count() >= 1, 'settings: without JavaScript, Reset empties the colors in the form')
    plain.goto(base + '/admin/appearance/settings/' + os.environ.get('THEME', 'bootstrap_native'))
    ok(plain.input_value('input[name="bootstrap_native_color_primary"]') == '#123456', 'settings: ...and saves nothing')
    nojs.close()
    # SVG logos: cleaned on upload; one that can't be read safely is refused.
    svg_dir = os.path.join(out, 'svg')
    os.makedirs(svg_dir, exist_ok=True)
    with open(os.path.join(svg_dir, 'bnu-test-logo.svg'), 'w') as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 60" onload="alert(1)"><script>alert(2)</script><rect width="200" height="60" rx="8" fill="#1d3557"/><text x="100" y="40" fill="#f19c21" font-size="28" text-anchor="middle">LOGO</text></svg>')
    with open(os.path.join(svg_dir, 'bnu-test-bad.svg'), 'w') as f:
        f.write('<?xml version="1.0"?><!DOCTYPE svg [<!ENTITY x SYSTEM "file:///etc/passwd">]><svg xmlns="http://www.w3.org/2000/svg">&x;</svg>')
    page.set_input_files('input[name="files[logo_upload]"]', os.path.join(svg_dir, 'bnu-test-bad.svg'))
    page.click('input[value="Save configuration"]')
    page.wait_for_load_state()
    ok(page.locator('.messages.error').count() == 1 and 'can be read safely' in page.inner_text('.messages.error'), 'settings: an SVG with entities is refused')
    page.set_input_files('input[name="files[logo_upload]"]', os.path.join(svg_dir, 'bnu-test-logo.svg'))
    page.click('input[value="Save configuration"]')
    page.wait_for_load_state()
    ok(page.locator('.messages.error').count() == 0 and page.locator('.bn-logo-current img[src*="bnu-test-logo"]').count() == 1, 'settings: an SVG logo is taken')
    logo_url = page.locator('.bn-logo-current img').get_attribute('src')
    body = page.request.get(logo_url).text()
    ok('<rect' in body and 'script' not in body and 'onload' not in body and 'alert' not in body, 'settings: the saved SVG has its drawing, no script or onload')
    visitor.goto(base + '/user/login')
    box = visitor.locator('.navbar-brand img').bounding_box()
    ok(box is not None and box['height'] > 20 and box['width'] > 60, 'settings: the SVG logo shows in the header (%s)' % (box and '%dx%d' % (box['width'], box['height'])))
    visitor.screenshot(path=out + '/12-svg-logo.png')
    # Step 6: the theme's own Bootstrap parts, built and checked on Save,
    # used once applied; then back to the site-wide choice.
    theme = os.environ.get('THEME', 'bootstrap_native')
    page.goto(base + '/admin/appearance/settings/' + theme)
    ok(page.locator('#bn-step-components input[name="bootstrap_native_parts_mode"][value="site"]').is_checked(), 'theme parts: starts on the site-wide choice')
    ok(not page.locator('input[name="bootstrap_native_parts_own[accordion]"]').is_visible(), 'theme parts: the parts list shows only for its own')
    page.check('input[name="bootstrap_native_parts_mode"][value="own"]')
    page.uncheck('input[name="bootstrap_native_parts_own[accordion]"]')
    page.locator('#bn-step-components').screenshot(path=out + '/13-theme-parts.png')
    page.click('input[value="Save configuration"]')
    page.wait_for_url('**/bootstrap-updates/result/**', timeout=120000)
    ok('theme=' + theme in page.url and 'with its own parts' in page.inner_text('h1'), 'theme parts: Save builds and checks them, results for this theme (%s)' % page.inner_text('h1'))
    page.screenshot(path=out + '/14-theme-parts-results.png', full_page=True)
    page.click('input[value="Apply to this theme"]')
    page.wait_for_load_state()
    ok(page.get_by_text('now uses its own parts').count() == 1, 'theme parts: applied to this theme')
    page.goto(base + '/admin/appearance/bootstrap-components')
    ok(page.locator('tr', has_text='(default theme)').get_by_text('Its own').count() == 1, 'components: the theme list shows its own parts')
    page.screenshot(path=out + '/12-components-themes.png')
    visitor.goto(base + '/user/login')
    ok('/bootstrap-native/builds/' in visitor.content(), "theme parts: visitors get the theme's own build")
    page.goto(base + '/admin/appearance/settings/' + theme)
    page.check('input[name="bootstrap_native_parts_mode"][value="site"]')
    page.click('input[value="Save configuration"]')
    page.wait_for_load_state()
    ok(page.get_by_text('uses the site-wide choice of Bootstrap parts again').count() == 1, 'theme parts: back to the site-wide choice')
    page.set_viewport_size({'width': 390, 'height': 844})
    page.goto(base + '/admin/appearance/settings/' + os.environ.get('THEME', 'bootstrap_native'))
    width = page.evaluate('document.documentElement.scrollWidth')
    ok(width <= 390, 'settings: phone: fits (%d px)' % width)
    ok(page.locator('details.bn-details[open]').count() == 1, 'settings: phone: only the first step open')
    page.click('.bn-preview-open')
    ok(page.locator('.bn-preview-pane.is-open').count() == 1, 'settings: phone: Preview opens over the page')
    page.screenshot(path=out + '/11-settings-phone-preview.png')
    page.keyboard.press('Escape')
    ok(page.locator('.bn-preview-pane.is-open').count() == 0, 'settings: phone: Escape closes it')
    page.set_viewport_size({'width': 1280, 'height': 900})

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
