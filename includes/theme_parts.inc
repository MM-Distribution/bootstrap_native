<?php

/**
 * @file
 * Step 6 of a Bootstrap Native theme's settings: its Bootstrap components
 * (the site-wide choice, or its own; a theme whose .info fixes them shows
 * them locked). Saving its own builds and checks them first; its pages
 * keep what they use until the results are applied.
 */

module_load_include('inc', 'bootstrap_native_update', 'includes/releases');
module_load_include('inc', 'bootstrap_native_update', 'includes/admin');
module_load_include('inc', 'bootstrap_native_update', 'includes/components');

/**
 * Adds the step to a theme's settings form.
 */
function bootstrap_native_update_theme_parts_step(&$form, &$form_state, $theme) {
  $version = bootstrap_native_update_active_version();
  $mode = bootstrap_native_update_parts_mode($theme);
  $site = bootstrap_native_update_plan_build($version);
  $summary = array('site' => t('Site-wide choice'), 'fixed' => t('Fixed in its .info'));
  if ($mode == 'own') {
    $own = bootstrap_native_update_plan_build($version, FALSE, $theme);
    $diff = bootstrap_native_update_selection_diff($own['effective'], $site['effective']);
    $summary['own'] = format_plural(count($diff), 'Its own (1 change)', 'Its own (@count changes)');
  }
  $title = t('Bootstrap components');
  $step = function_exists('bootstrap_native_settings_step')
    ? bootstrap_native_settings_step(6, $title, 'components', check_plain($summary[$mode]))
    : array('#type' => 'fieldset', '#title' => $title);
  $step['#attached'] = bootstrap_native_update_admin_css();
  $step['#attached']['js'][] = drupal_get_path('module', 'bootstrap_native_update') . '/js/components.js';

  if ($mode == 'fixed') {
    $themes = list_themes();
    $fixed = bootstrap_native_update_plan_build($version, FALSE, $theme);
    $ids = bootstrap_native_update_selection_ids($fixed['effective']);
    $step['fixed'] = array('#markup' => '<div class="bnu-locked"><svg aria-hidden="true" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/></svg><span>'
      . t('Set in <code>@file</code> (<code>bootstrap_native[parts_fixed] = TRUE</code>): @count parts its pages are built for. The site-wide choice doesn\'t change them; to change them, change the theme\'s .info (or make a sub-theme of it).', array('@file' => basename($themes[$theme]->filename), '@count' => count($ids)))
      . '</span></div>');
    $step['list'] = array(
      '#type' => 'fieldset',
      '#title' => format_plural(count($ids), 'Its part', 'Its @count parts'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#attached' => array('library' => array(array('system', 'drupal.collapse'))),
      'parts' => array('#markup' => '<p class="bnu-muted">' . check_plain(implode(', ', array_map('bootstrap_native_update_part_name', $ids))) . '</p>'),
    );
    bootstrap_native_update_theme_parts_place($form, $step);
    return;
  }

  $sass = bootstrap_native_update_sass();
  $own_saved = bootstrap_native_update_own_selection($theme);
  $context = bootstrap_native_update_parts_context($version, $theme);
  // Its own starts as a copy of the site-wide choice.
  $start = $mode == 'own' ? $own['effective'] : $site['effective'];
  $chosen = array_flip(bootstrap_native_update_selection_ids($start));
  $site_ids = array_flip(bootstrap_native_update_selection_ids($site['effective']));
  $notes = array();
  foreach (array_keys($chosen + $site_ids) as $id) {
    if (isset($chosen[$id]) && !isset($site_ids[$id])) {
      $notes[$id] = '<span class="bnu-note-own">' . t('On here · off site-wide') . '</span>';
    }
    elseif (!isset($chosen[$id]) && isset($site_ids[$id])) {
      $notes[$id] = '<span class="bnu-note-own">' . t('Off here · on site-wide') . '</span>';
    }
  }
  $site_size = $site['build'] === '' ? filesize(bootstrap_native_update_theme_dir() . '/css/bootstrap_native.css') : (isset(bootstrap_native_update_builds()[$site['build']]['meta']['size']) ? bootstrap_native_update_builds()[$site['build']]['meta']['size'] : 0);

  $step['bootstrap_native_parts_mode'] = array(
    '#type' => 'radios',
    '#title' => t("This theme's parts"),
    '#options' => array(
      'site' => t('Same as the site-wide choice'),
      'own' => t('Its own'),
    ),
    '#default_value' => $mode,
    'site' => array('#description' => t('@count parts, @kb KB. <a href="@url">Change the site-wide choice</a>', array('@count' => count($site_ids), '@kb' => round($site_size / 1024), '@url' => url('admin/appearance/bootstrap-components')))),
    'own' => array('#description' => t('Starts as a copy of the site-wide choice.')),
  );
  $step['own'] = array(
    '#type' => 'container',
    '#states' => array('visible' => array(':input[name="bootstrap_native_parts_mode"]' => array('value' => 'own'))),
  );
  if ($mode == 'own' && ($diff = bootstrap_native_update_selection_diff($own['effective'], $site['effective']))) {
    $step['own']['diff'] = array('#markup' => '<div class="bnu-diff">' . t('Differs from the site-wide choice: @changes.', array('@changes' => implode(', ', $diff))) . ' <label class="bnu-link" for="edit-bootstrap-native-parts-mode-site">' . t('Back to the site-wide choice') . '</label></div>');
  }
  $groups = array(
    'base' => t('Base'),
    'content' => t('Content'),
    'layout' => t('Layout'),
    'forms' => t('Forms'),
    'buttons' => t('Buttons'),
    'components' => t('Components'),
    'helpers' => t('Helpers'),
    'utilities' => t('Utilities'),
  );
  $step['own']['bootstrap_native_parts_own'] = array(
    '#theme' => 'bootstrap_native_update_components_table',
    '#tree' => TRUE,
    '#groups' => $groups,
    '#bnu_title' => '',
  );
  $step['own']['bootstrap_native_parts_own'] += bootstrap_native_update_parts_checkboxes($version, $context, $chosen, (bool) $sass, bootstrap_native_update_parts_in_templates($version, $context), bootstrap_native_update_part_info($version), $notes);
  $step['help'] = array('#markup' => '<p class="bnu-small">' . t("Save builds this theme's stylesheet with these parts and shows the check results before any page uses it (the same as Build and check). Nothing changes for this theme's visitors until then.") . '</p>');
  if (!$sass) {
    $step['nosass'] = bootstrap_native_update_notice('warning', t('No Sass compiler found: its own parts can be chosen, and are built once there is one (<code>drush bootstrap-native-sass --install</code>).'));
  }
  $form['bnu_parts_was'] = array('#type' => 'value', '#value' => $own_saved);
  $form['#validate'][] = 'bootstrap_native_update_theme_parts_validate';
  $form['#submit'][] = 'bootstrap_native_update_theme_parts_submit';
  bootstrap_native_update_theme_parts_place($form, $step);
}

/**
 * Puts the step after the theme's own five (or at the end of the form).
 */
function bootstrap_native_update_theme_parts_place(&$form, $step) {
  if (isset($form['bn_settings']['steps'])) {
    $form['bn_settings']['steps']['components'] = $step;
  }
  else {
    $step['#weight'] = 50;
    $form['bootstrap_native_components'] = $step;
  }
}

/**
 * Form validation: the choice becomes the saved setting
 * (bootstrap_native_parts: a selection, or NULL for the site-wide one).
 */
function bootstrap_native_update_theme_parts_validate($form, &$form_state) {
  if (!isset($form_state['values']['bootstrap_native_parts_mode'])) {
    return;
  }
  $selection = NULL;
  if ($form_state['values']['bootstrap_native_parts_mode'] == 'own' && isset($form_state['values']['bootstrap_native_parts_own'])) {
    $selection = bootstrap_native_update_components_selection($form_state['values']['bootstrap_native_parts_own']);
  }
  $form_state['values']['bootstrap_native_parts'] = $selection;
  unset($form_state['values']['bootstrap_native_parts_mode'], $form_state['values']['bootstrap_native_parts_own']);
}

/**
 * Form submit (after core saved the settings): back to the site-wide
 * build, or build and check its own parts.
 */
function bootstrap_native_update_theme_parts_submit($form, &$form_state) {
  $theme = $form_state['build_info']['args'][0];
  $themes = list_themes();
  $mode = bootstrap_native_update_parts_mode($theme);
  if ($mode == 'site') {
    if (array_key_exists($theme, bootstrap_native_update_theme_builds())) {
      bootstrap_native_update_drop_theme_build($theme);
      drupal_set_message(t('@theme uses the site-wide choice of Bootstrap parts again.', array('@theme' => $themes[$theme]->info['name'])));
    }
    return;
  }
  if ($mode != 'own') {
    return;
  }
  $version = bootstrap_native_update_active_version();
  $plan = bootstrap_native_update_plan_build($version, FALSE, $theme);
  if ($plan['build'] === bootstrap_native_update_build_for_theme($theme)) {
    return;
  }
  if ($plan['build'] === '') {
    // The theme's own files: nothing to build or check.
    bootstrap_native_update_apply_theme_build($theme, '');
    return;
  }
  if (!bootstrap_native_update_sass()) {
    return;
  }
  $steps = array('build' => t('Build a test stylesheet with these parts'), 'check' => t('Check for breaks'));
  batch_set(array(
    'title' => t('Building Bootstrap @version with the parts of @theme', array('@version' => $version, '@theme' => $themes[$theme]->info['name'])),
    'operations' => array(
      array('bootstrap_native_update_theme_parts_batch', array('build', $version, $theme)),
      array('bootstrap_native_update_theme_parts_batch', array('check', $version, $theme)),
    ),
    'init_message' => bootstrap_native_update_steps_html($steps, 'build'),
    'progress_message' => '',
    'error_message' => t('The build stopped with an error.'),
    'finished' => 'bootstrap_native_update_batch_finished',
    'file' => drupal_get_path('module', 'bootstrap_native_update') . '/includes/theme_parts.inc',
    'css' => array(drupal_get_path('module', 'bootstrap_native_update') . '/css/bootstrap_native_update.css'),
  ));
  $form_state['redirect'] = array('admin/appearance/bootstrap-updates/result/' . $plan['build'], array('query' => array('theme' => $theme)));
}

/**
 * Batch operation: build a theme's own parts, then check them against what
 * the theme uses now.
 */
function bootstrap_native_update_theme_parts_batch($step, $version, $theme, &$context) {
  $steps = array('build' => t('Build a test stylesheet with these parts'), 'check' => t('Check for breaks'));
  if ($step == 'build') {
    bootstrap_native_update_make_build($version, FALSE, $theme);
    $context['message'] = bootstrap_native_update_steps_html($steps, 'check');
    return;
  }
  $plan = bootstrap_native_update_plan_build($version, FALSE, $theme);
  bootstrap_native_update_check_build($plan['build'], bootstrap_native_update_build_for_theme($theme), bootstrap_native_update_check_themes_for($theme));
  $context['message'] = bootstrap_native_update_steps_html($steps, '');
}
