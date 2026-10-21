<?php

/**
 * @file
 * Tests of the theme's includes/color.inc (plain PHP: php tests/color_test.php).
 */

require dirname(__DIR__) . '/themes/bootstrap_native/includes/color.inc';

$failures = 0;
function check($name, $ok) {
  global $failures;
  print ($ok ? 'ok    ' : 'FAIL  ') . $name . "\n";
  $failures += $ok ? 0 : 1;
}

$white = array(1, 1, 1);
$black = array(0, 0, 0);

check('parse #abc', bootstrap_native_color_hex(bootstrap_native_color_parse('#abc')) === '#aabbcc');
check('parse without #, uppercase', bootstrap_native_color_hex(bootstrap_native_color_parse('F19C21')) === '#f19c21');
check('reject non-hex', bootstrap_native_color_parse('red') === NULL && bootstrap_native_color_parse('#12345') === NULL && bootstrap_native_color_parse('') === NULL);
check('contrast black/white is 21', abs(bootstrap_native_color_contrast($black, $white) - 21) < 0.01);
check('OKLCH round trip', bootstrap_native_color_hex(bootstrap_native_color_from_oklch(bootstrap_native_color_oklch(bootstrap_native_color_parse('#1d3557')))) === '#1d3557');

// Text on buttons: white on Bootstrap blue, black on a light orange.
check('white text on #0d6efd', bootstrap_native_color_on(bootstrap_native_color_parse('#0d6efd')) === $white);
check('black text on #f19c21', bootstrap_native_color_on(bootstrap_native_color_parse('#f19c21')) === $black);

// Readable text: passes 4.5:1, keeps colors that already pass.
$orange = bootstrap_native_color_parse('#f19c21');
$text = bootstrap_native_color_readable($orange, $white, 4.5);
check('orange as text on white reaches 4.5:1', bootstrap_native_color_contrast($text, $white) >= 4.5);
check('...and stays close (not black)', bootstrap_native_color_contrast($text, $white) < 5);
check('navy on white unchanged', bootstrap_native_color_readable(bootstrap_native_color_parse('#1d3557'), $white) === bootstrap_native_color_parse('#1d3557'));
check('navy on dark gets lighter', bootstrap_native_color_luminance(bootstrap_native_color_readable(bootstrap_native_color_parse('#1d3557'), bootstrap_native_color_parse('#121417'))) > bootstrap_native_color_luminance(bootstrap_native_color_parse('#1d3557')));

// Tokens.
$none = bootstrap_native_color_tokens(array());
check('no settings: no variables, light', $none['vars'] === array() && $none['scheme'] === 'light');
$tokens = bootstrap_native_color_tokens(array('primary' => '#f19c21'));
check('primary: button keeps the brand color', $tokens['vars']['--bs-primary-bg'] === '#f19c21');
check('primary: link made readable and noted', isset($tokens['notes']['primary']) && bootstrap_native_color_contrast(bootstrap_native_color_parse($tokens['vars']['--bs-link-color']), $white) >= 4.5);
check('primary: no base variables', !isset($tokens['vars']['--bs-bg-body']));
$dark = bootstrap_native_color_tokens(array('base' => '#121417'));
check('dark base: dark scheme, light text', $dark['scheme'] === 'dark' && bootstrap_native_color_luminance(bootstrap_native_color_parse($dark['vars']['--bs-fg-body'])) > 0.7);
foreach ($dark['vars'] as $name => $value) {
  if (preg_match('/^--bs-fg-[123]$/', $name)) {
    check("dark base: $name readable", bootstrap_native_color_contrast(bootstrap_native_color_parse($value), bootstrap_native_color_parse('#121417')) >= 4.5);
  }
}
$css = bootstrap_native_color_css($tokens);
check('css is one :root rule with hex values only', (bool) preg_match('/^:root\{color-scheme:light;(--b[sn]-[a-z0-9-]+:#[0-9a-f]{6};)+\}$/', $css));

// Bootstrap's named colors (from the shipped release; hex values as
// Chromium draws Bootstrap's oklch() and color-mix() values).
bootstrap_native_color_palette(bootstrap_native_color_palette_parse(file_get_contents(dirname(__DIR__) . '/themes/bootstrap_native/vendor/bootstrap/scss/_colors.scss')));
$palette = bootstrap_native_color_palette();
check('palette: 16 colors, 6 tints, 6 shades', count($palette['colors']) == 16 && count($palette['tints']) == 6 && count($palette['shades']) == 6);
foreach (array('orange' => '#ff6800', 'orange-500' => '#ff6800', 'indigo-600' => '#5d31cb', 'blue-100' => '#c9eaff', 'gray-975' => '#080a0c') as $name => $hex) {
  check("named: $name is $hex", bootstrap_native_color_hex(bootstrap_native_color_parse($name)) === $hex);
}
check('named: hex without # still works (abcdef, fab)', bootstrap_native_color_hex(bootstrap_native_color_parse('abcdef')) === '#abcdef' && bootstrap_native_color_hex(bootstrap_native_color_parse('fab')) === '#ffaabb');
check('named: unknown names and stops are not colors', !bootstrap_native_color_parse('nope') && !bootstrap_native_color_parse('red-123') && !bootstrap_native_color_parse('red-'));
check('named: is_name tells names from hex', bootstrap_native_color_is_name('teal-300') && !bootstrap_native_color_is_name('#abcdef') && !bootstrap_native_color_is_name('abcdef1'));
$named = bootstrap_native_color_tokens(array('primary' => 'indigo-600'));
check('named: tokens use the worked-out hex', $named['vars']['--bs-primary-base'] === '#5d31cb');

// The automatic hover is a clear step from the link (about 0.18 in OKLCH
// lightness), the other way for a link already near black.
foreach (array(array('#f19c21', '#ffffff'), array('#0d6efd', '#ffffff'), array('#222222', '#ffffff'), array('#f19c21', '#14181f')) as $case) {
  $t = bootstrap_native_color_tokens(array('primary' => $case[0], 'base' => $case[1]));
  list($ll) = bootstrap_native_color_oklch(bootstrap_native_color_parse($t['vars']['--bs-link-color']));
  list($hl) = bootstrap_native_color_oklch(bootstrap_native_color_parse($t['vars']['--bs-link-hover-color']));
  check("hover: {$case[0]} on {$case[1]} steps at least 0.15 and stays readable", abs($hl - $ll) >= 0.15 && bootstrap_native_color_contrast(bootstrap_native_color_parse($t['vars']['--bs-link-hover-color']), bootstrap_native_color_parse($case[1])) >= 4.5);
}

// Menus, tabs, navbar and breadcrumb follow the link colors, only once set.
check('nav links: none without link colors', !isset($none['vars']['--bn-nav-link-color']));
$t = bootstrap_native_color_tokens(array('link' => '#1d3557', 'link_hover' => '#f19c21'));
check('nav links: follow link and hover', $t['vars']['--bn-nav-link-color'] === $t['vars']['--bs-link-color'] && $t['vars']['--bn-nav-link-hover-color'] === $t['vars']['--bs-link-hover-color']);

print $failures ? "$failures failed\n" : "all passed\n";
exit($failures ? 1 : 0);
