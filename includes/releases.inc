<?php

/**
 * @file
 * Bootstrap releases and builds: the registry, downloads, builds, checks,
 * apply and roll back.
 *
 * A build is a release built with a selection of its parts (and any Sass
 * modules sub-themes bring), named <version>~<key>: key "default" for the
 * theme's own parts, else a hash of the selection. The site uses one build
 * at a time; '' is the theme's own files (the shipped release, default
 * parts), which never needs building.
 *
 * Where things live:
 * - The shipped release: themes/bootstrap_native/vendor (never removed).
 * - Downloaded releases: public://bootstrap-native/releases/<version>/
 *   bootstrap (scss/, dist/js, LICENSE, package.json). Public files are per
 *   site and writable everywhere Drupal runs.
 * - Builds: public://bootstrap-native/builds/<build>/ (bootstrap_native.css,
 *   bootstrap.bundle.min.js).
 *
 * State (variables, bootstrap_native_update_*): active and previous (build
 * names), selection (the site's parts; NULL = the theme's), builds and
 * versions (records), releases and checked (the registry, last fetched).
 * Themes with their own or fixed parts have builds of their own: see
 * includes/themes.inc.
 */

require_once dirname(__FILE__) . '/themes.inc';

/**
 * Inactive builds and releases are removed this long after they stop being
 * active.
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
 * A version string safe to use in paths (and that looks like a release).
 */
function bootstrap_native_update_valid_version($version) {
  return is_string($version) && preg_match('/^6\.\d+\.\d+(-[0-9A-Za-z.-]+)?$/', $version) && strpos($version, '..') === FALSE;
}

/**
 * A build name safe to use in paths.
 */
function bootstrap_native_update_valid_build($build) {
  return is_string($build) && preg_match('/^(.+)~(default|[0-9a-f]{8})$/', $build, $m) && bootstrap_native_update_valid_version($m[1]);
}

/**
 * Records of downloaded releases, keyed by version.
 *
 * Each: downloaded, uid, inactive_since, error, build (the last build a
 * check made of it).
 */
function bootstrap_native_update_versions() {
  return variable_get('bootstrap_native_update_versions', array());
}

/**
 * Records of builds, keyed by build name.
 *
 * Each: version, key, input (the selection asked for; NULL = the theme's),
 * theme (the theme with own or fixed parts it was made for; NULL = the
 * site-wide choice), theme_builds (a site-wide build of a release check:
 * the builds made for those themes with it), effective (what was built),
 * sass (extra modules), created, uid, meta
 * (properties, css_menus, js_menus, undefined, size, seconds, theme_hash,
 * stamp; NULL when the build failed), error, check, applied,
 * inactive_since.
 */
function bootstrap_native_update_builds() {
  return variable_get('bootstrap_native_update_builds', array());
}

/**
 * Saves one record (NULL removes it), merged with what is there.
 */
function _bootstrap_native_update_save($variable, $key, $record) {
  $records = variable_get($variable, array());
  if ($record === NULL) {
    unset($records[$key]);
  }
  else {
    $records[$key] = $record + (isset($records[$key]) ? $records[$key] : array());
  }
  uksort($records, function ($a, $b) {
    return version_compare($b, $a) ?: strcmp($a, $b);
  });
  variable_set($variable, $records);
}

/**
 * Saves a release record.
 */
function bootstrap_native_update_save_version($version, $record) {
  _bootstrap_native_update_save('bootstrap_native_update_versions', $version, $record);
}

/**
 * Saves a build record.
 */
function bootstrap_native_update_save_build_record($build, $record) {
  _bootstrap_native_update_save('bootstrap_native_update_builds', $build, $record);
}

/**
 * The build the site uses ('' = the theme's own files).
 */
function bootstrap_native_update_active() {
  $active = variable_get('bootstrap_native_update_active', '');
  $builds = bootstrap_native_update_builds();
  return $active !== '' && !empty($builds[$active]['meta']) ? $active : '';
}

/**
 * The release version a build is of ('' = the shipped one).
 */
function bootstrap_native_update_build_version($build) {
  if ($build === '') {
    return bootstrap_native_update_shipped_version();
  }
  return substr($build, 0, strrpos($build, '~'));
}

/**
 * The release version the site uses.
 */
function bootstrap_native_update_active_version() {
  return bootstrap_native_update_build_version(bootstrap_native_update_active());
}

/**
 * The folder holding a release's bootstrap/ (a Sass load path).
 */
function bootstrap_native_update_release_dir($version) {
  if ($version === bootstrap_native_update_shipped_version()) {
    return bootstrap_native_update_theme_dir() . '/vendor';
  }
  return drupal_realpath(bootstrap_native_update_release_uri($version));
}

/**
 * Whether a release's files are here (the shipped one always is).
 */
function bootstrap_native_update_release_present($version) {
  $dir = bootstrap_native_update_release_dir($version);
  return $dir && is_dir($dir . '/bootstrap/scss');
}

/**
 * Where a downloaded release's files go.
 */
function bootstrap_native_update_release_uri($version) {
  return 'public://bootstrap-native/releases/' . $version;
}

/**
 * Where a build goes.
 */
function bootstrap_native_update_build_uri($build) {
  return 'public://bootstrap-native/builds/' . $build;
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
 * Themes built on Bootstrap Native that are enabled.
 *
 * @return array
 *   Theme objects (list_themes()), keyed by name, Bootstrap Native first.
 */
function bootstrap_native_update_theme_objects() {
  $list = list_themes();
  $themes = array();
  if (isset($list['bootstrap_native'])) {
    $themes['bootstrap_native'] = $list['bootstrap_native'];
  }
  foreach (array_keys(bootstrap_native_update_themes_in_use()) as $name) {
    $themes[$name] = $list[$name];
  }
  return $themes;
}

/**
 * What sub-themes add to a build: parts they need, Sass modules they bring.
 *
 * In a sub-theme's .info:
 *   bootstrap_native[parts][] = carousel
 *   bootstrap_native[sass][] = scss/_extras.scss
 *
 * @param array|null $names
 *   The themes whose builds these are for (each with its base themes); NULL
 *   for every enabled Bootstrap Native theme.
 *
 * @return array
 *   Keys: needed (part => theme names), sass (absolute file => theme name).
 */
function bootstrap_native_update_theme_additions($names = NULL) {
  $additions = array('needed' => array(), 'sass' => array());
  $list = list_themes();
  if ($names === NULL) {
    $names = array_keys(bootstrap_native_update_themes_in_use());
  }
  $chain = array();
  foreach ($names as $name) {
    $chain = array_merge($chain, bootstrap_native_update_theme_chain($name));
  }
  foreach (array_unique($chain) as $name) {
    $theme = $list[$name];
    if (empty($theme->info['bootstrap_native'])) {
      continue;
    }
    $own = $theme->info['bootstrap_native'];
    foreach (isset($own['parts']) ? (array) $own['parts'] : array() as $part) {
      $additions['needed'][$part][] = $theme->info['name'];
    }
    foreach (isset($own['sass']) ? (array) $own['sass'] : array() as $file) {
      $path = DRUPAL_ROOT . '/' . dirname($theme->filename) . '/' . $file;
      // Only .scss files inside the sub-theme.
      if (strpos($file, '..') === FALSE && preg_match('/\.scss$/', $file) && is_file($path)) {
        $additions['sass'][$path] = $theme->info['name'];
      }
    }
  }
  ksort($additions['sass']);
  return $additions;
}

/**
 * Everything a build of a release needs to know about its parts.
 *
 * @param string $version
 *   The release.
 * @param string|null $theme_name
 *   A theme with own or fixed parts (its build); NULL for the site-wide
 *   build (the themes that use the site-wide choice).
 *
 * @return array
 *   Keys: parts, utility_groups, default, required, needed, exclude (see
 *   bootstrap_native_update_effective_selection()), needed_by, sass.
 */
function bootstrap_native_update_parts_context($version, $theme_name = NULL) {
  module_load_include('inc', 'bootstrap_native_update', 'includes/parts');
  $theme = bootstrap_native_update_theme_dir();
  $scss = bootstrap_native_update_release_dir($version) . '/bootstrap/scss';
  $info = drupal_parse_info_file($theme . '/bootstrap_native.info');
  $additions = bootstrap_native_update_theme_additions($theme_name === NULL ? bootstrap_native_update_site_themes() : array($theme_name));
  // A folder listed (buttons) stands for its parts.
  $needed_by = array();
  foreach ($additions['needed'] as $part => $themes) {
    foreach (bootstrap_native_update_expand_part($part, $scss) as $one) {
      $needed_by[$one] = isset($needed_by[$one]) ? array_unique(array_merge($needed_by[$one], $themes)) : $themes;
    }
  }
  $additions['needed'] = $needed_by;
  return array(
    'parts' => bootstrap_native_update_release_parts($scss),
    'utility_groups' => bootstrap_native_update_utility_groups($scss),
    'default' => bootstrap_native_update_default_selection($theme, $scss),
    'required' => bootstrap_native_update_required_parts($theme, $scss),
    'needed' => array_keys($additions['needed']),
    'needed_by' => $additions['needed'],
    'exclude' => isset($info['bootstrap_native']['exclude']) ? $info['bootstrap_native']['exclude'] : array(),
    'sass' => $additions['sass'],
  );
}

/**
 * The build a release with a selection makes, without making it.
 *
 * @param string $version
 *   The release.
 * @param array|null|false $selection
 *   The parts asked for (NULL: the theme's; FALSE: what applies: the
 *   site's selection, or the theme's own or fixed parts).
 * @param string|null $theme_name
 *   A theme: its build (the site-wide one if it uses the site-wide choice).
 *
 * @return array
 *   Keys: build (name; '' when it is the theme's own files), effective,
 *   sass, input, theme (NULL for the site-wide build).
 */
function bootstrap_native_update_plan_build($version, $selection = FALSE, $theme_name = NULL) {
  $mode = $theme_name === NULL ? 'site' : bootstrap_native_update_parts_mode($theme_name);
  if ($mode == 'site') {
    $theme_name = NULL;
  }
  if ($mode == 'fixed') {
    // The .info decides, whatever was asked.
    $selection = bootstrap_native_update_fixed_selection($theme_name);
  }
  elseif ($selection === FALSE) {
    $selection = $mode == 'own' ? bootstrap_native_update_own_selection($theme_name) : variable_get('bootstrap_native_update_selection', NULL);
  }
  $context = bootstrap_native_update_parts_context($version, $theme_name);
  $effective = bootstrap_native_update_effective_selection($selection, $context);
  $default = bootstrap_native_update_effective_selection(NULL, array('needed' => array()) + $context);
  $sass = array_keys($context['sass']);
  $key = bootstrap_native_update_selection_key($effective, $default, $sass);
  $build = $key == 'default' && $version === bootstrap_native_update_shipped_version() ? '' : $version . '~' . $key;
  return array('build' => $build, 'effective' => $effective, 'sass' => $sass, 'input' => $selection, 'theme' => $theme_name);
}

/**
 * Makes a build (compiles it and records it).
 *
 * @return array
 *   Keys: build (name), error ('' when built; else the problem, recorded on
 *   the build so the checks report it).
 */
function bootstrap_native_update_make_build($version, $selection = FALSE, $theme_name = NULL) {
  module_load_include('inc', 'bootstrap_native_update', 'includes/build');
  module_load_include('inc', 'bootstrap_native_update', 'includes/parts');
  $plan = bootstrap_native_update_plan_build($version, $selection, $theme_name);
  if ($plan['build'] === '') {
    return array('build' => '', 'error' => '');
  }
  $sass = bootstrap_native_update_sass();
  if (!$sass) {
    return array('build' => $plan['build'], 'error' => t('No Sass compiler found.'));
  }
  if (!bootstrap_native_update_release_present($version)) {
    return array('build' => $plan['build'], 'error' => t("Bootstrap @version's files are missing; check it again.", array('@version' => $version)));
  }

  // The generated entry, in a folder of its own (first on the load path).
  $context = bootstrap_native_update_parts_context($version, $plan['theme']);
  $tmp = file_directory_temp() . '/bootstrap-native-build-' . getmypid() . '-' . mt_rand();
  mkdir($tmp, 0700);
  $load_paths = array($tmp, bootstrap_native_update_release_dir($version), bootstrap_native_update_theme_dir() . '/scss');
  $modules = array();
  foreach ($plan['sass'] as $i => $file) {
    // Each sub-theme module under its own name, so two _extras.scss don't
    // collide.
    $name = 'bnu-extra-' . $i;
    $url = 'file://' . implode('/', array_map('rawurlencode', explode('/', $file)));
    file_put_contents($tmp . '/_' . $name . '.scss', '@forward "' . $url . "\";\n");
    $modules[] = $name;
  }
  foreach (bootstrap_native_update_entry_files($plan['effective'], $context['utility_groups'], $modules) as $name => $text) {
    file_put_contents($tmp . '/' . $name, $text);
  }
  $build = bootstrap_native_update_build($sass['path'], bootstrap_native_update_theme_dir(), $load_paths, FALSE, $tmp . '/bootstrap_native.scss');
  file_unmanaged_delete_recursive($tmp);

  $builds = bootstrap_native_update_builds();
  $record = array(
    'version' => $version,
    'key' => substr($plan['build'], strrpos($plan['build'], '~') + 1),
    'input' => $plan['input'],
    'effective' => $plan['effective'],
    'sass' => $plan['sass'],
    'created' => REQUEST_TIME,
    'uid' => $GLOBALS['user']->uid,
    'theme' => $plan['theme'],
    'inactive_since' => isset(bootstrap_native_update_builds_in_use()[$plan['build']]) ? NULL : REQUEST_TIME,
    'check' => NULL,
    'error' => $build['error'],
    'meta' => NULL,
  );
  if (isset($builds[$plan['build']]['applied'])) {
    $record['applied'] = $builds[$plan['build']]['applied'];
  }
  // The same parts for another theme (or the site-wide choice): one build,
  // and it keeps what it was first made for.
  $first = isset($builds[$plan['build']]) ? $builds[$plan['build']] + array('theme' => NULL) : NULL;
  if ($first && !empty($first['meta']) && $first['theme'] !== $plan['theme']) {
    $record['input'] = $builds[$plan['build']]['input'];
    $record['theme'] = $builds[$plan['build']]['theme'];
  }
  if (!$build['error']) {
    $dir = bootstrap_native_update_build_uri($plan['build']);
    file_unmanaged_delete_recursive($dir);
    if (!file_prepare_directory($dir, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS)
      || !file_unmanaged_save_data($build['css'], $dir . '/bootstrap_native.css', FILE_EXISTS_REPLACE)
      || !file_unmanaged_save_data($build['js'], $dir . '/bootstrap.bundle.min.js', FILE_EXISTS_REPLACE)) {
      return array('build' => $plan['build'], 'error' => t('The build could not be saved to the files folder.'));
    }
    $record['meta'] = array(
      'properties' => $build['properties'],
      'css_menus' => $build['css_menus'],
      'js_menus' => $build['js_menus'],
      'undefined' => $build['undefined'],
      'size' => strlen($build['css']),
      'seconds' => $build['seconds'],
      'theme_hash' => bootstrap_native_update_theme_hash(),
      // A new name for every build, so browsers don't keep an old one.
      'stamp' => substr(md5($build['css'] . $build['js']), 0, 8),
    );
  }
  bootstrap_native_update_save_build_record($plan['build'], $record);
  return array('build' => $plan['build'], 'error' => $build['error']);
}

/**
 * A build, ready for the checks ('' = the theme's own files).
 */
function bootstrap_native_update_load_build($build) {
  module_load_include('inc', 'bootstrap_native_update', 'includes/build');
  $theme = bootstrap_native_update_theme_dir();
  if ($build === '') {
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
  $builds = bootstrap_native_update_builds();
  $record = $builds[$build];
  if (empty($record['meta'])) {
    return array('css' => NULL, 'error' => $record['error']);
  }
  $dir = drupal_realpath(bootstrap_native_update_build_uri($build));
  return array(
    'css' => (string) @file_get_contents($dir . '/bootstrap_native.css'),
    'js' => (string) @file_get_contents($dir . '/bootstrap.bundle.min.js'),
    'css_menus' => $record['meta']['css_menus'],
    'js_menus' => $record['meta']['js_menus'],
    'undefined' => $record['meta']['undefined'],
    'scss_dir' => bootstrap_native_update_release_dir($record['version']) . '/bootstrap/scss',
  );
}

/**
 * Checks a build against the one in use, and records the result.
 *
 * @param string $build
 *   The build.
 * @param string|null $against
 *   The build to compare with (NULL: the site-wide one in use).
 * @param array|null $themes
 *   The theme folders whose templates count (NULL: Bootstrap Native and
 *   the themes using the site-wide choice).
 */
function bootstrap_native_update_check_build($build, $against = NULL, $themes = NULL) {
  module_load_include('inc', 'bootstrap_native_update', 'includes/build');
  module_load_include('inc', 'bootstrap_native_update', 'includes/checks');
  $active = $against === NULL ? bootstrap_native_update_active() : $against;
  $result = bootstrap_native_update_run_checks(bootstrap_native_update_load_build($active), bootstrap_native_update_load_build($build), $themes === NULL ? bootstrap_native_update_check_themes_for() : $themes, bootstrap_native_update_color_properties());
  if ($result['verdict'] != 'failed') {
    $builds = bootstrap_native_update_builds();
    foreach (bootstrap_native_update_missing_color_names($builds[$build]['version']) as $row) {
      // Before the size row (always last).
      array_splice($result['rows'], -1, 0, array($row));
      if ($result['verdict'] == 'passed') {
        $result['verdict'] = 'check';
      }
    }
  }
  $result['checked'] = REQUEST_TIME;
  $result['against'] = $active;
  bootstrap_native_update_save_build_record($build, array('check' => $result));
  return $result;
}

/**
 * The steps of a release check, in order, with their labels.
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
 * Runs one step of a release check.
 *
 * @return string
 *   '' when it worked, else the problem.
 */
function bootstrap_native_update_check_step($step, $version) {
  $error = _bootstrap_native_update_check_step($step, $version);
  // A refused download leaves nothing behind (unless a build of that release
  // was applied before: it stays for rolling back).
  if ($error && in_array($step, array('verify', 'unpack'))) {
    bootstrap_native_update_discard_release($version, TRUE);
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
  $shipped = $version === bootstrap_native_update_shipped_version();
  if ($shipped && in_array($step, array('download', 'verify', 'unpack'))) {
    // The theme has this one: nothing to download.
    return '';
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
      bootstrap_native_update_save_version($version, array(
        'downloaded' => REQUEST_TIME,
        'uid' => $GLOBALS['user']->uid,
        'inactive_since' => REQUEST_TIME,
        'error' => '',
      ));
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
      if (!bootstrap_native_update_sass()) {
        return t('No Sass compiler found.');
      }
      // A failed build is not a failed step: the result page explains it.
      $made = bootstrap_native_update_make_build($version);
      if ($made['build'] === '') {
        return t("@version with the theme's parts is the theme's own files: use Roll back to this version under Installed versions.", array('@version' => $version));
      }
      if (!$shipped) {
        bootstrap_native_update_save_version($version, array('build' => $made['build']));
      }
      // Themes with own or fixed parts: a build each, with this release.
      bootstrap_native_update_save_build_record($made['build'], array('theme_builds' => bootstrap_native_update_make_theme_builds($version)));
      variable_set('bootstrap_native_update_last_build', $made['build']);
      return '';

    case 'check':
      $build = variable_get('bootstrap_native_update_last_build', '');
      $builds = bootstrap_native_update_builds();
      if ($build === '' || !isset($builds[$build]) || $builds[$build]['version'] !== $version) {
        return t('Nothing was built.');
      }
      bootstrap_native_update_check_build($build);
      bootstrap_native_update_check_theme_builds($build);
      if (!$shipped) {
        bootstrap_native_update_save_version($version, array('error' => ''));
      }
      return '';
  }
  return t('Unknown step.');
}

/**
 * Color settings that name a Bootstrap color the release doesn't have.
 *
 * Such a color would fall back to Bootstrap's own (as if empty).
 *
 * @return array
 *   Check rows (status check, code color_name_missing).
 */
function bootstrap_native_update_missing_color_names($version) {
  require_once bootstrap_native_update_theme_dir() . '/includes/color.inc';
  $palette = bootstrap_native_color_palette_parse((string) @file_get_contents(bootstrap_native_update_release_dir($version) . '/bootstrap/scss/_colors.scss'));
  $before = bootstrap_native_color_palette();
  bootstrap_native_color_palette($palette);
  $rows = array();
  foreach (bootstrap_native_update_theme_objects() as $name => $theme) {
    foreach (array('primary', 'secondary', 'base', 'text', 'link', 'link_hover') as $key) {
      $value = theme_get_setting('bootstrap_native_color_' . $key, $name);
      if (bootstrap_native_color_is_name($value) && !bootstrap_native_color_named($value)) {
        $rows[] = array('check' => 'colors', 'status' => 'check', 'code' => 'color_name_missing', 'values' => array('name' => $value, 'setting' => $key, 'theme' => $theme->info['name']));
      }
    }
  }
  bootstrap_native_color_palette($before);
  return $rows;
}

/**
 * Themes the checks look at: Bootstrap Native and its enabled sub-themes.
 *
 * @return array
 *   Absolute folder, keyed by theme name.
 */
function bootstrap_native_update_check_themes() {
  $themes = array('bootstrap_native' => bootstrap_native_update_theme_dir());
  foreach (bootstrap_native_update_theme_objects() as $name => $theme) {
    if ($name != 'bootstrap_native') {
      $themes[$name] = DRUPAL_ROOT . '/' . dirname($theme->filename);
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
 * Hash of what a build depends on in the theme and sub-themes.
 *
 * The theme's SCSS, the build code, sub-themes' Sass modules. A build made
 * before one of them changed (a git pull) is out of date.
 */
function bootstrap_native_update_theme_hash() {
  $module = DRUPAL_ROOT . '/' . drupal_get_path('module', 'bootstrap_native_update');
  $files = glob(bootstrap_native_update_theme_dir() . '/scss/*.scss');
  $files[] = $module . '/includes/build.inc';
  $files[] = $module . '/includes/parts.inc';
  $additions = bootstrap_native_update_theme_additions();
  $files = array_merge($files, array_keys($additions['sass']));
  sort($files);
  $hash = '';
  foreach ($files as $file) {
    $hash .= md5_file($file);
  }
  return md5($hash);
}

/**
 * Switches the site to a build ('' = the theme's own files).
 *
 * Themes with own or fixed parts switch too: to the builds made with it (a
 * release check), else to what they had when it was last in use (a roll
 * back), else they keep theirs if it is of the same release, else to a
 * build made now.
 *
 * @param string $build
 *   The site-wide build.
 * @param array|null $theme_builds
 *   The themes' builds to use (NULL: as above).
 */
function bootstrap_native_update_activate($build, $theme_builds = NULL) {
  $old = bootstrap_native_update_active();
  $builds = bootstrap_native_update_builds();
  $shipped = bootstrap_native_update_shipped_version();
  $old_map = bootstrap_native_update_theme_builds();
  $version = bootstrap_native_update_build_version($build);
  if ($theme_builds === NULL) {
    if ($build !== '' && !empty($builds[$build]['theme_builds'])) {
      $theme_builds = $builds[$build]['theme_builds'];
    }
    elseif ($build === variable_get('bootstrap_native_update_previous', NULL)) {
      $theme_builds = variable_get('bootstrap_native_update_previous_theme_builds', array());
    }
    else {
      // Their builds stay when they are of the same release (Build and
      // check on the components page only touches the site-wide one).
      $theme_builds = $old_map;
    }
  }
  // Only builds of this release, for themes that still have own or fixed
  // parts; the others are made now (a theme with its own choice that was
  // never applied keeps the site-wide build).
  $separate = bootstrap_native_update_separate_themes();
  foreach ($theme_builds as $name => $theme_build) {
    if (!isset($separate[$name]) || bootstrap_native_update_build_version($theme_build) !== $version || ($theme_build !== '' && empty($builds[$theme_build]['meta']))) {
      unset($theme_builds[$name]);
    }
  }
  foreach ($separate as $name => $mode) {
    if (!isset($theme_builds[$name]) && ($mode == 'fixed' || isset($old_map[$name]))) {
      $made = bootstrap_native_update_make_build($version, $mode == 'own' && isset($old_map[$name]) && isset($builds[$old_map[$name]]['input']) ? $builds[$old_map[$name]]['input'] : FALSE, $name);
      if (!$made['error']) {
        $theme_builds[$name] = $made['build'];
      }
    }
  }
  if ($build !== '') {
    bootstrap_native_update_save_build_record($build, array('inactive_since' => NULL));
    if ($builds[$build]['version'] !== $shipped) {
      bootstrap_native_update_save_version($builds[$build]['version'], array('inactive_since' => NULL));
    }
    variable_set('bootstrap_native_update_selection', $builds[$build]['input']);
  }
  else {
    variable_set('bootstrap_native_update_selection', NULL);
  }
  foreach (array_filter($theme_builds, 'strlen') as $theme_build) {
    bootstrap_native_update_save_build_record($theme_build, array('inactive_since' => NULL));
  }
  if ($old !== $build || $old_map != $theme_builds) {
    variable_set('bootstrap_native_update_previous', $old);
    variable_set('bootstrap_native_update_previous_theme_builds', $old_map);
  }
  variable_set('bootstrap_native_update_active', $build);
  variable_set('bootstrap_native_update_theme_builds', $theme_builds);
  bootstrap_native_update_mark_inactive(array_merge(array($old), array_values($old_map)));
  // A release none of whose builds is in use starts its 30 days.
  $old_version = bootstrap_native_update_build_version($old);
  if ($old !== '' && $old_version !== $version && $old_version !== $shipped) {
    bootstrap_native_update_save_version($old_version, array('inactive_since' => REQUEST_TIME));
  }
  variable_del('bootstrap_native_update_fallback');
  bootstrap_native_update_flush_assets();
  watchdog('bootstrap_native', 'Bootstrap build @new is now active (was @old).', array(
    '@new' => $build === '' ? $shipped : $build,
    '@old' => $old === '' ? $shipped : $old,
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
 * Removes a build (never one in use).
 */
function bootstrap_native_update_discard_build($build) {
  if (!bootstrap_native_update_valid_build($build) || isset(bootstrap_native_update_builds_in_use()[$build])) {
    return;
  }
  file_unmanaged_delete_recursive(bootstrap_native_update_build_uri($build));
  bootstrap_native_update_save_build_record($build, NULL);
  if (variable_get('bootstrap_native_update_previous', '') === $build) {
    variable_set('bootstrap_native_update_previous', '');
  }
}

/**
 * Removes a downloaded release and its builds (never the active one's).
 *
 * @param bool $unapplied_only
 *   Leave it if one of its builds was ever applied (for rolling back).
 */
function bootstrap_native_update_discard_release($version, $unapplied_only = FALSE) {
  if (!bootstrap_native_update_valid_version($version) || $version === bootstrap_native_update_active_version() || $version === bootstrap_native_update_shipped_version()) {
    return;
  }
  $builds = array_filter(bootstrap_native_update_builds(), function ($record) use ($version) {
    return $record['version'] === $version;
  });
  if ($unapplied_only) {
    foreach ($builds as $record) {
      if (!empty($record['applied'])) {
        return;
      }
    }
  }
  foreach (array_keys($builds) as $build) {
    bootstrap_native_update_discard_build($build);
  }
  file_unmanaged_delete_recursive(bootstrap_native_update_release_uri($version));
  bootstrap_native_update_save_version($version, NULL);
}

/**
 * Removes builds and releases inactive for longer than the keep period.
 */
function bootstrap_native_update_cleanup() {
  $in_use = bootstrap_native_update_builds_in_use();
  $limit = REQUEST_TIME - BOOTSTRAP_NATIVE_UPDATE_KEEP;
  foreach (bootstrap_native_update_builds() as $build => $record) {
    if (!isset($in_use[$build]) && !empty($record['inactive_since']) && $record['inactive_since'] < $limit) {
      bootstrap_native_update_discard_build($build);
    }
  }
  $builds = bootstrap_native_update_builds();
  foreach (bootstrap_native_update_versions() as $version => $record) {
    $used = array_filter($builds, function ($b) use ($version) {
      return $b['version'] === $version;
    });
    if (!$used && !empty($record['inactive_since']) && $record['inactive_since'] < $limit) {
      bootstrap_native_update_discard_release($version);
    }
  }
}

/**
 * Whether a build's files are there and are what was built.
 *
 * Compares with the size and fingerprint recorded when it was made (reads
 * the files: for cache clears and cron, not page views).
 */
function bootstrap_native_update_build_intact($build) {
  if ($build === '') {
    return TRUE;
  }
  $builds = bootstrap_native_update_builds();
  if (empty($builds[$build]['meta'])) {
    return FALSE;
  }
  $dir = drupal_realpath(bootstrap_native_update_build_uri($build));
  $css = $dir ? $dir . '/bootstrap_native.css' : '';
  $js = $dir ? $dir . '/bootstrap.bundle.min.js' : '';
  if (!$dir || !is_file($css) || !is_file($js) || filesize($css) != $builds[$build]['meta']['size']) {
    return FALSE;
  }
  return substr(md5(file_get_contents($css) . file_get_contents($js)), 0, 8) === $builds[$build]['meta']['stamp'];
}

/**
 * Downloads a release again (checked against npm's checksum).
 *
 * @return string
 *   '' when its files are back, else the problem.
 */
function bootstrap_native_update_fetch_release($version) {
  $releases = variable_get('bootstrap_native_update_releases', array());
  if (!isset($releases[$version]) && ($error = bootstrap_native_update_refresh_releases())) {
    return $error;
  }
  $versions = bootstrap_native_update_versions();
  $record = isset($versions[$version]) ? $versions[$version] : NULL;
  foreach (array('download', 'verify', 'unpack') as $step) {
    if ($error = _bootstrap_native_update_check_step($step, $version)) {
      return $error;
    }
  }
  // Keep when and by whom it was first downloaded, and its 30 days.
  if ($record) {
    bootstrap_native_update_save_version($version, array_intersect_key($record, array_flip(array('downloaded', 'uid', 'inactive_since', 'build'))));
  }
  return '';
}

/**
 * Makes the files of a build again (re-downloading its release if needed).
 *
 * @return string
 *   '' when the build is whole again, else the problem.
 */
function bootstrap_native_update_restore_build($build) {
  $builds = bootstrap_native_update_builds();
  if (!isset($builds[$build])) {
    return t('Unknown build.');
  }
  $record = $builds[$build];
  if (!bootstrap_native_update_release_present($record['version']) && ($error = bootstrap_native_update_fetch_release($record['version']))) {
    return t('Bootstrap @version could not be downloaded again: @error', array('@version' => $record['version'], '@error' => $error));
  }
  $made = bootstrap_native_update_make_build($record['version'], $record['input'], isset($record['theme']) ? $record['theme'] : NULL);
  if ($made['error']) {
    return $made['error'];
  }
  // The same parts made a build of another name (a sub-theme changed):
  // the stale check sorts that out; this one stays as it was.
  if ($made['build'] !== $build) {
    return t('The parts changed since it was built.');
  }
  // Keep its check, applied and 30 days.
  bootstrap_native_update_save_build_record($build, array_intersect_key($record, array_flip(array('check', 'applied', 'inactive_since', 'created', 'uid'))));
  return bootstrap_native_update_build_intact($build) ? '' : t('The rebuilt files still differ.');
}

/**
 * Repairs the active and previous builds when their files are missing or
 * damaged (cache clears and cron).
 *
 * Nothing is touched when they are whole. If the active build can't be
 * repaired, the site uses the theme's own files and the status report says
 * why.
 */
function bootstrap_native_update_repair() {
  if (variable_get('bootstrap_native_update_repair', FALSE)) {
    variable_del('bootstrap_native_update_repair');
  }
  $active = bootstrap_native_update_active();
  $previous = variable_get('bootstrap_native_update_previous', '');
  $theme_builds = array_merge(array_values(bootstrap_native_update_theme_builds()), array_values(variable_get('bootstrap_native_update_previous_theme_builds', array())));
  foreach (array_unique(array_filter(array_merge(array($active, $previous), $theme_builds), 'strlen')) as $build) {
    if (bootstrap_native_update_build_intact($build)) {
      continue;
    }
    $error = bootstrap_native_update_restore_build($build);
    watchdog('bootstrap_native', 'The files of the Bootstrap build @build were missing or damaged: @result', array('@build' => $build, '@result' => $error ? $error : t('rebuilt')), $error ? WATCHDOG_WARNING : WATCHDOG_NOTICE);
    if ($error && $build === $active) {
      bootstrap_native_update_activate('');
      variable_set('bootstrap_native_update_fallback', array('version' => $build, 'error' => $error, 'time' => REQUEST_TIME, 'reason' => 'files'));
    }
    elseif ($error) {
      bootstrap_native_update_save_build_record($build, array('error' => $error));
    }
    elseif ($build === $active) {
      bootstrap_native_update_flush_assets();
    }
  }
}

/**
 * Brings the active build in line with the code and the sub-themes.
 *
 * After a code update (caches cleared): rebuilds when the theme's SCSS or a
 * sub-theme's Sass changed, or a sub-theme needs a part the build lacks.
 * If that can't be done, the site goes back to the theme's own files and
 * the status report says why.
 */
function bootstrap_native_update_rebuild_if_stale() {
  $active = bootstrap_native_update_active();
  $version = bootstrap_native_update_active_version();
  $plan = bootstrap_native_update_plan_build($version);
  $builds = bootstrap_native_update_builds();
  $stale = $active !== '' && $builds[$active]['meta']['theme_hash'] !== bootstrap_native_update_theme_hash();
  // Themes with own or fixed parts keep theirs (activate() below keeps the
  // map; bootstrap_native_update_rebuild_theme_builds() renews them).
  $map = bootstrap_native_update_theme_builds();
  if ($plan['build'] === $active && !$stale) {
    variable_del('bootstrap_native_update_needs_build');
    bootstrap_native_update_rebuild_theme_builds();
    return;
  }
  if ($plan['build'] === '') {
    bootstrap_native_update_activate('', $map);
    bootstrap_native_update_rebuild_theme_builds();
    return;
  }
  $made = bootstrap_native_update_make_build($version);
  if (!$made['error']) {
    bootstrap_native_update_activate($made['build'], $map);
    variable_del('bootstrap_native_update_needs_build');
    bootstrap_native_update_rebuild_theme_builds();
    return;
  }
  if ($active !== '') {
    bootstrap_native_update_activate('', $map);
    bootstrap_native_update_rebuild_theme_builds();
    variable_set('bootstrap_native_update_fallback', array('version' => $active, 'error' => $made['error'], 'time' => REQUEST_TIME));
    watchdog('bootstrap_native', "Bootstrap Native changed and the Bootstrap build @build could not be rebuilt for it, so the site uses the theme's own files again: @error", array('@build' => $active, '@error' => $made['error']), WATCHDOG_WARNING);
  }
  else {
    // The theme's own files are in use and a sub-theme wants more.
    variable_set('bootstrap_native_update_needs_build', array('error' => $made['error'], 'time' => REQUEST_TIME));
  }
}
