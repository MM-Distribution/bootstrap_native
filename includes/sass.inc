<?php

/**
 * @file
 * Installing Dart Sass into sites/all/libraries/dart-sass (drush only).
 *
 * Only a person running drush installs or replaces the compiler: the web
 * pages never download a program they would later run.
 */

module_load_include('inc', 'bootstrap_native_update', 'includes/releases');
module_load_include('inc', 'bootstrap_native_update', 'includes/build');
module_load_include('inc', 'bootstrap_native_update', 'includes/checks');

/**
 * Where drush installs Dart Sass.
 */
function bootstrap_native_update_sass_library() {
  return DRUPAL_ROOT . '/sites/all/libraries/dart-sass';
}

/**
 * A path from the Drupal root, if it is inside it.
 */
function bootstrap_native_update_sass_relative($path) {
  return strpos($path, DRUPAL_ROOT . '/') === 0 ? substr($path, strlen(DRUPAL_ROOT) + 1) : $path;
}

/**
 * Dart Sass's name for this server's build (linux-x64, macos-arm64, ...).
 *
 * @return string
 *   '' when Dart Sass has no build for it (Windows gets a .zip: install by
 *   hand).
 */
function bootstrap_native_update_sass_platform() {
  $os = array('Linux' => 'linux', 'Darwin' => 'macos');
  $os = isset($os[PHP_OS]) ? $os[PHP_OS] : '';
  $machine = strtolower(php_uname('m'));
  $archs = array(
    'x86_64' => 'x64',
    'amd64' => 'x64',
    'aarch64' => 'arm64',
    'arm64' => 'arm64',
    'armv7l' => 'arm',
    'armv6l' => 'arm',
    'riscv64' => 'riscv64',
  );
  if (!$os || !isset($archs[$machine])) {
    return '';
  }
  $platform = $os . '-' . $archs[$machine];
  // Alpine and other musl systems have their own build.
  if ($os == 'linux' && (file_exists('/etc/alpine-release') || glob('/lib/ld-musl-*'))) {
    $platform .= '-musl';
  }
  return $platform;
}

/**
 * The newest Dart Sass release on GitHub.
 *
 * @return array|string
 *   Keys version, assets (GitHub's list); or what went wrong.
 */
function bootstrap_native_update_sass_latest() {
  $response = drupal_http_request('https://api.github.com/repos/sass/dart-sass/releases/latest', array(
    'timeout' => 20,
    'headers' => array('Accept' => 'application/vnd.github+json'),
  ));
  $release = $response->code == 200 ? json_decode($response->data, TRUE) : NULL;
  if (empty($release['tag_name']) || empty($release['assets']) || !preg_match('/^\d+\.\d+\.\d+$/', $release['tag_name'])) {
    return t('Could not read the newest Dart Sass release from GitHub (@code).', array('@code' => isset($response->error) ? $response->error : $response->code));
  }
  return array('version' => $release['tag_name'], 'assets' => $release['assets']);
}

/**
 * The download for this server in a release.
 *
 * @return array|string
 *   Keys url, size, sha256, name; or what went wrong.
 */
function bootstrap_native_update_sass_asset(array $release, $platform) {
  $name = 'dart-sass-' . $release['version'] . '-' . $platform . '.tar.gz';
  foreach ($release['assets'] as $asset) {
    if ($asset['name'] !== $name) {
      continue;
    }
    if (empty($asset['digest']) || !preg_match('/^sha256:([0-9a-f]{64})$/', $asset['digest'], $m)) {
      return t('GitHub lists no checksum for @name, so it is not downloaded.', array('@name' => $name));
    }
    if (strpos($asset['browser_download_url'], 'https://github.com/sass/dart-sass/releases/download/') !== 0) {
      return t('Unexpected download address for @name.', array('@name' => $name));
    }
    return array('url' => $asset['browser_download_url'], 'size' => $asset['size'], 'sha256' => $m[1], 'name' => $name);
  }
  return t('Dart Sass @version has no @name.', array('@version' => $release['version'], '@name' => $name));
}

/**
 * Downloads, checks and installs Dart Sass.
 *
 * Unpacked next to the current copy, tested (sass --version, then a test
 * build of Bootstrap Native), and only then switched in; the old copy stays
 * as dart-sass.previous.
 *
 * @param array $asset
 *   From bootstrap_native_update_sass_asset().
 * @param string $version
 *   The release's version.
 * @param bool $force
 *   Switch in even if the test build fails.
 * @param callable $say
 *   Gets one line per step.
 *
 * @return array
 *   Keys: error ('' when installed).
 */
function bootstrap_native_update_sass_install(array $asset, $version, $force, $say) {
  $library = bootstrap_native_update_sass_library();
  $new = $library . '.new';
  $archive = file_directory_temp() . '/' . $asset['name'];
  $fail = function ($error) use ($new, $archive) {
    file_unmanaged_delete_recursive($new);
    @unlink($archive);
    return array('error' => $error);
  };

  $response = drupal_http_request($asset['url'], array('timeout' => 120, 'max_redirects' => 5));
  if ($response->code != 200 || empty($response->data) || file_put_contents($archive, $response->data) === FALSE) {
    return $fail(t('The download failed (@code).', array('@code' => isset($response->error) ? $response->error : $response->code)));
  }
  $say(t('✓ Downloaded @name', array('@name' => $asset['name'])));
  if (!hash_equals($asset['sha256'], hash_file('sha256', $archive))) {
    return $fail(t("✗ The download doesn't match the checksum GitHub publishes. It was removed."));
  }
  $say(t('✓ Checksum matches the one GitHub publishes (sha256)'));

  require_once DRUPAL_ROOT . '/modules/system/system.tar.inc';
  $tar = new Archive_Tar($archive, 'gz');
  try {
    $entries = $tar->listContent();
    $problem = bootstrap_native_update_archive_problem(is_array($entries) ? $entries : array(), 'dart-sass/');
  }
  catch (Exception $e) {
    $problem = $e->getMessage();
  }
  if ($problem) {
    return $fail(t('✗ Refused: @problem', array('@problem' => $problem)));
  }
  file_unmanaged_delete_recursive($new);
  if (!@mkdir($new, 0755, TRUE) || !$tar->extractModify($new, 'dart-sass/', FALSE, FALSE)) {
    return $fail(t('✗ Could not unpack into @dir.', array('@dir' => bootstrap_native_update_sass_relative($new))));
  }
  @unlink($archive);
  foreach (array('sass', 'src/dart') as $program) {
    if (is_file($new . '/' . $program)) {
      @chmod($new . '/' . $program, 0755);
    }
  }
  if (bootstrap_native_update_sass_version($new . '/sass') !== $version) {
    return $fail(t("✗ The unpacked compiler doesn't run on this server."));
  }
  $say(t('✓ Unpacked; it runs (Dart Sass @version)', array('@version' => $version)));

  $build = bootstrap_native_update_compile($new . '/sass', bootstrap_native_update_theme_dir() . '/scss/bootstrap_native.scss', bootstrap_native_update_theme_dir() . '/vendor');
  if ($build['css'] === NULL) {
    $say(t('✗ Test build of Bootstrap Native failed:') . "\n     " . str_replace("\n", "\n     ", $build['error']));
    if (!$force) {
      return $fail(t('To use it anyway: add --force.'));
    }
  }
  else {
    $say(t('✓ Test build of Bootstrap Native: @seconds s', array('@seconds' => round($build['seconds'], 1))));
  }

  $previous = $library . '.previous';
  if (is_dir($library)) {
    file_unmanaged_delete_recursive($previous);
    if (!@rename($library, $previous)) {
      return $fail(t('✗ Could not move the current copy aside.'));
    }
  }
  if (!@rename($new, $library)) {
    @rename($previous, $library);
    return $fail(t('✗ Could not move the new copy into place.'));
  }
  // The folder holds programs, not web pages.
  $deny = "# Dart Sass, used by Bootstrap Native's update checks; not for the web.\n<IfModule mod_authz_core.c>\n  Require all denied\n</IfModule>\n<IfModule !mod_authz_core.c>\n  Deny from all\n</IfModule>\n";
  foreach (array($library, $previous) as $dir) {
    if (is_dir($dir)) {
      file_put_contents($dir . '/.htaccess', $deny);
    }
  }
  drupal_static_reset('bootstrap_native_update_sass');
  return array('error' => '');
}
