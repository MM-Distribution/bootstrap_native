<?php

/**
 * @file
 * Tests of includes/build.inc and includes/checks.inc (plain PHP:
 * php tests/checks_test.php).
 */

$module = dirname(__DIR__);
require $module . '/includes/build.inc';
require $module . '/includes/checks.inc';
require $module . '/includes/parts.inc';

$failures = 0;

/**
 * Reports one check.
 */
function ok($condition, $label) {
  global $failures;
  echo ($condition ? 'ok    ' : 'FAIL  ') . $label . "\n";
  if (!$condition) {
    $failures++;
  }
}

// Post-processing.
$css = bootstrap_native_update_postprocess_css('.menu{--gap:1rem;color:var(--fg)}.menu-item{--bn-x:1}.nav .menu,.x>.menu{--bs-y:2}');
ok($css['css'] === '.bs-menu{--bs-gap:1rem;color:var(--bs-fg)}.menu-item{--bn-x:1}.nav .bs-menu,.x>.bs-menu{--bs-y:2}', 'css: prefix added, --bn-/--bs- kept, .menu renamed, .menu-item kept');
ok($css['properties'] == 2 && $css['menus'] == 3, 'css: counts');
$js = bootstrap_native_update_postprocess_js('q(".menu"),q(".dropdown .menu"),o.menu,q(`.menu>li`),q(".menu-item")');
ok($js['js'] === 'q(".bs-menu"),q(".dropdown .bs-menu"),o.menu,q(`.bs-menu>li`),q(".menu-item")', 'js: selectors in strings renamed, property o.menu kept');

// Classes.
$classes = bootstrap_native_update_css_classes('.a{}.lg\:navbar-expand .b-c{background:url(x.png)}.d{content:".e"}@media(min-width:.5rem){.f{}}');
ok(isset($classes['a'], $classes['lg:navbar-expand'], $classes['b-c'], $classes['f']) && !isset($classes['png']) && !isset($classes['e']), 'css classes: escaped colon, url() and strings left out');

$dir = sys_get_temp_dir() . '/bnu-test-' . getmypid();
mkdir($dir . '/templates', 0700, TRUE);
mkdir($dir . '/vendor', 0700, TRUE);
file_put_contents($dir . '/templates/page.tpl.php', '<div class="navbar <?php print $classes; ?> lg:navbar-expand"><?php print "not-a-template-class"; ?></div>');
file_put_contents($dir . '/template.php', "<?php\n\$x['class'][] = 'nav-link';\n\$y = array('btn-solid', 'theme-primary');\n\$z = \"\$interpolated-skip\";\n");
file_put_contents($dir . '/vendor/skip.php', "<?php 'vendor-class';");
$used = bootstrap_native_update_theme_classes($dir);
ok(isset($used['navbar'], $used['lg:navbar-expand'], $used['nav-link'], $used['btn-solid'], $used['theme-primary']), 'theme classes: templates and PHP strings');
ok(!isset($used['vendor-class']) && !isset($used['not-a-template-class']) && $used['navbar'] == 'templates/page.tpl.php', 'theme classes: vendor/ and PHP inside templates left out, file recorded');

mkdir($dir . '/scss-old');
mkdir($dir . '/scss-new');
file_put_contents($dir . '/scss-old/_x.scss', "// .comment-class\n.navbar-nav { a { color: red; } }\n.keep, .gone:hover { x: 1; }\n@use \"sass:map\";\n");
file_put_contents($dir . '/scss-new/_x.scss', ".keep { x: 1; }\n");
$gone = array_diff_key(bootstrap_native_update_source_classes($dir . '/scss-old'), bootstrap_native_update_source_classes($dir . '/scss-new'));
ok(array_keys($gone) == array('navbar-nav', 'gone'), 'source classes: removed ones found, comments and @use left out');

// Checks.
$old = array('css' => '.navbar-toggler-icon{}.btn{color:var(--bs-link-color)}a{color:var(--bs-link-color)}', 'js' => '', 'css_menus' => 16, 'js_menus' => 25, 'undefined' => array());
$new = array('css' => '.btn{color:var(--bs-link-color)}', 'js' => '', 'css_menus' => 16, 'js_menus' => 25, 'undefined' => array('--bs-new-gap'));
file_put_contents($dir . '/templates/page.tpl.php', '<i class="navbar-toggler-icon"></i>');
$result = bootstrap_native_update_run_checks($old, $new, array('bootstrap_native' => $dir, 'mysite' => $dir), array('--bs-link-color', '--bs-primary-base'));
$codes = array();
foreach ($result['rows'] as $row) {
  $codes[] = $row['code'] . ':' . $row['status'] . (isset($row['values']['theme']) ? ':' . $row['values']['theme'] : '');
}
ok($result['verdict'] == 'breaks', 'checks: verdict breaks');
ok(in_array('class_removed:breaks:bootstrap_native', $codes) && in_array('class_removed:check:mysite', $codes), 'checks: removed class is Breaks in the theme, Check in a sub-theme');
ok(in_array('color_property_fewer:check', $codes), 'checks: color variable read in fewer places is Check');
ok(in_array('undefined_properties:check', $codes) && in_array('size:info', $codes), 'checks: new undefined variables, size');
$new['css'] = '.navbar-toggler-icon{}.btn{}';
$result = bootstrap_native_update_run_checks($old, $new, array(), array('--bs-link-color'));
ok($result['rows'][2]['code'] == 'color_property' && $result['rows'][2]['status'] == 'breaks', 'checks: color variable read nowhere is Breaks');
$new['css_menus'] = 3;
$result = bootstrap_native_update_run_checks($old, $new, array(), array());
ok($result['rows'][1]['code'] == 'menu_rename' && $result['verdict'] == 'breaks', 'checks: menu rename falling short is Breaks');
$few = $old;
$few['css_menus'] = 3;
$same = bootstrap_native_update_run_checks($few, $new, array(), array());
ok($same['rows'][1]['code'] == 'menu_ok', 'checks: a build that never had many menus (no dropdown part) is not short of them');
$result = bootstrap_native_update_run_checks($old, array('css' => NULL, 'error' => 'Undefined mixin'), array(), array());
ok($result['verdict'] == 'failed' && count($result['rows']) == 1, 'checks: a failed build stops the checks');
$result = bootstrap_native_update_run_checks($old, $old, array('bootstrap_native' => $dir), array('--bs-link-color'));
ok($result['verdict'] == 'passed', 'checks: the same build passes');

// Archives.
ok(bootstrap_native_update_archive_problem(array(array('filename' => 'package/scss/a.scss', 'typeflag' => '0'), array('filename' => 'package/scss', 'typeflag' => '5')), 'package/') === '', 'archive: files and folders inside package/ pass');
ok(bootstrap_native_update_archive_problem(array(array('filename' => 'package/../x', 'typeflag' => '0')), 'package/') !== '', 'archive: ../ refused');
ok(bootstrap_native_update_archive_problem(array(array('filename' => 'other/x', 'typeflag' => '0')), 'package/') !== '', 'archive: outside package/ refused');
ok(bootstrap_native_update_archive_problem(array(array('filename' => 'package/link', 'typeflag' => '2')), 'package/') !== '', 'archive: symlink refused');
ok(bootstrap_native_update_archive_problem(array(), 'package/') !== '', 'archive: empty refused');

// Checksum.
file_put_contents($dir . '/f.tgz', 'release');
ok(bootstrap_native_update_integrity_matches($dir . '/f.tgz', 'sha512-' . base64_encode(hash('sha512', 'release', TRUE))), 'integrity: matches');
ok(!bootstrap_native_update_integrity_matches($dir . '/f.tgz', 'sha512-' . base64_encode(hash('sha512', 'other', TRUE))), 'integrity: mismatch');
ok(!bootstrap_native_update_integrity_matches($dir . '/f.tgz', 'sha1-abc'), 'integrity: only sha512');

// Registry.
$doc = array('versions' => array(
  '6.0.0-alpha.1' => array('dist' => array('tarball' => 'https://registry.npmjs.org/bootstrap/-/a.tgz', 'integrity' => 'sha512-x')),
  '6.0.0' => array('dist' => array('tarball' => 'https://registry.npmjs.org/bootstrap/-/b.tgz', 'integrity' => 'sha512-x')),
  '6.0.0-beta.2' => array('dist' => array('tarball' => 'https://registry.npmjs.org/bootstrap/-/c.tgz', 'integrity' => 'sha512-x')),
  '6.0.0-beta.10' => array('dist' => array('tarball' => 'https://registry.npmjs.org/bootstrap/-/d.tgz', 'integrity' => 'sha512-x')),
  '5.3.8' => array('dist' => array('tarball' => 'https://registry.npmjs.org/bootstrap/-/e.tgz', 'integrity' => 'sha512-x')),
  '6.0.1' => array('dist' => array('tarball' => 'https://evil.example/f.tgz', 'integrity' => 'sha512-x')),
  '6.0.2/../x' => array('dist' => array('tarball' => 'https://registry.npmjs.org/bootstrap/-/g.tgz', 'integrity' => 'sha512-x')),
), 'time' => array('6.0.0' => '2026-12-01T00:00:00.000Z'));
$releases = bootstrap_native_update_parse_registry($doc, 'https://registry.npmjs.org/bootstrap');
ok(array_keys($releases) == array('6.0.0', '6.0.0-beta.10', '6.0.0-beta.2', '6.0.0-alpha.1'), 'registry: 6.x only, newest first, other hosts and odd versions dropped');
ok($releases['6.0.0']['prerelease'] === FALSE && $releases['6.0.0-beta.2']['prerelease'] === TRUE && $releases['6.0.0']['date'] > 0, 'registry: pre-release flag, date');

// Running programs.
$run = bootstrap_native_update_run(array('/bin/sh', '-c', 'echo out; echo err >&2; exit 3'));
ok($run['status'] === 3 && trim($run['stdout']) == 'out' && trim($run['stderr']) == 'err', 'run: output and exit code');
$run = bootstrap_native_update_run(array('/bin/sleep', '5'), 1);
ok($run['timed_out'] && $run['seconds'] < 3, 'run: stopped at the timeout');
ok(bootstrap_native_update_sass_version('/bin/echo') === NULL, 'sass version: a program that is not Sass');

// Parts (against the shipped release).
$theme = $module . '/themes/bootstrap_native';
$scss = $theme . '/vendor/bootstrap/scss';
$parts = bootstrap_native_update_release_parts($scss);
ok(isset($parts['accordion'], $parts['forms/otp-input'], $parts['utilities/api'], $parts['content/reboot']) && !isset($parts['banner']) && !isset($parts['forms']), 'parts: from bootstrap.scss, folders expanded, banner left out');
ok($parts['forms/otp-input'] == 'forms' && $parts['accordion'] == 'components' && $parts['content/reboot'] == 'base', 'parts: groups');
$groups = bootstrap_native_update_utility_groups($scss);
ok(isset($groups['margin']) && in_array('margin-x', $groups['margin']) && count($groups) > 40, 'utility groups: from the scss-docs markers');
$required = bootstrap_native_update_required_parts($theme, $scss);
ok(in_array('alert', $required) && in_array('buttons/button', $required) && in_array('forms/form-control', $required) && !in_array('accordion', $required), "required: the parts the theme's partials use, folders expanded");
$default = bootstrap_native_update_default_selection($theme, $scss);
$context = array('parts' => $parts, 'utility_groups' => $groups, 'default' => $default, 'required' => $required, 'needed' => array('carousel'), 'exclude' => array('placeholder' => 'x'));
$effective = bootstrap_native_update_effective_selection(array('parts' => array('placeholder', 'stepper'), 'utilities' => array('margin', 'nope')), $context);
ok(in_array('alert', $effective['parts']) && in_array('carousel', $effective['parts']) && in_array('stepper', $effective['parts']) && !in_array('placeholder', $effective['parts']), 'effective: required and needed in, excluded out');
ok($effective['utilities'] == array('margin'), 'effective: unknown utility groups dropped');
ok(array_search('config', $effective['parts']) < array_search('alert', $effective['parts']) && array_search('alert', $effective['parts']) < array_search('stepper', $effective['parts']), "effective: in Bootstrap's order");
$default_effective = bootstrap_native_update_effective_selection(NULL, array('needed' => array()) + $context);
ok(bootstrap_native_update_selection_key($default_effective, $default_effective) == 'default' && preg_match('/^[0-9a-f]{8}$/', bootstrap_native_update_selection_key($effective, $default_effective)), 'selection key: default, else a hash');
ok(bootstrap_native_update_selection_key($default_effective, $default_effective, array('/x/_extras.scss')) != 'default', 'selection key: extra Sass makes its own build');
$files = bootstrap_native_update_entry_files($effective, $groups, array('bnu-extra-0'));
$entry = $files['bootstrap_native.scss'];
ok(isset($files['_bnu-utilities.scss']) && strpos($files['_bnu-utilities.scss'], '"padding"') !== FALSE && strpos($files['_bnu-utilities.scss'], '"margin"') === FALSE, 'entry: utility groups left out are removed from the map');
ok(strpos($entry, '@use "bnu-utilities"') < strpos($entry, '@use "bootstrap/scss/utilities/api"') && strpos($entry, '@use "bnu-extra-0"') < strpos($entry, '@use "drupal";'), 'entry: filter before the API, add-ons before the Drupal rules');
$all = bootstrap_native_update_entry_files($default_effective, $groups);
ok(!isset($all['_bnu-utilities.scss']), 'entry: no filter when every group is in');

// Docs pages.
$pages = bootstrap_native_update_docs_pages('<a href="/docs/6.0/forms/otp-input">x</a><a href="/docs/6.0/forms/datepicker/">x</a><a href="/docs/6.0/utilities/text-wrapping">x</a><a href="/docs/6.0/helpers/vertical-rule">x</a><a href="/docs/5.3/forms/checks-radios">old</a><a href="/docs/6.0/forms/checkbox">x</a>', '6.0');
ok(count($pages) == 5, 'docs: pages of this version only');
ok(bootstrap_native_update_docs_page('forms/otp-input', $pages) == 'forms/otp-input', 'docs: same name');
ok(bootstrap_native_update_docs_page('datepicker', $pages) == 'forms/datepicker', 'docs: found in another section');
ok(bootstrap_native_update_docs_page('utilities:text-wrap', $pages) == 'utilities/text-wrapping', 'docs: close name');
ok(bootstrap_native_update_docs_page('forms/check', $pages) == 'forms/checkbox', 'docs: close name (check, checkbox)');
ok(bootstrap_native_update_docs_page('helpers/vr', $pages) == 'helpers/vertical-rule', 'docs: alias');
ok(bootstrap_native_update_docs_page('carousel', $pages) === '', 'docs: none found, no link');

exec('rm -rf ' . escapeshellarg($dir));
echo $failures ? "$failures failed\n" : "all passed\n";
exit($failures ? 1 : 0);
