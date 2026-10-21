<?php

/**
 * @file
 * Builds the theme's stylesheet and script from a Bootstrap release.
 *
 * Plain PHP, no Drupal: build/build.php (the developer build) uses the same
 * functions as the Bootstrap updates page and drush.
 *
 * What a build does to Bootstrap's output:
 * - Custom properties get the --bs- prefix (as in Bootstrap's own dist
 *   files, so its docs apply); the theme's own use --bn-.
 * - .menu becomes .bs-menu: Bootstrap 6's dropdown is .menu {display: none},
 *   and every Drupal menu is a <ul class="menu">. Bootstrap markup in the
 *   theme uses class="bs-menu" for dropdowns.
 */

/**
 * Fewest changes a build of the shipped release makes; fewer means
 * Bootstrap moved things and the output needs a look.
 */
define('BOOTSTRAP_NATIVE_UPDATE_MIN_PROPERTIES', 1000);
define('BOOTSTRAP_NATIVE_UPDATE_MIN_CSS_MENUS', 10);
define('BOOTSTRAP_NATIVE_UPDATE_MIN_JS_MENUS', 20);

/**
 * Seconds a Sass build may take before it is stopped.
 */
define('BOOTSTRAP_NATIVE_UPDATE_SASS_TIMEOUT', 120);

/**
 * Runs a command without a shell in between.
 *
 * @param array $command
 *   The program, then its arguments.
 * @param int $timeout
 *   Seconds before the program is stopped.
 *
 * @return array
 *   Keys: status (exit code; -1 if it couldn't start or was stopped),
 *   stdout, stderr, seconds, timed_out.
 */
function bootstrap_native_update_run(array $command, $timeout = 30) {
  $result = array('status' => -1, 'stdout' => '', 'stderr' => '', 'seconds' => 0, 'timed_out' => FALSE);
  $start = microtime(TRUE);
  $pipes = array();
  $process = @proc_open(implode(' ', array_map('escapeshellarg', $command)), array(
    0 => array('pipe', 'r'),
    1 => array('pipe', 'w'),
    2 => array('pipe', 'w'),
  ), $pipes);
  if (!is_resource($process)) {
    $result['stderr'] = 'Could not start ' . $command[0];
    return $result;
  }
  fclose($pipes[0]);
  stream_set_blocking($pipes[1], FALSE);
  stream_set_blocking($pipes[2], FALSE);
  $open = array(1 => $pipes[1], 2 => $pipes[2]);
  while ($open) {
    $left = $timeout - (microtime(TRUE) - $start);
    if ($left <= 0) {
      $result['timed_out'] = TRUE;
      proc_terminate($process, 9);
      break;
    }
    $read = array_values($open);
    $write = $except = NULL;
    if (stream_select($read, $write, $except, (int) ceil($left)) === FALSE) {
      break;
    }
    foreach ($read as $stream) {
      $key = $stream === $pipes[1] ? 'stdout' : 'stderr';
      $chunk = fread($stream, 65536);
      if ($chunk === '' || $chunk === FALSE) {
        if (feof($stream)) {
          unset($open[$key == 'stdout' ? 1 : 2]);
        }
        continue;
      }
      $result[$key] .= $chunk;
    }
  }
  fclose($pipes[1]);
  fclose($pipes[2]);
  $status = proc_close($process);
  $result['status'] = $result['timed_out'] ? -1 : $status;
  $result['seconds'] = microtime(TRUE) - $start;
  return $result;
}

/**
 * The version of a Dart Sass program, or NULL if it doesn't run.
 */
function bootstrap_native_update_sass_version($sass) {
  if (!is_file($sass) || !is_executable($sass)) {
    return NULL;
  }
  $run = bootstrap_native_update_run(array($sass, '--version'), 15);
  $version = trim($run['stdout']);
  return $run['status'] === 0 && preg_match('/^\d+\.\d+\.\d+\S*$/', $version) ? $version : NULL;
}

/**
 * Finds a Dart Sass program that runs.
 *
 * @param array $paths
 *   Places to try, in order: paths to the sass program or to the folder it
 *   is in.
 *
 * @return array|null
 *   Keys path and version, or NULL.
 */
function bootstrap_native_update_sass_find(array $paths) {
  foreach ($paths as $path) {
    if ($path === '' || $path === NULL) {
      continue;
    }
    if (is_dir($path)) {
      $path = rtrim($path, '/') . '/sass';
    }
    if ($version = bootstrap_native_update_sass_version($path)) {
      return array('path' => $path, 'version' => $version);
    }
  }
  return NULL;
}

/**
 * The folders on the PATH, each with /sass added.
 */
function bootstrap_native_update_sass_on_path() {
  $paths = array();
  foreach (explode(PATH_SEPARATOR, (string) getenv('PATH')) as $dir) {
    if ($dir !== '') {
      $paths[] = rtrim($dir, '/') . '/sass';
    }
  }
  return $paths;
}

/**
 * Compiles the theme's stylesheet.
 *
 * @param string $sass
 *   The Dart Sass program.
 * @param string $entry
 *   The theme's scss/bootstrap_native.scss.
 * @param string $load_path
 *   The folder holding bootstrap/scss (the theme's vendor/, or a release).
 * @param bool $dev
 *   Readable output instead of compressed.
 *
 * @return array
 *   Keys: css (NULL on failure), error, seconds.
 */
function bootstrap_native_update_compile($sass, $entry, $load_path, $dev = FALSE) {
  $run = bootstrap_native_update_run(array(
    $sass,
    '--no-source-map',
    '--style=' . ($dev ? 'expanded' : 'compressed'),
    '--quiet-deps',
    '--load-path=' . $load_path,
    $entry,
  ), BOOTSTRAP_NATIVE_UPDATE_SASS_TIMEOUT);
  $error = '';
  if ($run['timed_out']) {
    $error = 'Sass took longer than ' . BOOTSTRAP_NATIVE_UPDATE_SASS_TIMEOUT . ' seconds and was stopped.';
  }
  elseif ($run['status'] !== 0 || $run['stdout'] === '') {
    $error = trim($run['stderr']) !== '' ? trim($run['stderr']) : 'Sass stopped with exit code ' . $run['status'] . '.';
  }
  return array(
    'css' => $error ? NULL : $run['stdout'],
    'error' => $error,
    'seconds' => $run['seconds'],
  );
}

/**
 * Prefixes custom properties and renames .menu in compiled CSS.
 *
 * @return array
 *   Keys: css, properties (prefixed), menus (renamed).
 */
function bootstrap_native_update_postprocess_css($css) {
  $css = preg_replace('/(?<![\w-])--(?!bs-|bn-)([a-zA-Z_][\w-]*)/', '--bs-$1', $css, -1, $properties);
  $css = preg_replace('/(?<![\w.-])\.menu(?![\w-])/', '.bs-menu', $css, -1, $menus);
  return array('css' => $css, 'properties' => $properties, 'menus' => $menus);
}

/**
 * Renames .menu in Bootstrap's script (inside strings only).
 *
 * After a quote, "(", " ", ">" or ",": none of those can come before
 * ".menu" in JavaScript code itself, where it would be a property like
 * config.menu.
 *
 * @return array
 *   Keys: js, menus.
 */
function bootstrap_native_update_postprocess_js($js) {
  $js = preg_replace('/(?<=["\'`( >,])\.menu(?![\w-])/', '.bs-menu', $js, -1, $menus);
  return array('js' => $js, 'menus' => $menus);
}

/**
 * Builds the theme against a Bootstrap release.
 *
 * @param string $sass
 *   The Dart Sass program.
 * @param string $theme_dir
 *   The theme's folder.
 * @param string $release_dir
 *   The folder holding bootstrap/ (scss/ and dist/js/).
 * @param bool $dev
 *   Readable CSS instead of compressed.
 *
 * @return array
 *   Keys: css, js (NULL on failure), error, seconds, properties,
 *   css_menus, js_menus, undefined (custom properties used but defined
 *   nowhere).
 */
function bootstrap_native_update_build($sass, $theme_dir, $release_dir, $dev = FALSE) {
  $build = array('css' => NULL, 'js' => NULL, 'error' => '', 'seconds' => 0, 'properties' => 0, 'css_menus' => 0, 'js_menus' => 0, 'undefined' => array());
  $compiled = bootstrap_native_update_compile($sass, $theme_dir . '/scss/bootstrap_native.scss', $release_dir, $dev);
  $build['seconds'] = $compiled['seconds'];
  if ($compiled['css'] === NULL) {
    $build['error'] = $compiled['error'];
    return $build;
  }
  $bundle = $release_dir . '/bootstrap/dist/js/bootstrap.bundle.min.js';
  if (!is_file($bundle)) {
    $build['error'] = 'The release has no dist/js/bootstrap.bundle.min.js.';
    return $build;
  }
  $css = bootstrap_native_update_postprocess_css($compiled['css']);
  $js = bootstrap_native_update_postprocess_js(file_get_contents($bundle));
  $build['css'] = $css['css'];
  $build['js'] = $js['js'];
  $build['properties'] = $css['properties'];
  $build['css_menus'] = $css['menus'];
  $build['js_menus'] = $js['menus'];
  $build['undefined'] = bootstrap_native_update_undefined_properties($css['css'], $release_dir . '/bootstrap/scss');
  return $build;
}

/**
 * Custom properties the CSS uses but neither it nor Bootstrap defines.
 *
 * A typo or a missed prefix would silently fall back to nothing. Properties
 * set at runtime (by Bootstrap's script or the color settings) appear in
 * Bootstrap's sources, so those count as defined.
 */
function bootstrap_native_update_undefined_properties($css, $scss_dir) {
  preg_match_all('/(--bs-[\w-]+)\s*:/', $css, $m);
  $defined = array_flip($m[1]);
  preg_match_all('/var\((--bs-[\w-]+)/', $css, $m);
  $used = array_unique($m[1]);
  $known = array();
  if (is_dir($scss_dir)) {
    $files = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($scss_dir, FilesystemIterator::SKIP_DOTS));
    foreach ($files as $file) {
      if (substr($file->getFilename(), -5) == '.scss') {
        preg_match_all('/--([a-z][\w-]*)/', file_get_contents($file->getPathname()), $m);
        foreach ($m[1] as $name) {
          $known['--bs-' . $name] = TRUE;
        }
      }
    }
  }
  $missing = array();
  foreach ($used as $name) {
    if (!isset($defined[$name]) && !isset($known[$name])) {
      $missing[] = $name;
    }
  }
  sort($missing);
  return $missing;
}
