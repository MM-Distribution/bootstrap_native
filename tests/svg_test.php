<?php

/**
 * @file
 * Tests of includes/svg.inc (plain PHP: php tests/svg_test.php).
 */

require dirname(__DIR__) . '/includes/svg.inc';

$failures = 0;
function check($name, $ok) {
  global $failures;
  print ($ok ? 'ok    ' : 'FAIL  ') . $name . "\n";
  if (!$ok) {
    $failures++;
  }
}
function svg($inner, $attrs = '') {
  return '<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 10 10"' . $attrs . '>' . $inner . '</svg>';
}
function clean($inner, $attrs = '') {
  return bootstrap_native_update_svg_clean(svg($inner, $attrs));
}

$logo = clean('<defs><linearGradient id="g"><stop offset="0" stop-color="#f19c21"/></linearGradient></defs><title>Logo</title><path d="M0 0h10v10z" fill="url(#g)"/><text x="1" y="9" style="font-weight:700">A</text>');
check('a plain logo keeps its drawing', strpos($logo, '<path d="M0 0h10v10z" fill="url(#g)"/>') !== FALSE && strpos($logo, 'linearGradient') !== FALSE && strpos($logo, '<title>Logo</title>') !== FALSE && strpos($logo, 'font-weight:700') !== FALSE);
check('the result is an SVG file', strpos($logo, '<?xml') === 0 && strpos($logo, 'viewBox="0 0 10 10"') !== FALSE);

check('script elements go', strpos(clean('<script>alert(1)</script><rect width="1" height="1"/>'), 'script') === FALSE);
check('script in another prefix goes', strpos(clean('<svg:script xmlns:svg="http://www.w3.org/2000/svg">alert(1)</svg:script>'), 'alert') === FALSE);
check('foreignObject with HTML goes', strpos(clean('<foreignObject><div xmlns="http://www.w3.org/1999/xhtml"><img src="x" onerror="alert(1)"/></div></foreignObject>'), 'onerror') === FALSE);
check('event attributes go', strpos(clean('<rect width="1" height="1" onclick="alert(1)" onload="x()"/>', ' onload="alert(1)"'), 'on') === FALSE || !preg_match('/\son\w+=/', clean('<rect width="1" height="1" onclick="alert(1)"/>', ' onload="alert(1)"')));
check('javascript links go', strpos(clean('<a href="javascript:alert(1)"><rect width="1" height="1"/></a>'), 'javascript') === FALSE);
check('xlink:href to javascript goes', strpos(clean('<use xlink:href="javascript:alert(1)"/>'), 'javascript') === FALSE);
check('use of an outside file goes', strpos(clean('<use href="https://example.com/x.svg#a"/>'), 'example.com') === FALSE);
check('use of an own part stays', strpos(clean('<symbol id="s"><rect width="1" height="1"/></symbol><use href="#s"/>'), 'href="#s"') !== FALSE);
check('animations that could set links go', strpos(clean('<set attributeName="href" to="javascript:alert(1)"/><animate attributeName="href" values="javascript:alert(1)"/>'), 'javascript') === FALSE);
check('embedded PNG picture stays', strpos(clean('<image href="data:image/png;base64,iVBORw0KGgo=" width="1" height="1"/>'), 'data:image/png') !== FALSE);
check('embedded SVG or HTML picture goes', strpos(clean('<image href="data:image/svg+xml;base64,PHN2Zz4=" width="1" height="1"/><image href="data:text/html,x"/>'), 'data:') === FALSE);
check('styles lose @import and outside url()', ($c = clean('<style>@import url(https://example.com/x.css); .a{fill:url(https://example.com/y)} .b{fill:url(#g)}</style>')) && strpos($c, 'example.com') === FALSE && strpos($c, 'url(#g)') !== FALSE);
check('CSS escapes do not hide url()', strpos(clean('<rect width="1" height="1" style="fill:u\72l(https://example.com/z)"/>'), 'example.com') === FALSE);
check('editor attributes and elements go', strpos(clean('<sodipodi:namedview xmlns:sodipodi="http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"/><rect xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" inkscape:label="x" width="1" height="1"/>'), 'inkscape:label') === FALSE);
check('comments go', strpos(clean('<!-- note --><rect width="1" height="1"/>'), 'note') === FALSE);

check('entities are refused', bootstrap_native_update_svg_clean('<?xml version="1.0"?><!DOCTYPE svg [<!ENTITY x SYSTEM "file:///etc/passwd">]><svg xmlns="http://www.w3.org/2000/svg">&x;</svg>') === FALSE);
check('not XML is refused', bootstrap_native_update_svg_clean('<svg><rect></svg') === FALSE);
check('HTML is refused', bootstrap_native_update_svg_clean('<html><body><script>alert(1)</script></body></html>') === FALSE);
check('an svg without the SVG namespace is refused', bootstrap_native_update_svg_clean('<svg><rect/></svg>') === FALSE);
check('empty is refused', bootstrap_native_update_svg_clean('') === FALSE);

print $failures ? "$failures failed\n" : "all passed\n";
exit($failures ? 1 : 0);
