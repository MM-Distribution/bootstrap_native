<?php

/**
 * @file
 * Tests of includes/color.inc (plain PHP: php tests/color_test.php).
 */

require dirname(__DIR__) . '/includes/color.inc';

$failures = 0;
function check($name, $ok) {
  global $failures;
  print ($ok ? 'ok    ' : 'FAIL  ') . $name . "\n";
  $failures += $ok ? 0 : 1;
}

$white = array(1, 1, 1);
$black = array(0, 0, 0);

check('parse #abc', bs_native_color_hex(bs_native_color_parse('#abc')) === '#aabbcc');
check('parse without #, uppercase', bs_native_color_hex(bs_native_color_parse('F19C21')) === '#f19c21');
check('reject non-hex', bs_native_color_parse('red') === NULL && bs_native_color_parse('#12345') === NULL && bs_native_color_parse('') === NULL);
check('contrast black/white is 21', abs(bs_native_color_contrast($black, $white) - 21) < 0.01);
check('OKLCH round trip', bs_native_color_hex(bs_native_color_from_oklch(bs_native_color_oklch(bs_native_color_parse('#1d3557')))) === '#1d3557');

// Text on buttons: white on Bootstrap blue, black on a light orange.
check('white text on #0d6efd', bs_native_color_on(bs_native_color_parse('#0d6efd')) === $white);
check('black text on #f19c21', bs_native_color_on(bs_native_color_parse('#f19c21')) === $black);

// Readable text: passes 4.5:1, keeps colors that already pass.
$orange = bs_native_color_parse('#f19c21');
$text = bs_native_color_readable($orange, $white, 4.5);
check('orange as text on white reaches 4.5:1', bs_native_color_contrast($text, $white) >= 4.5);
check('...and stays close (not black)', bs_native_color_contrast($text, $white) < 5);
check('navy on white unchanged', bs_native_color_readable(bs_native_color_parse('#1d3557'), $white) === bs_native_color_parse('#1d3557'));
check('navy on dark gets lighter', bs_native_color_luminance(bs_native_color_readable(bs_native_color_parse('#1d3557'), bs_native_color_parse('#121417'))) > bs_native_color_luminance(bs_native_color_parse('#1d3557')));

// Tokens.
$none = bs_native_color_tokens(array());
check('no settings: no variables, light', $none['vars'] === array() && $none['scheme'] === 'light');
$tokens = bs_native_color_tokens(array('primary' => '#f19c21'));
check('primary: button keeps the brand color', $tokens['vars']['--bs-primary-bg'] === '#f19c21');
check('primary: link made readable and noted', isset($tokens['notes']['primary']) && bs_native_color_contrast(bs_native_color_parse($tokens['vars']['--bs-link-color']), $white) >= 4.5);
check('primary: no base variables', !isset($tokens['vars']['--bs-bg-body']));
$dark = bs_native_color_tokens(array('base' => '#121417'));
check('dark base: dark scheme, light text', $dark['scheme'] === 'dark' && bs_native_color_luminance(bs_native_color_parse($dark['vars']['--bs-fg-body'])) > 0.7);
foreach ($dark['vars'] as $name => $value) {
  if (preg_match('/^--bs-fg-[123]$/', $name)) {
    check("dark base: $name readable", bs_native_color_contrast(bs_native_color_parse($value), bs_native_color_parse('#121417')) >= 4.5);
  }
}
$css = bs_native_color_css($tokens);
check('css is one :root rule with hex values only', (bool) preg_match('/^:root\{color-scheme:light;(--bs-[a-z0-9-]+:#[0-9a-f]{6};)+\}$/', $css));

print $failures ? "$failures failed\n" : "all passed\n";
exit($failures ? 1 : 0);
