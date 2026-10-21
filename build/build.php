<?php

/**
 * @file
 * Developer build: themes/bootstrap_native/css/bootstrap_native.css and
 * js/bootstrap.bundle.min.js from scss/ and the shipped Bootstrap in
 * vendor/bootstrap. Run through build/build.sh.
 *
 * Dart Sass comes from SASS=/path/to/sass, sites/all/libraries/dart-sass,
 * the PATH or ~/tools/dart-sass, in that order.
 */

$module = dirname(__DIR__);
require $module . '/includes/build.inc';
$theme = $module . '/themes/bootstrap_native';
$dev = in_array('--dev', array_slice($argv, 1));

$sass = bootstrap_native_update_sass_find(array_merge(
  array((string) getenv('SASS'), dirname(dirname($module)) . '/libraries/dart-sass'),
  bootstrap_native_update_sass_on_path(),
  array(getenv('HOME') . '/tools/dart-sass')
));
if (!$sass) {
  fwrite(STDERR, "No Dart Sass found (https://sass-lang.com/install, standalone release): set SASS=/path/to/sass.\n");
  exit(1);
}

$build = bootstrap_native_update_build($sass['path'], $theme, $theme . '/vendor', $dev);
if ($build['error']) {
  fwrite(STDERR, $build['error'] . "\n");
  exit(1);
}
$problems = array();
if ($build['properties'] < BOOTSTRAP_NATIVE_UPDATE_MIN_PROPERTIES || $build['css_menus'] < BOOTSTRAP_NATIVE_UPDATE_MIN_CSS_MENUS) {
  $problems[] = sprintf('css: only %d custom properties and %d .menu selectors changed', $build['properties'], $build['css_menus']);
}
if ($build['js_menus'] < BOOTSTRAP_NATIVE_UPDATE_MIN_JS_MENUS) {
  $problems[] = sprintf('js: only %d .menu selectors changed', $build['js_menus']);
}
if ($problems) {
  fwrite(STDERR, implode("\n", $problems) . "\nNothing written: check what changed in Bootstrap.\n");
  exit(1);
}

file_put_contents($theme . '/css/bootstrap_native.css', $build['css']);
file_put_contents($theme . '/js/bootstrap.bundle.min.js', $build['js']);
preg_match_all('/(--bs-[\w-]+)\s*:/', $build['css'], $m);
printf("Dart Sass %s, %.1f s\n", $sass['version'], $build['seconds']);
printf("css: %d custom properties prefixed, %d .menu -> .bs-menu\n", $build['properties'], $build['css_menus']);
printf("js: %d .menu -> .bs-menu\n", $build['js_menus']);
if ($build['undefined']) {
  echo 'Used but never defined: ', implode(', ', $build['undefined']), "\n";
}
printf("css: %d KB, %d properties defined\n", strlen($build['css']) / 1024, count(array_unique($m[1])));
exit($build['undefined'] ? 1 : 0);
