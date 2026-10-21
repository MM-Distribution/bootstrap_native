<?php

/**
 * @file
 * Bootstrap releases: the registry, downloads, checks, apply and roll back.
 *
 * Where things live:
 * - The release the theme ships with: themes/bootstrap_native/vendor (never
 *   removed; the fallback).
 * - Downloaded releases: public://bootstrap-native/releases/<version>/
 *   bootstrap (Bootstrap's scss/, dist/js, LICENSE, package.json). Public
 *   files are per site and writable everywhere Drupal runs.
 * - Builds of them: public://bootstrap-native/builds/<version>/
 *   (bootstrap_native.css, bootstrap.bundle.min.js).
 *
 * State (variables): bootstrap_native_update_active ('' = the shipped
 * release), _previous, _versions (one record per downloaded release),
 * _releases and _checked (the registry, last fetched).
 */

/**
 * Inactive releases are removed this long after they stop being active.
 */
define('BOOTSTRAP_NATIVE_UPDATE_KEEP', 30 * 86400);

/**
 * Largest release archive accepted (6.0.0-alpha.1: 1.8 MB).
 */
define('BOOTSTRAP_NATIVE_UPDATE_MAX_ARCHIVE', 30 * 1048576);

/**
 * The theme's folder.
 */
function bootstrap_native_update_theme_dir() {
  return DRUPAL_ROOT . '/' . drupal_get_path('module', 'bootstrap_native_update') . '/themes/bootstrap_native';
}

/**
 * The Bootstrap release the theme ships with.
 */
function bootstrap_native_update_shipped_version() {
  $file = bootstrap_native_update_theme_dir() . '/vendor/bootstrap/VERSION';
  return is_file($file) ? trim(file_get_contents($file)) : '';
}

/**
 * The release the site uses ('' = the shipped one).
 */
function bootstrap_native_update_active() {
  $active = variable_get('bootstrap_native_update_active', '');
  $versions = bootstrap_native_update_versions();
  return $active !== '' && !empty($versions[$active]['build']) ? $active : '';
}

/**
 * The version number the site uses.
 */
function bootstrap_native_update_active_version() {
  $active = bootstrap_native_update_active();
  return $active === '' ? bootstrap_native_update_shipped_version() : $active;
}

/**
 * Records of downloaded releases, keyed by version.
 *
 * Each: downloaded (timestamp), uid, inactive_since (timestamp or NULL),
 * build (meta of the build, or NULL), check (result or NULL), error.
 */
function bootstrap_native_update_versions() {
  return variable_get('bootstrap_native_update_versions', array());
}

/**
 * Saves one release record (NULL removes it).
 */
function bootstrap_native_update_save_version($version, $record) {
  $versions = bootstrap_native_update_versions();
  if ($record === NULL) {
    unset($versions[$version]);
  }
  else {
    $versions[$version] = $record + (isset($versions[$version]) ? $versions[$version] : array());
  }
  uksort($versions, function ($a, $b) {
    return version_compare($b, $a);
  });
  variable_set('bootstrap_native_update_versions', $versions);
}

/**
 * Where a release's files go.
 */
function bootstrap_native_update_release_uri($version) {
  return 'public://bootstrap-native/releases/' . $version;
}

/**
 * Where a release's build goes.
 */
function bootstrap_native_update_build_uri($version) {
  return 'public://bootstrap-native/builds/' . $version;
}

/**
 * A version string safe to use in paths (and that looks like a release).
 */
function bootstrap_native_update_valid_version($version) {
  return is_string($version) && preg_match('/^6\.\d+\.\d+(-[0-9A-Za-z.-]+)?$/', $version) && strpos($version, '..') === FALSE;
}

/**
 * The Dart Sass program to use.
 *
 * In order: the path setting, sites/all/libraries/dart-sass, the PATH.
 *
 * @return array|null
 *   Keys path and version, or NULL.
 */
function bootstrap_native_update_sass() {
  $sass = &drupal_static(__FUNCTION__, FALSE);
  if ($sass === FALSE) {
    module_load_include('inc', 'bootstrap_native_update', 'includes/build');
    $sass = bootstrap_native_update_sass_find(array_merge(
      array(variable_get('bootstrap_native_update_sass_path', ''), DRUPAL_ROOT . '/sites/all/libraries/dart-sass'),
      bootstrap_native_update_sass_on_path()
    ));
  }
  return $sass;
}

/**
 * Fetches the list of Bootstrap 6 releases from npm.
 *
 * @return string
 *   '' on success, else what went wrong.
 */
function bootstrap_native_update_refresh_releases() {
  module_load_include('inc', 'bootstrap_native_update', 'includes/build');
  module_load_include('inc', 'bootstrap_native_update', 'includes/checks');
  $registry = variable_get('bootstrap_native_update_registry', 'https://registry.npmjs.org/bootstrap');
  $response = drupal_http_request($registry, array('timeout' => 20, 'headers' => array('Accept' => 'application/json')));
  variable_set('bootstrap_native_update_checked', REQUEST_TIME);
  if ($response->code != 200 || !($doc = json_decode($response->data, TRUE)) || !is_array($doc)) {
    $error = t('Could not read the release list from @url (@code).', array('@url' => $registry, '@code' => isset($response->error) ? $response->error : $response->code));
    watchdog('bootstrap_native', $error, array(), WATCHDOG_WARNING);
    return $error;
  }
  variable_set('bootstrap_native_update_releases', bootstrap_native_update_parse_registry($doc, $registry));
  return '';
}

/**
 * Known releases, newest first; pre-releases only if offered.
 */
function bootstrap_native_update_releases() {
  $releases = variable_get('bootstrap_native_update_releases', array());
  if (!variable_get('bootstrap_native_update_prereleases', 1)) {
    $releases = array_filter($releases, function ($release) {
      return !$release['prerelease'];
    });
  }
  return $releases;
}

/**
 * The newest release, if it is newer than the one in use.
 */
function bootstrap_native_update_available() {
  $releases = bootstrap_native_update_releases();
  $newest = reset($releases);
  return $newest && version_compare($newest['version'], bootstrap_native_update_active_version(), '>') ? $newest : NULL;
}

/**
 * The steps of a check, in order, with their labels.
 */
function bootstrap_native_update_check_steps() {
  return array(
    'download' => t('Download from npm'),
    'verify' => t("Verify npm's checksum"),
    'unpack' => t('Unpack'),
    'build' => t('Build a test stylesheet with the theme'),
    'check' => t('Check for breaks'),
  );
}

/**
 * Runs one step of a check.
 *
 * @return string
 *   '' when it worked, else the problem (the step's files are removed).
 */
function bootstrap_native_update_check_step($step, $version) {
  $error = _bootstrap_native_update_check_step($step, $version);
  // A refused download leaves nothing behind (unless that release was
  // applied before: it stays for rolling back).
  if ($error && in_array($step, array('verify', 'unpack'))) {
    $versions = bootstrap_native_update_versions();
    if (isset($versions[$version]) && empty($versions[$version]['applied'])) {
      bootstrap_native_update_discard($version);
    }
  }
  return $error;
}

/**
 * Does the work of bootstrap_native_update_check_step().
 */
function _bootstrap_native_update_check_step($step, $version) {
  module_load_include('inc', 'bootstrap_native_update', 'includes/build');
  module_load_include('inc', 'bootstrap_native_update', 'includes/checks');
  $releases = variable_get('bootstrap_native_update_releases', array());
  if (!bootstrap_native_update_valid_version($version) || !isset($releases[$version])) {
    return t('@version is not a known Bootstrap 6 release.', array('@version' => $version));
  }
  $release = $releases[$version];
  $archive = 'temporary://bootstrap-native-' . $version . '.tgz';
  $dir = bootstrap_native_update_release_uri($version);

  switch ($step) {
    case 'download':
      $response = drupal_http_request($release['tarball'], array('timeout' => 60));
      if ($response->code != 200 || empty($response->data)) {
        return t('The download failed (@code).', array('@code' => isset($response->error) ? $response->error : $response->code));
      }
      if (strlen($response->data) > BOOTSTRAP_NATIVE_UPDATE_MAX_ARCHIVE) {
        return t('The download is larger than a Bootstrap release should be.');
      }
      if (!file_unmanaged_save_data($response->data, $archive, FILE_EXISTS_REPLACE)) {
        return t('The download could not be saved to the temporary folder.');
      }
      $versions = bootstrap_native_update_versions();
      bootstrap_native_update_save_version($version, array(
        'downloaded' => REQUEST_TIME,
        'uid' => $GLOBALS['user']->uid,
        'inactive_since' => REQUEST_TIME,
        'build' => NULL,
        'check' => NULL,
        'error' => '',
      ) + (isset($versions[$version]) ? $versions[$version] : array()));
      return '';

    case 'verify':
      if (!bootstrap_native_update_integrity_matches(drupal_realpath($archive), $release['integrity'])) {
        file_unmanaged_delete($archive);
        return t("The download doesn't match npm's checksum, so it was removed. Nothing was changed.");
      }
      return '';

    case 'unpack':
      require_once DRUPAL_ROOT . '/modules/system/system.tar.inc';
      $tar = new Archive_Tar(drupal_realpath($archive), 'gz');
      try {
        $entries = $tar->listContent();
        $problem = bootstrap_native_update_archive_problem(is_array($entries) ? $entries : array(), 'package/');
      }
      catch (Exception $e) {
        // Core's tar reader refuses some paths itself (../ for one).
        $problem = $e->getMessage();
      }
      if ($problem) {
        file_unmanaged_delete($archive);
        return t('The download was refused: @problem', array('@problem' => $problem));
      }
      // Only what a build needs.
      $wanted = array();
      foreach ($entries as $entry) {
        $name = $entry['filename'];
        if ($entry['typeflag'] != '5' && (strpos($name, 'package/scss/') === 0 || in_array($name, array('package/dist/js/bootstrap.bundle.min.js', 'package/LICENSE', 'package/package.json')))) {
          $wanted[] = $name;
        }
      }
      file_unmanaged_delete_recursive($dir);
      $target = $dir . '/bootstrap';
      if (!file_prepare_directory($target, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS) || !$tar->extractList($wanted, drupal_realpath($target), 'package/', FALSE, FALSE)) {
        file_unmanaged_delete_recursive($dir);
        file_unmanaged_delete($archive);
        return t('The release could not be unpacked into the files folder.');
      }
      file_unmanaged_delete($archive);
      $package = json_decode((string) @file_get_contents($target . '/package.json'), TRUE);
      if (!is_dir($target . '/scss') || empty($package['version']) || $package['version'] !== $version) {
        file_unmanaged_delete_recursive($dir);
        return t('The download is not Bootstrap @version (no scss folder, or another version in package.json).', array('@version' => $version));
      }
      return '';

    case 'build':
      $sass = bootstrap_native_update_sass();
      if (!$sass) {
        return t('No Sass compiler found.');
      }
      $build = bootstrap_native_update_build($sass['path'], bootstrap_native_update_theme_dir(), drupal_realpath($dir));
      if ($build['error']) {
        bootstrap_native_update_save_version($version, array('build' => NULL, 'check' => NULL, 'build_error' => $build['error']));
        // Not a failed step: the result page explains it.
        return '';
      }
      $error = bootstrap_native_update_save_build($version, $build);
      return $error;

    case 'check':
      $versions = bootstrap_native_update_versions();
      $record = isset($versions[$version]) ? $versions[$version] : array();
      $new = array('css' => NULL, 'error' => isset($record['build_error']) ? $record['build_error'] : '');
      if (!empty($record['build'])) {
        $new = bootstrap_native_update_load_build($version);
      }
      $result = bootstrap_native_update_run_checks(bootstrap_native_update_load_build(bootstrap_native_update_active()), $new, bootstrap_native_update_check_themes(), bootstrap_native_update_color_properties());
      $result['checked'] = REQUEST_TIME;
      $result['against'] = bootstrap_native_update_active_version();
      bootstrap_native_update_save_version($version, array('check' => $result, 'error' => ''));
      return '';
  }
  return t('Unknown step.');
}

/**
 * Writes a build to the files folder and records it.
 *
 * @return string
 *   '' or the problem.
 */
function bootstrap_native_update_save_build($version, array $build) {
  $dir = bootstrap_native_update_build_uri($version);
  file_unmanaged_delete_recursive($dir);
  if (!file_prepare_directory($dir, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS)
    || !file_unmanaged_save_data($build['css'], $dir . '/bootstrap_native.css', FILE_EXISTS_REPLACE)
    || !file_unmanaged_save_data($build['js'], $dir . '/bootstrap.bundle.min.js', FILE_EXISTS_REPLACE)) {
    return t('The build could not be saved to the files folder.');
  }
  bootstrap_native_update_save_version($version, array(
    'build' => array(
      'properties' => $build['properties'],
      'css_menus' => $build['css_menus'],
      'js_menus' => $build['js_menus'],
      'undefined' => $build['undefined'],
      'size' => strlen($build['css']),
      'seconds' => $build['seconds'],
      'theme_hash' => bootstrap_native_update_theme_hash(),
      // A new name for every build, so browsers don't keep an old one.
      'stamp' => substr(md5($build['css'] . $build['js']), 0, 8),
    ),
    'build_error' => '',
  ));
  return '';
}

/**
 * A build, ready for the checks ('' = the shipped one).
 */
function bootstrap_native_update_load_build($version) {
  module_load_include('inc', 'bootstrap_native_update', 'includes/build');
  $theme = bootstrap_native_update_theme_dir();
  if ($version === '') {
    $css = (string) file_get_contents($theme . '/css/bootstrap_native.css');
    $js = (string) file_get_contents($theme . '/js/bootstrap.bundle.min.js');
    return array(
      'css' => $css,
      'js' => $js,
      'css_menus' => substr_count($css, '.bs-menu'),
      'js_menus' => substr_count($js, '.bs-menu'),
      'undefined' => bootstrap_native_update_undefined_properties($css, $theme . '/vendor/bootstrap/scss'),
      'scss_dir' => $theme . '/vendor/bootstrap/scss',
    );
  }
  $versions = bootstrap_native_update_versions();
  $meta = $versions[$version]['build'];
  $dir = drupal_realpath(bootstrap_native_update_build_uri($version));
  return array(
    'css' => (string) @file_get_contents($dir . '/bootstrap_native.css'),
    'js' => (string) @file_get_contents($dir . '/bootstrap.bundle.min.js'),
    'css_menus' => $meta['css_menus'],
    'js_menus' => $meta['js_menus'],
    'undefined' => $meta['undefined'],
    'scss_dir' => drupal_realpath(bootstrap_native_update_release_uri($version)) . '/bootstrap/scss',
  );
}

/**
 * Themes the checks look at: Bootstrap Native and its enabled sub-themes.
 *
 * @return array
 *   Absolute folder, keyed by theme name.
 */
function bootstrap_native_update_check_themes() {
  $themes = array('bootstrap_native' => bootstrap_native_update_theme_dir());
  $list = list_themes();
  foreach (array_keys(bootstrap_native_update_themes_in_use()) as $name) {
    if ($name != 'bootstrap_native') {
      $themes[$name] = DRUPAL_ROOT . '/' . dirname($list[$name]->filename);
    }
  }
  return $themes;
}

/**
 * Custom properties the color settings set (all colors, light and dark).
 */
function bootstrap_native_update_color_properties() {
  require_once bootstrap_native_update_theme_dir() . '/includes/color.inc';
  $properties = array();
  foreach (array('#ffffff', '#1a1a2e') as $base) {
    $tokens = bootstrap_native_color_tokens(array(
      'primary' => '#0d6efd',
      'secondary' => '#6610f2',
      'base' => $base,
      'text' => '',
      'link' => '#0d6efd',
      'link_hover' => '#0a58ca',
    ));
    $properties += array_flip(array_keys($tokens['vars']));
  }
  return array_keys($properties);
}

/**
 * Hash of what a build depends on in the theme (its SCSS and the build).
 *
 * A build made before the theme changed (a git pull) is out of date.
 */
function bootstrap_native_update_theme_hash() {
  $files = glob(bootstrap_native_update_theme_dir() . '/scss/*.scss');
  $files[] = DRUPAL_ROOT . '/' . drupal_get_path('module', 'bootstrap_native_update') . '/includes/build.inc';
  sort($files);
  $hash = '';
  foreach ($files as $file) {
    $hash .= md5_file($file);
  }
  return md5($hash);
}

/**
 * Switches the site to a release ('' = the shipped one).
 */
function bootstrap_native_update_activate($version) {
  $old = bootstrap_native_update_active();
  if ($old === $version) {
    return;
  }
  if ($old !== '') {
    bootstrap_native_update_save_version($old, array('inactive_since' => REQUEST_TIME));
  }
  if ($version !== '') {
    bootstrap_native_update_save_version($version, array('inactive_since' => NULL));
  }
  variable_set('bootstrap_native_update_previous', $old);
  variable_set('bootstrap_native_update_active', $version);
  variable_del('bootstrap_native_update_fallback');
  bootstrap_native_update_flush_assets();
  watchdog('bootstrap_native', 'Bootstrap @new is now active (was @old).', array(
    '@new' => $version === '' ? bootstrap_native_update_shipped_version() : $version,
    '@old' => $old === '' ? bootstrap_native_update_shipped_version() : $old,
  ));
}

/**
 * New stylesheet and script names for every page.
 */
function bootstrap_native_update_flush_assets() {
  _drupal_flush_css_js();
  drupal_clear_css_cache();
  drupal_clear_js_cache();
  cache_clear_all('*', 'cache_page', TRUE);
}

/**
 * Removes a downloaded release (never the active one).
 */
function bootstrap_native_update_discard($version) {
  if (!bootstrap_native_update_valid_version($version) || $version === bootstrap_native_update_active()) {
    return;
  }
  file_unmanaged_delete_recursive(bootstrap_native_update_release_uri($version));
  file_unmanaged_delete_recursive(bootstrap_native_update_build_uri($version));
  bootstrap_native_update_save_version($version, NULL);
  if (variable_get('bootstrap_native_update_previous', '') === $version) {
    variable_set('bootstrap_native_update_previous', '');
  }
}

/**
 * Removes releases inactive for longer than BOOTSTRAP_NATIVE_UPDATE_KEEP.
 */
function bootstrap_native_update_cleanup() {
  $active = bootstrap_native_update_active();
  foreach (bootstrap_native_update_versions() as $version => $record) {
    if ($version !== $active && !empty($record['inactive_since']) && $record['inactive_since'] < REQUEST_TIME - BOOTSTRAP_NATIVE_UPDATE_KEEP) {
      bootstrap_native_update_discard($version);
    }
  }
}

/**
 * Rebuilds the active release when the theme changed since its build.
 *
 * If that can't be done, the site goes back to the shipped release (which
 * always matches the theme) and the status report says why.
 */
function bootstrap_native_update_rebuild_if_stale() {
  $active = bootstrap_native_update_active();
  if ($active === '') {
    return;
  }
  $versions = bootstrap_native_update_versions();
  if ($versions[$active]['build']['theme_hash'] === bootstrap_native_update_theme_hash()) {
    return;
  }
  module_load_include('inc', 'bootstrap_native_update', 'includes/build');
  $sass = bootstrap_native_update_sass();
  $error = $sass ? '' : t('No Sass compiler found.');
  if ($sass) {
    $build = bootstrap_native_update_build($sass['path'], bootstrap_native_update_theme_dir(), drupal_realpath(bootstrap_native_update_release_uri($active)));
    $error = $build['error'] ? $build['error'] : bootstrap_native_update_save_build($active, $build);
  }
  if ($error) {
    bootstrap_native_update_activate('');
    variable_set('bootstrap_native_update_fallback', array('version' => $active, 'error' => $error, 'time' => REQUEST_TIME));
    watchdog('bootstrap_native', 'Bootstrap Native changed and Bootstrap @version could not be rebuilt for it, so the site uses the shipped release again: @error', array('@version' => $active, '@error' => $error), WATCHDOG_WARNING);
  }
  else {
    bootstrap_native_update_flush_assets();
  }
}
