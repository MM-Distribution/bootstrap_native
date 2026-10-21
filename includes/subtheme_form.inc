<?php

/**
 * @file
 * Appearance > Create a Bootstrap Native sub-theme.
 *
 * Writes the same files as drush bootstrap-native-subtheme
 * (includes/subtheme.inc), or offers them as a .tar.gz to unpack by hand.
 */

module_load_include('inc', 'bootstrap_native_update', 'includes/subtheme');

/**
 * Form: create a sub-theme.
 */
function bootstrap_native_update_subtheme_form($form, &$form_state) {
  drupal_set_title(t('Create a sub-theme'));
  $form['#attached']['css'][] = drupal_get_path('module', 'bootstrap_native_update') . '/css/bootstrap_native_update.css';
  $form['#attributes']['class'][] = 'bnu-subtheme';
  // The site's first theme (or a move off Bootstrap Native itself): its
  // name, enabled and the default.
  $state = bootstrap_native_update_site_theme_state();
  $first = $state !== '';
  $intro = $state == 'base'
    ? t("This site uses Bootstrap Native directly. Its own theme gets the same settings, colors, logo and blocks, and becomes the default; the pages look the same, and the site's styles and templates have a safe place.")
    : t("A sub-theme holds this site's own styles and templates. Bootstrap Native updates never touch it. It starts with Bootstrap Native's regions, colors and logo, so switching to it changes nothing until you edit it.");
  $form['intro'] = array('#markup' => '<p class="bnu-intro">' . $intro . '</p>');
  $site_name = variable_get('site_name', '');
  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#required' => TRUE,
    '#maxlength' => 64,
    '#size' => 40,
    '#default_value' => $first ? bootstrap_native_update_subtheme_clean_name($site_name) : '',
  );
  $form['machine'] = array(
    '#type' => 'machine_name',
    '#maxlength' => 32,
    '#machine_name' => array(
      'exists' => 'bootstrap_native_update_subtheme_name_taken',
      'source' => array('name'),
    ),
    '#description' => t('Lowercase letters, digits and underscores. The folder and the PHP functions are named after it.'),
    '#default_value' => $first && $site_name !== '' ? bootstrap_native_update_subtheme_suggest($site_name) : '',
  );
  $folders = array('sites/all/themes' => t('sites/all/themes (all sites)'));
  $site = conf_path() . '/themes';
  if ($site != 'sites/all/themes') {
    $folders[$site] = t('@path (this site only)', array('@path' => $site));
  }
  $form['parent'] = array(
    '#type' => 'select',
    '#title' => t('Folder'),
    '#options' => $folders,
    '#default_value' => 'sites/all/themes',
    '#access' => count($folders) > 1,
  );
  $form['enable'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable it'),
    '#default_value' => 1,
  );
  $form['default'] = array(
    '#type' => 'checkbox',
    '#title' => t('Make it the default theme'),
    '#description' => $first ? t("On for the site's first theme.") : t('Off by default: check it on the Appearance page first.'),
    '#default_value' => $first ? 1 : 0,
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['create'] = array(
    '#type' => 'submit',
    '#value' => t('Create sub-theme'),
    '#attributes' => array('class' => array('button--primary')),
  );
  $form['actions']['download'] = array(
    '#type' => 'submit',
    '#value' => t('Download as .tar.gz instead'),
    '#submit' => array('bootstrap_native_update_subtheme_form_download'),
  );
  $form['help'] = array('#markup' => '<p class="bnu-small">' . t("Download gives you the same files to unpack yourself (for servers where the site can't write to its themes folder, or to keep the theme in its own repository). The same as <code>drush bootstrap-native-subtheme</code>.") . '</p>');
  return $form;
}

/**
 * Machine name callback: taken by a module or theme, or reserved.
 */
function bootstrap_native_update_subtheme_name_taken($machine) {
  return (bool) db_query("SELECT 1 FROM {system} WHERE name = :name", array(':name' => $machine))->fetchField() || in_array($machine, array('bootstrap_native', 'starterkit', 'theme', 'system', 'node', 'user'));
}

/**
 * Form validation: the same checks as drush, and a writable folder.
 */
function bootstrap_native_update_subtheme_form_validate($form, &$form_state) {
  $values = $form_state['values'];
  $parent = isset($form['parent']['#options'][$values['parent']]) ? $values['parent'] : 'sites/all/themes';
  $form_state['values']['parent'] = $parent;
  $download = isset($form_state['triggering_element']['#submit']) && in_array('bootstrap_native_update_subtheme_form_download', $form_state['triggering_element']['#submit']);
  // A folder of that name only matters when writing here.
  foreach (bootstrap_native_update_subtheme_problems($values['machine'], $values['name'], $parent, !$download) as $problem) {
    form_set_error('machine', $problem);
  }
  if (!$download && !form_get_errors()) {
    $dir = DRUPAL_ROOT . '/' . $parent;
    $writable = is_dir($dir) ? is_writable($dir) : is_writable(dirname($dir));
    if (!$writable) {
      form_set_error('parent', t("@path isn't writable by the site. Use <em>Download as .tar.gz instead</em> and unpack it there, or run <code>drush bootstrap-native-subtheme @machine --name=\"@name\"</code>.", array(
        '@path' => $parent,
        '@machine' => $values['machine'],
        '@name' => bootstrap_native_update_subtheme_clean_name($values['name']),
      )));
    }
  }
}

/**
 * Form submit: Create sub-theme.
 */
function bootstrap_native_update_subtheme_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $result = bootstrap_native_update_subtheme_create($values['machine'], $values['name'], $values['parent']);
  if ($result['problems']) {
    foreach ($result['problems'] as $problem) {
      drupal_set_message($problem, 'error');
    }
    return;
  }
  $did = t('Created @path.', array('@path' => $result['path']));
  if ($values['enable'] || $values['default']) {
    bootstrap_native_update_subtheme_enable($values['machine'], (bool) $values['default']);
    $did = $values['default']
      ? t('Created @path; it is now the default theme.', array('@path' => $result['path']))
      : t('Created @path and enabled it.', array('@path' => $result['path']));
  }
  else {
    system_rebuild_theme_data();
  }
  drupal_set_message('<strong>' . $did . '</strong> ' . t('Next: put this site\'s styles in <code>css/@machine.css</code>, and copy any template to change into <code>templates/</code>.', array('@machine' => $values['machine']))
    . (($values['enable'] || $values['default']) ? ' ' . l(t('Settings for @name', array('@name' => bootstrap_native_update_subtheme_clean_name($values['name']))), 'admin/appearance/settings/' . $values['machine']) : ''));
  $form_state['redirect'] = 'admin/appearance';
}

/**
 * Form submit: Download as .tar.gz instead.
 */
function bootstrap_native_update_subtheme_form_download($form, &$form_state) {
  require_once DRUPAL_ROOT . '/modules/system/system.tar.inc';
  $values = $form_state['values'];
  $machine = $values['machine'];
  $tmp = file_directory_temp() . '/bnu-subtheme-' . mt_rand();
  // Built in a temporary "themes" folder, then packed from there.
  $result = bootstrap_native_update_subtheme_write($machine, $values['name'], $tmp . '/' . $machine);
  if ($result) {
    drupal_set_message($result, 'error');
    file_unmanaged_delete_recursive($tmp);
    return;
  }
  $archive = $tmp . '/' . $machine . '.tar.gz';
  $tar = new Archive_Tar($archive, 'gz');
  $tar->createModify(array($tmp . '/' . $machine), '', $tmp);
  drupal_add_http_header('Content-Type', 'application/gzip');
  drupal_add_http_header('Content-Disposition', 'attachment; filename="' . $machine . '.tar.gz"');
  drupal_add_http_header('Content-Length', filesize($archive));
  drupal_send_headers();
  readfile($archive);
  file_unmanaged_delete_recursive($tmp);
  drupal_exit();
}
