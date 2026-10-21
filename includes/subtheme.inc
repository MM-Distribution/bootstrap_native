<?php

/**
 * @file
 * Creates a sub-theme of Bootstrap Native from starterkit/.
 *
 * What goes into the files: the starter kit's own text, the machine name
 * (checked to be [a-z][a-z0-9_]*, so it is safe in PHP and file names) and
 * the human name (in the .info file and the README only, cleaned to one
 * line of plain text). Nothing else from the request is written.
 */

/**
 * Checks a new sub-theme's names and folder.
 *
 * @return array
 *   Problems (empty when it can be created).
 */
function bootstrap_native_update_subtheme_problems($machine, $name, $parent) {
  $problems = array();
  if (!preg_match('/^[a-z][a-z0-9_]{1,31}$/', $machine)) {
    $problems[] = t('The machine name must be 2 to 32 characters: lowercase letters, digits and underscores, starting with a letter.');
  }
  elseif (db_query("SELECT 1 FROM {system} WHERE name = :name", array(':name' => $machine))->fetchField() || in_array($machine, array('bootstrap_native', 'starterkit', 'theme', 'system', 'node', 'user'))) {
    $problems[] = t('@name is already the name of a module or theme.', array('@name' => $machine));
  }
  if (bootstrap_native_update_subtheme_clean_name($name) === '') {
    $problems[] = t('The name is empty.');
  }
  $parent = rtrim($parent, '/');
  if (!preg_match('#^sites/[^/]+/themes$#', $parent)) {
    $problems[] = t('Sub-themes go in sites/all/themes or sites/<site>/themes.');
  }
  elseif (!$problems && file_exists(DRUPAL_ROOT . '/' . $parent . '/' . $machine)) {
    $problems[] = t('@path already exists.', array('@path' => $parent . '/' . $machine));
  }
  return $problems;
}

/**
 * A human name as one short line of plain text.
 */
function bootstrap_native_update_subtheme_clean_name($name) {
  $name = preg_replace('/[\x00-\x1F\x7F"\\\\]+/u', ' ', (string) $name);
  return trim(drupal_substr(preg_replace('/\s+/', ' ', $name), 0, 64));
}

/**
 * Creates the sub-theme.
 *
 * @param string $machine
 *   Machine name.
 * @param string $name
 *   Human name.
 * @param string $parent
 *   Folder for it, from the Drupal root (sites/all/themes).
 *
 * @return array
 *   Keys: path (from the Drupal root, '' on failure), problems.
 */
function bootstrap_native_update_subtheme_create($machine, $name, $parent = 'sites/all/themes') {
  $parent = rtrim($parent, '/');
  $problems = bootstrap_native_update_subtheme_problems($machine, $name, $parent);
  if ($problems) {
    return array('path' => '', 'problems' => $problems);
  }
  $name = bootstrap_native_update_subtheme_clean_name($name);
  $path = $parent . '/' . $machine;
  $target = DRUPAL_ROOT . '/' . $path;
  $kit = DRUPAL_ROOT . '/' . drupal_get_path('module', 'bootstrap_native_update') . '/starterkit';

  if (!is_dir(DRUPAL_ROOT . '/' . $parent) && !@mkdir(DRUPAL_ROOT . '/' . $parent, 0755, TRUE) || !@mkdir($target, 0755)) {
    return array('path' => '', 'problems' => array(t('@path could not be created (permissions?).', array('@path' => $path))));
  }

  $files = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($kit, FilesystemIterator::SKIP_DOTS), RecursiveIteratorIterator::SELF_FIRST);
  foreach ($files as $file) {
    $relative = str_replace('STARTERKIT', $machine, substr($file->getPathname(), strlen($kit) + 1));
    if ($file->isDir()) {
      @mkdir($target . '/' . $relative, 0755);
      continue;
    }
    $text = strtr(file_get_contents($file->getPathname()), array(
      'STARTERKIT_NAME' => $name,
      'STARTERKIT' => $machine,
    ));
    if (file_put_contents($target . '/' . $relative, $text) === FALSE) {
      file_unmanaged_delete_recursive($target);
      return array('path' => '', 'problems' => array(t('@file could not be written.', array('@file' => $path . '/' . $relative))));
    }
  }
  file_put_contents($target . '/' . $machine . '.info', bootstrap_native_update_subtheme_info($machine, $name));

  // Same look from the start: Bootstrap Native's settings (colors, logo).
  $settings = variable_get('theme_bootstrap_native_settings', array());
  if ($settings) {
    variable_set('theme_' . $machine . '_settings', $settings);
  }
  watchdog('bootstrap_native', 'Created the sub-theme @name (@path).', array('@name' => $name, '@path' => $path));
  return array('path' => $path, 'problems' => array());
}

/**
 * The sub-theme's .info file.
 *
 * Regions and features are copied from Bootstrap Native: in Drupal 7 a
 * sub-theme that lists none gets Drupal's defaults, not its base theme's.
 */
function bootstrap_native_update_subtheme_info($machine, $name) {
  $base = drupal_parse_info_file(DRUPAL_ROOT . '/' . drupal_get_path('module', 'bootstrap_native_update') . '/themes/bootstrap_native/bootstrap_native.info');
  $lines = array(
    'name = "' . $name . '"',
    'description = "A sub-theme of Bootstrap Native."',
    'core = 7.x',
    'base theme = bootstrap_native',
    '',
    'stylesheets[all][] = css/' . $machine . '.css',
    '',
  );
  foreach ($base['regions'] as $key => $label) {
    $lines[] = 'regions[' . $key . '] = ' . $label;
  }
  $lines[] = '';
  foreach ($base['features'] as $feature) {
    $lines[] = 'features[] = ' . $feature;
  }
  $lines[] = '';
  $lines[] = '; Colors are set on the theme settings page (they start as Bootstrap';
  $lines[] = "; Native's at the time this theme was created).";
  $lines[] = 'settings[default_logo] = 0';
  return implode("\n", $lines) . "\n";
}
