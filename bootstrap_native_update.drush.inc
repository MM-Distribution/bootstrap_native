<?php

/**
 * @file
 * Drush commands: the Sass compiler, checks, sub-themes.
 */

/**
 * Implements hook_drush_command().
 */
function bootstrap_native_update_drush_command() {
  $items['bootstrap-native-sass'] = array(
    'description' => 'Shows the Sass compiler Bootstrap updates use; installs or updates Dart Sass in sites/all/libraries/dart-sass.',
    'options' => array(
      'install' => 'Download Dart Sass for this server and unpack it to sites/all/libraries/dart-sass.',
      'update' => 'Replace sites/all/libraries/dart-sass with the newest release (the old one is kept as dart-sass.previous).',
      'force' => 'Use the new release even if the test build of Bootstrap Native fails.',
    ),
    'examples' => array(
      'drush bootstrap-native-sass' => 'Status.',
      'drush bootstrap-native-sass --install' => 'Install Dart Sass.',
      'drush bootstrap-native-sass --update --yes' => 'Update without asking (for scripts).',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  $items['bootstrap-native-check'] = array(
    'description' => 'Checks a Bootstrap release against Bootstrap Native (the same check as the Bootstrap updates page).',
    'arguments' => array('version' => 'A Bootstrap 6 release, like 6.0.0-beta.1. Default: the newest.'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  $items['bootstrap-native-subtheme'] = array(
    'description' => 'Creates a sub-theme of Bootstrap Native.',
    'arguments' => array('machine_name' => 'Lowercase letters, digits and underscores, like mysite.'),
    'options' => array(
      'name' => 'Its name on the Appearance page (default: from the machine name).',
      'path' => 'Folder to create it in (default: sites/all/themes).',
      'enable' => 'Enable it.',
      'default' => 'Enable it and make it the default theme.',
    ),
    'examples' => array(
      'drush bootstrap-native-subtheme mysite --name="My Site"' => 'Creates sites/all/themes/mysite.',
    ),
    'required-arguments' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  return $items;
}

/**
 * Command callback: bootstrap-native-sass.
 */
function drush_bootstrap_native_update_bootstrap_native_sass() {
  module_load_include('inc', 'bootstrap_native_update', 'includes/sass');
  $install = drush_get_option('install');
  $update = drush_get_option('update');
  $sass = bootstrap_native_update_sass();
  $platform = bootstrap_native_update_sass_platform();
  $library = bootstrap_native_update_sass_library();

  if (!$install && !$update) {
    $latest = bootstrap_native_update_sass_latest();
    $rows = array(
      array(dt('Sass compiler'), $sass ? dt('Dart Sass !version', array('!version' => $sass['version'])) : dt('not found')),
      array($sass ? dt('Found in') : dt('Looked in'), $sass ? bootstrap_native_update_sass_relative(dirname($sass['path'])) : dt('sites/all/libraries/dart-sass, the PATH, the path setting (!setting)', array('!setting' => variable_get('bootstrap_native_update_sass_path', '') === '' ? dt('empty') : variable_get('bootstrap_native_update_sass_path', '')))),
      array(dt('This server'), $platform ? $platform : dt('no Dart Sass build for !os !arch', array('!os' => PHP_OS, '!arch' => php_uname('m')))),
      array(dt('Newest release'), is_array($latest) ? $latest['version'] : dt('unknown (!error)', array('!error' => $latest))),
    );
    drush_print_table($rows);
    if (!$sass) {
      drush_print(dt('Install it with: drush bootstrap-native-sass --install'));
    }
    elseif (is_array($latest) && version_compare($latest['version'], $sass['version'], '>')) {
      drush_print(dt('An update is available: drush bootstrap-native-sass --update'));
    }
    return;
  }

  if (!$platform) {
    return drush_set_error('BOOTSTRAP_NATIVE_SASS', dt('Dart Sass has no build for this server (!os !arch); install it by hand (https://sass-lang.com/install) and set its path on the Bootstrap updates page.', array('!os' => PHP_OS, '!arch' => php_uname('m'))));
  }
  $installed = bootstrap_native_update_sass_version($library . '/sass');
  if ($install && $installed) {
    return drush_set_error('BOOTSTRAP_NATIVE_SASS', dt('Dart Sass !version is already in sites/all/libraries/dart-sass; use --update.', array('!version' => $installed)));
  }
  if ($update && !$installed) {
    return drush_set_error('BOOTSTRAP_NATIVE_SASS', $sass ? dt('The compiler in use (!path) is managed elsewhere; --update only updates sites/all/libraries/dart-sass.', array('!path' => $sass['path'])) : dt('Nothing to update; use --install.'));
  }
  $latest = bootstrap_native_update_sass_latest();
  if (!is_array($latest)) {
    return drush_set_error('BOOTSTRAP_NATIVE_SASS', $latest);
  }
  if ($update && version_compare($latest['version'], $installed, '<=')) {
    drush_log(dt('Dart Sass !version is the newest release.', array('!version' => $installed)), 'ok');
    return;
  }
  $asset = bootstrap_native_update_sass_asset($latest, $platform);
  if (!is_array($asset)) {
    return drush_set_error('BOOTSTRAP_NATIVE_SASS', $asset);
  }
  $question = $install
    ? dt('Dart Sass !version for !platform (!size MB) will be downloaded from github.com/sass/dart-sass and unpacked to sites/all/libraries/dart-sass. Continue?', array('!version' => $latest['version'], '!platform' => $platform, '!size' => round($asset['size'] / 1048576, 1)))
    : dt('Dart Sass !old -> !new (!platform, !size MB). Continue?', array('!old' => $installed, '!new' => $latest['version'], '!platform' => $platform, '!size' => round($asset['size'] / 1048576, 1)));
  if (!drush_confirm($question)) {
    return drush_user_abort();
  }

  $result = bootstrap_native_update_sass_install($asset, $latest['version'], (bool) drush_get_option('force'), function ($line) {
    drush_print(' ' . $line);
  });
  if ($result['error']) {
    drush_log($result['error'], 'warning');
    return drush_set_error('BOOTSTRAP_NATIVE_SASS', $installed ? dt('Nothing changed: !version is still in use.', array('!version' => $installed)) : dt('Nothing was installed.'));
  }
  drush_log($install
    ? dt('Dart Sass !version is installed. Bootstrap updates can now be checked and applied.', array('!version' => $latest['version']))
    : dt('Updated. !old is kept as dart-sass.previous until the next update.', array('!old' => $installed)), 'success');
}

/**
 * Command callback: bootstrap-native-check.
 */
function drush_bootstrap_native_update_bootstrap_native_check($version = NULL) {
  module_load_include('inc', 'bootstrap_native_update', 'includes/releases');
  module_load_include('inc', 'bootstrap_native_update', 'includes/admin');
  if ($error = bootstrap_native_update_refresh_releases()) {
    return drush_set_error('BOOTSTRAP_NATIVE_CHECK', $error);
  }
  $releases = variable_get('bootstrap_native_update_releases', array());
  if ($version === NULL) {
    $newest = reset($releases);
    $version = $newest ? $newest['version'] : '';
  }
  if (!isset($releases[$version])) {
    return drush_set_error('BOOTSTRAP_NATIVE_CHECK', dt('!version is not a known Bootstrap 6 release.', array('!version' => $version)));
  }
  if ($version === bootstrap_native_update_active_version()) {
    return drush_set_error('BOOTSTRAP_NATIVE_CHECK', dt('The site runs !version.', array('!version' => $version)));
  }
  foreach (bootstrap_native_update_check_steps() as $step => $label) {
    if ($error = bootstrap_native_update_check_step($step, $version)) {
      return drush_set_error('BOOTSTRAP_NATIVE_CHECK', html_entity_decode(strip_tags($label . ': ' . $error), ENT_QUOTES));
    }
    drush_print(' ' . $label);
  }
  $builds = bootstrap_native_update_builds();
  $build = variable_get('bootstrap_native_update_last_build', '');
  $result = $builds[$build]['check'];
  $rows = array();
  foreach ($result['rows'] as $row) {
    $text = bootstrap_native_update_row_text($row);
    $detail = $text['details'] !== '' ? $text['details'] : $text['changed'] . ($text['affects'] !== '' ? ' -- ' . $text['affects'] : '');
    $rows[] = array($row['status'], $text['check'], html_entity_decode(strip_tags($detail . ($text['where'] ? ' (' . $text['where'] . ')' : '')), ENT_QUOTES));
  }
  drush_print_table($rows);
  drush_print(dt('Build: !build', array('!build' => $build)));
  drush_print(dt('Verdict: !verdict', array('!verdict' => $result['verdict'])));
  // Themes with own or fixed parts: a line each, then the overall verdict.
  if (!empty($builds[$build]['theme_builds'])) {
    $per_theme = bootstrap_native_update_results_per_theme($build);
    foreach ($per_theme as $key => $row) {
      if ($key != 'site') {
        drush_print(dt('Theme !theme (!mode parts): !verdict (!build)', array('!theme' => implode(', ', $row['themes']), '!mode' => $row['mode'], '!verdict' => $row['check'] ? $row['check']['verdict'] : 'not checked', '!build' => $row['build'])));
      }
    }
    drush_print(dt('All themes: !verdict', array('!verdict' => bootstrap_native_update_worst_verdict($per_theme))));
  }
}

/**
 * Command callback: bootstrap-native-subtheme.
 */
function drush_bootstrap_native_update_bootstrap_native_subtheme($machine) {
  module_load_include('inc', 'bootstrap_native_update', 'includes/subtheme');
  $name = drush_get_option('name', ucwords(str_replace('_', ' ', $machine)));
  $result = bootstrap_native_update_subtheme_create($machine, $name, drush_get_option('path', 'sites/all/themes'));
  if ($result['problems']) {
    return drush_set_error('BOOTSTRAP_NATIVE_SUBTHEME', implode("\n", $result['problems']));
  }
  drush_log(dt('Created !path.', array('!path' => $result['path'])), 'success');
  if (drush_get_option('enable') || drush_get_option('default')) {
    bootstrap_native_update_subtheme_enable($machine, (bool) drush_get_option('default'));
    drush_log(dt('Enabled !name.', array('!name' => $machine)), 'ok');
    if (drush_get_option('default')) {
      drush_log(dt('!name is the default theme.', array('!name' => $machine)), 'ok');
    }
  }
  else {
    system_rebuild_theme_data();
  }
}
