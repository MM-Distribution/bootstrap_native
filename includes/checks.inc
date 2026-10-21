<?php

/**
 * @file
 * The checks a new Bootstrap release goes through before it is applied.
 *
 * Plain PHP, no Drupal (tests/checks_test.php runs it directly). Results are
 * codes and values; the Bootstrap updates page words them.
 *
 * Each check compares the new build with the one the site uses now, so a
 * problem the site already has isn't blamed on the update.
 */

/**
 * Class names in CSS (escaped colons, as in lg:navbar-expand, unescaped).
 */
function bootstrap_native_update_css_classes($css) {
  // url(...) and strings can hold dots that aren't classes.
  $css = preg_replace(array('/url\([^)]*\)/', '/"[^"]*"/', "/'[^']*'/"), '', $css);
  preg_match_all('/\.((?:[a-zA-Z_-]|\\\\:)(?:[\w-]|\\\\:)*)/', $css, $m);
  $classes = array();
  foreach ($m[1] as $class) {
    $classes[str_replace('\\:', ':', $class)] = TRUE;
  }
  return $classes;
}

/**
 * Class names in Bootstrap's own SCSS sources (comments left out).
 *
 * Catches a class Bootstrap drops even when the theme's own selectors keep
 * the name in the compiled CSS (.navbar-nav > li, for one).
 */
function bootstrap_native_update_source_classes($scss_dir) {
  $classes = array();
  if (!is_dir($scss_dir)) {
    return $classes;
  }
  $files = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($scss_dir, FilesystemIterator::SKIP_DOTS));
  foreach ($files as $file) {
    if (substr($file->getFilename(), -5) != '.scss') {
      continue;
    }
    $scss = file_get_contents($file->getPathname());
    $scss = preg_replace(array('#/\*.*?\*/#s', '#(?<!:)//[^\n]*#'), '', $scss);
    // Only selectors: what comes before a {, not values or @use paths.
    preg_match_all('/([^{};]+)\{/', $scss, $blocks);
    foreach ($blocks[1] as $selector) {
      if (strpos(ltrim($selector), '@') === 0 && !preg_match('/^\s*@at-root/', $selector)) {
        continue;
      }
      $classes += bootstrap_native_update_css_classes($selector);
    }
  }
  return $classes;
}

/**
 * Classes a theme's code and templates use, with the first file for each.
 *
 * Templates: class="..." attributes (PHP inside them left out). PHP: every
 * quoted string made of class-like words; only names Bootstrap defines
 * count later, so the rest (array keys, messages) doesn't matter.
 *
 * @param string $dir
 *   The theme's folder.
 *
 * @return array
 *   Relative file path, keyed by class.
 */
function bootstrap_native_update_theme_classes($dir) {
  $classes = array();
  if (!is_dir($dir)) {
    return $classes;
  }
  $files = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS));
  foreach ($files as $file) {
    $path = $file->getPathname();
    $relative = substr($path, strlen(rtrim($dir, '/')) + 1);
    if (preg_match('#(^|/)(vendor|node_modules)/#', $relative) || !preg_match('/\.(php|inc|module|theme)$/', $path)) {
      continue;
    }
    $source = file_get_contents($path);
    $strings = array();
    preg_match_all('/class\s*=\s*"([^"]*)"|class\s*=\s*\'([^\']*)\'/', $source, $m);
    foreach (array_merge($m[1], $m[2]) as $value) {
      $strings[] = preg_replace('/<\?php.*?\?>/s', ' ', $value);
    }
    if (substr($path, -8) != '.tpl.php') {
      preg_match_all('/\'([^\'\\\\]*)\'|"([^"\\\\$]*)"/', $source, $m);
      $strings = array_merge($strings, $m[1], $m[2]);
    }
    foreach ($strings as $string) {
      foreach (preg_split('/\s+/', trim($string)) as $word) {
        if (preg_match('/^[a-zA-Z_-][\w:-]*$/', $word) && !isset($classes[$word])) {
          $classes[$word] = $relative;
        }
      }
    }
  }
  return $classes;
}

/**
 * Runs the checks.
 *
 * @param array $old
 *   The build the site uses now: css, js, properties, css_menus, js_menus,
 *   undefined; plus scss_dir (that release's bootstrap/scss).
 * @param array $new
 *   The new build, the same keys, plus error when it failed.
 * @param array $themes
 *   Themes to look at: folder, keyed by name; the key 'bootstrap_native'
 *   is the base theme (its problems are Breaks, a sub-theme's are Check).
 * @param array $color_properties
 *   Custom properties the color settings set.
 *
 * @return array
 *   Keys: verdict (passed, check, breaks or failed), rows (each: check,
 *   status passed/check/breaks/info/failed, code, values).
 */
function bootstrap_native_update_run_checks(array $old, array $new, array $themes, array $color_properties) {
  $rows = array();
  if (!empty($new['error']) || $new['css'] === NULL) {
    $rows[] = array('check' => 'build', 'status' => 'failed', 'code' => 'build_failed', 'values' => array('error' => isset($new['error']) ? $new['error'] : ''));
    return array('verdict' => 'failed', 'rows' => $rows);
  }
  $rows[] = array('check' => 'build', 'status' => 'passed', 'code' => 'build_ok', 'values' => array());

  // The .menu rename: too few means Bootstrap's dropdown would hide every
  // Drupal menu, or its script would miss the renamed menus. A build that
  // leaves the dropdown out (a theme with fixed parts) has fewer to begin
  // with: only fewer than now counts.
  $menu = array('css' => $new['css_menus'], 'js' => $new['js_menus']);
  if (($new['css_menus'] < BOOTSTRAP_NATIVE_UPDATE_MIN_CSS_MENUS && $new['css_menus'] < $old['css_menus']) || ($new['js_menus'] < BOOTSTRAP_NATIVE_UPDATE_MIN_JS_MENUS && $new['js_menus'] < $old['js_menus'])) {
    $rows[] = array('check' => 'menu', 'status' => 'breaks', 'code' => 'menu_rename', 'values' => $menu + array('old_css' => $old['css_menus'], 'old_js' => $old['js_menus']));
  }
  else {
    $rows[] = array('check' => 'menu', 'status' => 'passed', 'code' => 'menu_ok', 'values' => $menu);
  }

  // Color settings: each property they set must still be read where it
  // was. Read nowhere: the setting no longer does anything (Breaks). Read
  // in fewer places: some parts stopped following it (Check).
  $color_rows = array();
  foreach ($color_properties as $property) {
    $used = 'var(' . $property . ')';
    $before = substr_count($old['css'], $used) + substr_count($old['css'], 'var(' . $property . ',');
    $after = substr_count($new['css'], $used) + substr_count($new['css'], 'var(' . $property . ',');
    if ($before && !$after) {
      $color_rows[] = array('check' => 'colors', 'status' => 'breaks', 'code' => 'color_property', 'values' => array('property' => $property));
    }
    elseif ($after < $before) {
      $color_rows[] = array('check' => 'colors', 'status' => 'check', 'code' => 'color_property_fewer', 'values' => array('property' => $property, 'old' => $before, 'new' => $after));
    }
  }
  if ($color_rows) {
    $rows = array_merge($rows, $color_rows);
  }
  else {
    $rows[] = array('check' => 'colors', 'status' => 'passed', 'code' => 'colors_ok', 'values' => array());
  }

  // Classes the themes use that Bootstrap no longer has.
  $removed = array_diff_key(bootstrap_native_update_css_classes($old['css']), bootstrap_native_update_css_classes($new['css']));
  if (!empty($old['scss_dir']) && !empty($new['scss_dir'])) {
    $removed += array_diff_key(bootstrap_native_update_source_classes($old['scss_dir']), bootstrap_native_update_source_classes($new['scss_dir']));
  }
  $class_rows = array();
  foreach ($themes as $name => $dir) {
    foreach (array_intersect_key(bootstrap_native_update_theme_classes($dir), $removed) as $class => $file) {
      $class_rows[] = array('check' => 'classes', 'status' => $name == 'bootstrap_native' ? 'breaks' : 'check', 'code' => 'class_removed', 'values' => array('class' => $class, 'theme' => $name, 'file' => $file));
    }
  }
  if ($class_rows) {
    $rows = array_merge($rows, $class_rows);
  }
  else {
    $rows[] = array('check' => 'classes', 'status' => 'passed', 'code' => 'classes_ok', 'values' => array('themes' => array_keys($themes)));
  }

  // Custom properties used but defined nowhere (new ones only).
  $undefined = array_values(array_diff($new['undefined'], $old['undefined']));
  if ($undefined) {
    $rows[] = array('check' => 'properties', 'status' => 'check', 'code' => 'undefined_properties', 'values' => array('properties' => $undefined));
  }

  $rows[] = array('check' => 'size', 'status' => 'info', 'code' => 'size', 'values' => array('new' => strlen($new['css']), 'old' => strlen($old['css'])));

  $verdict = 'passed';
  foreach ($rows as $row) {
    if ($row['status'] == 'breaks') {
      $verdict = 'breaks';
      break;
    }
    if ($row['status'] == 'check') {
      $verdict = 'check';
    }
  }
  return array('verdict' => $verdict, 'rows' => $rows);
}

/**
 * Checks that an npm package archive is safe to unpack.
 *
 * @param array $entries
 *   Archive_Tar::listContent() output.
 * @param string $prefix
 *   Every entry must be inside this folder (npm: "package/").
 *
 * @return string
 *   '' when safe, else the first problem.
 */
function bootstrap_native_update_archive_problem(array $entries, $prefix) {
  if (!$entries) {
    return 'The archive is empty or unreadable.';
  }
  foreach ($entries as $entry) {
    $name = $entry['filename'];
    if (strpos($name, $prefix) !== 0 || $name[0] == '/' || preg_match('#(^|/)\.\.(/|$)#', $name) || strpos($name, "\0") !== FALSE || strpos($name, '\\') !== FALSE) {
      return 'The archive holds a file outside ' . $prefix . ': ' . $name;
    }
    // Plain files and folders only: no links, devices or the like.
    if (!in_array((string) $entry['typeflag'], array('', '0', '5'), TRUE)) {
      return 'The archive holds something other than files and folders: ' . $name;
    }
  }
  return '';
}

/**
 * Checks a file against an npm "integrity" value (sha512-<base64>).
 */
function bootstrap_native_update_integrity_matches($file, $integrity) {
  if (!preg_match('/^sha512-([A-Za-z0-9+\/=]+)$/', (string) $integrity, $m) || !is_file($file)) {
    return FALSE;
  }
  return hash_equals($m[1], base64_encode(hash_file('sha512', $file, TRUE)));
}

/**
 * Bootstrap 6 releases from an npm registry document, newest first.
 *
 * @param array $doc
 *   The decoded document (https://registry.npmjs.org/bootstrap).
 * @param string $registry
 *   The registry's URL: tarballs must come from the same host.
 *
 * @return array
 *   Keyed by version: version, date (timestamp or 0), tarball, integrity,
 *   prerelease (bool).
 */
function bootstrap_native_update_parse_registry(array $doc, $registry) {
  $host = parse_url($registry, PHP_URL_HOST);
  $releases = array();
  if (empty($doc['versions']) || !is_array($doc['versions'])) {
    return $releases;
  }
  foreach ($doc['versions'] as $version => $info) {
    if (!preg_match('/^6\.\d+\.\d+(-[0-9A-Za-z.-]+)?$/', $version, $m) || empty($info['dist']['tarball']) || empty($info['dist']['integrity'])) {
      continue;
    }
    $tarball = $info['dist']['tarball'];
    if (parse_url($tarball, PHP_URL_SCHEME) !== parse_url($registry, PHP_URL_SCHEME) || parse_url($tarball, PHP_URL_HOST) !== $host) {
      continue;
    }
    $releases[$version] = array(
      'version' => $version,
      'date' => isset($doc['time'][$version]) ? (int) strtotime($doc['time'][$version]) : 0,
      'tarball' => $tarball,
      'integrity' => $info['dist']['integrity'],
      'prerelease' => !empty($m[1]),
    );
  }
  uksort($releases, function ($a, $b) {
    return version_compare($b, $a);
  });
  return $releases;
}
