<?php

/**
 * @file
 * Theme settings: five steps (brand, page, links, header, browser icon)
 * beside a live preview, and a save bar.
 *
 * Only a few colors to choose; the theme works out the rest (shades,
 * borders, text on buttons, readable link colors; see includes/color.inc).
 * Every color is optional: empty means automatic (worked out, or
 * Bootstrap's own). A color is a hex value or one of Bootstrap's names
 * (indigo-600), kept as typed.
 *
 * Core's own settings (logo, which parts the header shows, the browser
 * icon) move into the steps; their names and saving stay core's.
 */

require_once dirname(__FILE__) . '/includes/color.inc';

/**
 * Implements hook_form_FORM_ID_alter() for system_theme_settings.
 */
function bootstrap_native_form_system_theme_settings_alter(&$form, &$form_state, $form_id = NULL) {
  // Runs once per base theme as well (sub-themes): add the settings once.
  if (isset($form['bn_settings'])) {
    return;
  }
  // AJAX rebuilds load the form from cache; this file must come along.
  $form_state['build_info']['files']['bootstrap_native'] = drupal_get_path('theme', 'bootstrap_native') . '/theme-settings.php';
  $theme = isset($form_state['build_info']['args'][0]) ? $form_state['build_info']['args'][0] : 'bootstrap_native';
  $path = drupal_get_path('theme', 'bootstrap_native');
  bootstrap_native_color_load_palette();

  $form['#attributes']['class'][] = 'bn-settings-form';
  $form['#attached']['css'][] = $path . '/css/theme-settings.css';
  $form['#attached']['js'][] = $path . '/js/theme-settings.js';

  $values = bootstrap_native_color_form_values($form_state, $theme);
  $tokens = bootstrap_native_color_tokens(bootstrap_native_color_valid($values));

  $form['bn_settings'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bn-settings')),
    '#weight' => -100,
  );
  $steps = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bn-steps')),
  );

  // 1 Brand: logo, primary, secondary.
  $steps['brand'] = bootstrap_native_settings_step(1, t('Brand'), 'brand', bootstrap_native_settings_summary('brand', $values, $theme), TRUE);
  $steps['brand']['logo'] = bootstrap_native_settings_logo($form, $theme);
  $steps['brand']['colors'] = bootstrap_native_settings_colors(array('primary', 'secondary'), $values, $tokens);

  // 2 Page: background, text.
  $steps['page'] = bootstrap_native_settings_step(2, t('Page'), 'page', bootstrap_native_settings_summary('page', $values, $theme));
  $steps['page']['colors'] = bootstrap_native_settings_colors(array('base', 'text'), $values, $tokens);

  // 3 Links: link, hover.
  $steps['links'] = bootstrap_native_settings_step(3, t('Links'), 'links', bootstrap_native_settings_summary('links', $values, $theme));
  $steps['links']['colors'] = bootstrap_native_settings_colors(array('link', 'link_hover'), $values, $tokens);

  // 4 Header: core's display toggles (all but the browser icon).
  $steps['header'] = bootstrap_native_settings_step(4, t('Header'), 'header', bootstrap_native_settings_summary('header', $values, $theme));
  $steps['header']['help'] = array('#markup' => '<p class="bn-help">' . t('What the top of every page shows.') . '</p>');
  $steps['header']['toggles'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bn-toggles')),
  );
  if (isset($form['theme_settings'])) {
    foreach (element_children($form['theme_settings']) as $key) {
      if ($key != 'toggle_favicon') {
        $steps['header']['toggles'][$key] = $form['theme_settings'][$key];
      }
    }
  }

  // 5 Browser icon: core's favicon settings.
  $steps['icon'] = bootstrap_native_settings_step(5, t('Browser icon'), 'icon', bootstrap_native_settings_summary('icon', $values, $theme));
  if (isset($form['theme_settings']['toggle_favicon'])) {
    $steps['icon']['toggle_favicon'] = $form['theme_settings']['toggle_favicon'];
    $steps['icon']['toggle_favicon']['#title'] = t('Show a browser icon');
  }
  if (isset($form['favicon'])) {
    $favicon = theme_get_setting('favicon', $theme);
    $steps['icon']['current'] = array('#markup' => '<div class="bn-favicon">' . ($favicon ? '<img src="' . check_url($favicon) . '" alt="" width="32" height="32">' : '') . '<span class="bn-help">' . t('The small icon in browser tabs and bookmarks.') . '</span></div>');
    $steps['icon']['default_favicon'] = $form['favicon']['default_favicon'];
    $steps['icon']['default_favicon']['#title'] = t("Use Drupal's icon");
    unset($steps['icon']['default_favicon']['#description']);
    $steps['icon']['favicon_settings'] = $form['favicon']['settings'];
    unset($form['favicon']);
  }
  unset($form['theme_settings']);

  $form['bn_settings']['steps'] = $steps;
  // The sample the preview shows, kept across its updates (the script
  // sets it).
  $view = isset($form_state['input']['bn_preview_view']) && in_array($form_state['input']['bn_preview_view'], array('page', 'form', 'messages')) ? $form_state['input']['bn_preview_view'] : 'page';
  $form['bn_preview_view'] = array('#type' => 'hidden', '#value' => $view, '#attributes' => array('class' => array('bn-preview-view')));
  $form['bn_settings']['preview'] = bootstrap_native_settings_preview_pane($values, $tokens, $view);

  // The save bar: Save, whether anything changed, Preview (phones), and
  // Reset at the far end. Core adds its Save button to $form['actions']
  // after this alter (system_settings_form()), so the bar is made here.
  $form['actions']['#type'] = 'actions';
  $form['actions']['#attributes']['class'][] = 'bn-savebar';
  $form['actions']['#weight'] = 100;
  $form['actions']['unsaved'] = array(
    '#markup' => '<span class="bn-unsaved" aria-live="polite" data-saved="' . t('All changes saved') . '" data-unsaved="' . t('Unsaved changes') . '">' . t('All changes saved') . '</span>',
    '#weight' => 10,
  );
  $form['actions']['preview_open'] = array(
    '#markup' => '<button type="button" class="bn-preview-open">' . t('Preview') . '</button>',
    '#weight' => 15,
  );
  $form['actions']['reset'] = array(
    '#type' => 'submit',
    '#value' => t("Reset to Bootstrap's colors"),
    '#submit' => array('bootstrap_native_color_reset_submit'),
    '#limit_validation_errors' => array(),
    '#attributes' => array('class' => array('bn-reset'), 'data-confirm' => t("Clear all six colors and go back to Bootstrap's own? The logo and header settings stay.")),
    '#weight' => 20,
  );
}

/**
 * The colors as they are in the form now (typed, else saved).
 *
 * @return array
 *   Raw values (hex, name, or what was typed), keyed primary ... link_hover.
 */
function bootstrap_native_color_form_values($form_state, $theme) {
  $values = array();
  foreach (array_keys(bootstrap_native_color_fields()) as $key) {
    $name = 'bootstrap_native_color_' . $key;
    if (isset($form_state['input'][$name])) {
      $values[$key] = trim((string) $form_state['input'][$name]);
    }
    elseif (isset($form_state['values'][$name])) {
      $values[$key] = $form_state['values'][$name];
    }
    else {
      $values[$key] = (string) theme_get_setting($name, $theme);
    }
  }
  return $values;
}

/**
 * Only the values that are colors (others count as empty).
 */
function bootstrap_native_color_valid(array $values) {
  foreach ($values as $key => $value) {
    if (!bootstrap_native_color_parse($value)) {
      $values[$key] = '';
    }
  }
  return $values;
}

/**
 * The color fields: title, description, and what "automatic" means.
 */
function bootstrap_native_color_fields() {
  return array(
    'primary' => array(
      'title' => t('Primary color'),
      'description' => t('Buttons, checkboxes, the active tab, focus rings. A hex value or a Bootstrap color name: <code>orange</code>, <code>indigo-600</code>.'),
      'automatic' => t("Bootstrap's blue"),
    ),
    'secondary' => array(
      'title' => t('Secondary color'),
      'description' => t('Accents: a second button style, highlights.'),
      'automatic' => t("Bootstrap's indigo"),
    ),
    'base' => array(
      'title' => t('Background'),
      'description' => t('A dark color switches the site to its dark look.'),
      'automatic' => t('White'),
    ),
    'text' => array(
      'title' => t('Text'),
      'description' => t('Near black on a light background, near white on a dark one.'),
      'automatic' => t('near black'),
    ),
    'link' => array(
      'title' => t('Link'),
      'description' => t('The primary color, made readable on the background.'),
      'automatic' => t("Bootstrap's blue"),
    ),
    'link_hover' => array(
      'title' => t('Link hover'),
      'description' => t('A step further from the background than the link.'),
      'automatic' => t('a step darker than the link'),
    ),
  );
}

/**
 * A step: a <details> with a numbered heading and a one-line summary.
 */
function bootstrap_native_settings_step($number, $title, $id, $summary, $first = FALSE) {
  return array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bn-step'), 'id' => 'bn-step-' . $id),
    '#prefix' => '<details class="bn-details" open' . ($first ? ' data-first="1"' : '') . '><summary class="bn-summary"><span class="bn-number">' . $number . '</span><span class="bn-step-title">' . $title . '</span><span class="bn-step-summary" id="bn-sum-' . $id . '">' . $summary . '</span></summary>',
    '#suffix' => '</details>',
  );
}

/**
 * What a closed step shows it is set to (phones).
 */
function bootstrap_native_settings_summary($step, array $values, $theme) {
  $auto = t('Automatic');
  switch ($step) {
    case 'brand':
      $parts = array();
      if (theme_get_setting('logo_path', $theme) || theme_get_setting('default_logo', $theme)) {
        $parts[] = t('Logo');
      }
      $parts[] = $values['primary'] !== '' ? $values['primary'] : $auto;
      if ($values['secondary'] !== '') {
        $parts[] = $values['secondary'];
      }
      return check_plain(implode(' · ', $parts));

    case 'page':
      return check_plain(($values['base'] !== '' ? $values['base'] : t('White')) . ' · ' . ($values['text'] !== '' ? $values['text'] : t('automatic text')));

    case 'links':
      return check_plain($values['link'] !== '' || $values['link_hover'] !== '' ? implode(' · ', array_filter(array($values['link'], $values['link_hover']))) : $auto);

    case 'header':
      $on = array();
      foreach (array('logo' => t('Logo'), 'name' => t('name'), 'slogan' => t('slogan'), 'main_menu' => t('main menu'), 'secondary_menu' => t('secondary menu')) as $key => $label) {
        if (theme_get_setting('toggle_' . $key, $theme)) {
          $on[] = $label;
        }
      }
      return check_plain($on ? drupal_ucfirst(implode(', ', $on)) : t('Nothing'));

    case 'icon':
      return theme_get_setting('default_favicon', $theme) ? t("Drupal's") : t('Your own');
  }
  return '';
}

/**
 * The logo part of the Brand step (core's logo settings, moved).
 */
function bootstrap_native_settings_logo(&$form, $theme) {
  $element = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bn-logo')),
  );
  if (!isset($form['logo'])) {
    return $element;
  }
  $logo = theme_get_setting('logo_path', $theme) || theme_get_setting('default_logo', $theme) ? theme_get_setting('logo', $theme) : '';
  $element['current'] = array('#markup' => '<div class="bn-logo-current">' . ($logo ? '<img src="' . check_url($logo) . '" alt="' . t('Current logo') . '">' : '<span>' . t('No logo') . '</span>') . '</div>');
  $element['fields'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bn-logo-fields')),
  );
  $element['fields']['logo_upload'] = $form['logo']['settings']['logo_upload'];
  $element['fields']['logo_upload']['#title'] = t('Logo');
  $element['fields']['logo_upload']['#description'] = t('Shown in the header. SVG or PNG, about 200 × 60 px.');
  if ($logo) {
    $element['fields']['bn_logo_remove'] = array(
      '#type' => 'checkbox',
      '#title' => t('Remove the logo'),
      '#element_validate' => array('bootstrap_native_settings_logo_remove'),
    );
  }
  $element['fields']['more'] = array(
    '#type' => 'fieldset',
    '#title' => t('A file already on the server'),
    '#collapsible' => TRUE,
    '#collapsed' => !theme_get_setting('logo_path', $theme),
    'logo_path' => $form['logo']['settings']['logo_path'],
  );
  // The theme has no logo of its own ("default logo"): keep core's value.
  $element['fields']['default_logo'] = $form['logo']['default_logo'];
  $element['fields']['default_logo']['#access'] = FALSE;
  unset($form['logo']);
  return $element;
}

/**
 * Element validation: "Remove the logo" empties core's logo path.
 */
function bootstrap_native_settings_logo_remove($element, &$form_state) {
  if (!empty($element['#value'])) {
    form_set_value(array('#parents' => array('logo_path')), '', $form_state);
    form_set_value(array('#parents' => array('default_logo')), 0, $form_state);
  }
}

/**
 * Color fields: a text field (hex or a Bootstrap name) and its note.
 *
 * The script adds the picker and the Set / Automatic buttons; the note
 * says what the theme uses (automatic, a name's hex, an adjustment).
 */
function bootstrap_native_settings_colors(array $keys, array $values, array $tokens) {
  $fields = bootstrap_native_color_fields();
  $element = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bn-colors')),
  );
  foreach ($keys as $key) {
    $name = 'bootstrap_native_color_' . $key;
    $element[$name] = array(
      '#type' => 'textfield',
      '#title' => $fields[$key]['title'],
      '#default_value' => $values[$key],
      '#size' => 14,
      '#maxlength' => 24,
      '#attributes' => array(
        'class' => array('bootstrap-native-color'),
        'data-key' => $key,
        'spellcheck' => 'false',
        'autocomplete' => 'off',
        'aria-describedby' => 'bn-note-' . $key,
      ),
      '#prefix' => '<div class="bn-color" data-key="' . $key . '">',
      '#suffix' => bootstrap_native_settings_color_note($key, $values, $tokens) . '<div class="bn-help">' . $fields[$key]['description'] . '</div></div>',
      '#element_validate' => array('bootstrap_native_color_validate'),
      '#ajax' => array(
        'callback' => 'bootstrap_native_color_preview_ajax',
        'event' => 'change',
        'progress' => array('type' => 'none'),
      ),
    );
  }
  return $element;
}

/**
 * The line under a color: what it is, what the theme does with it.
 */
function bootstrap_native_settings_color_note($key, array $values, array $tokens) {
  $fields = bootstrap_native_color_fields();
  $value = $values[$key];
  $vars = $tokens['vars'];
  $used = array(
    'primary' => '--bs-primary-base',
    'secondary' => '--bs-accent-base',
    'base' => '--bs-bg-body',
    'text' => '--bs-fg-body',
    'link' => '--bs-link-color',
    'link_hover' => '--bs-link-hover-color',
  );
  $hex = isset($vars[$used[$key]]) ? $vars[$used[$key]] : '';
  if ($hex === '') {
    // Left to Bootstrap: its own values, worked out the same way (so "Set"
    // starts from what the page shows).
    $defaults = bootstrap_native_color_valid($values);
    $defaults += array('primary' => '', 'secondary' => '', 'base' => '');
    foreach (array('primary' => 'blue', 'secondary' => 'indigo', 'base' => '#ffffff') as $k => $v) {
      if ($defaults[$k] === '') {
        $defaults[$k] = $v;
      }
    }
    $all = bootstrap_native_color_tokens($defaults);
    $hex = isset($all['vars'][$used[$key]]) ? $all['vars'][$used[$key]] : '';
  }
  $swatch = function ($color) {
    return '<span class="bn-swatch" style="background:' . $color . '"></span>';
  };
  $out = '';
  $class = 'bn-color-note';
  $state = 'auto';
  if ($value === '') {
    $from = array('link' => t('from primary'), 'link_hover' => t('from the link'), 'text' => t('from the background'));
    $what = isset($vars[$used[$key]]) ? (isset($from[$key]) ? $from[$key] : '') : $fields[$key]['automatic'];
    $out = $hex
      ? t('Automatic: <strong>@hex</strong>', array('@hex' => $hex)) . ($what !== '' ? ' (' . $what . ')' : '')
      : t('Automatic: @what', array('@what' => $fields[$key]['automatic']));
  }
  elseif (!($rgb = bootstrap_native_color_parse($value))) {
    $state = 'error';
    $class .= ' is-error';
    $out = t('Use # and 6 digits (#1d3557) or a Bootstrap color name (indigo, indigo-600). The preview keeps the last good color.');
  }
  else {
    $state = 'set';
    if (bootstrap_native_color_is_name($value) && bootstrap_native_color_named($value)) {
      $out = t("Bootstrap's <strong>@name</strong> = @hex", array('@name' => $value, '@hex' => bootstrap_native_color_hex($rgb))) . ' ';
    }
    if (isset($tokens['notes'][$key])) {
      $note = $tokens['notes'][$key];
      $class .= ' is-adjusted';
      $out .= $swatch($note['from']) . '<span aria-hidden="true">→</span>' . $swatch($note['to']) . ' '
        . t('As text and links: <strong>@to</strong>, so it reads on the background (@ratio:1).', array('@to' => $note['to'], '@ratio' => $note['ratio']))
        . (in_array($key, array('primary', 'secondary')) ? ' ' . t('Buttons keep @from.', array('@from' => $note['from'])) : '');
    }
  }
  $resolved = $state == 'set' ? bootstrap_native_color_hex(bootstrap_native_color_parse($value)) : $hex;
  return '<div class="' . $class . '" id="bn-note-' . $key . '" data-state="' . $state . '" data-hex="' . check_plain($resolved) . '" role="status">' . $out . '</div>';
}

/**
 * Element validation: a hex color, a Bootstrap name, or empty.
 *
 * Hex is stored as lowercase #rrggbb, a name as typed in lowercase.
 */
function bootstrap_native_color_validate($element, &$form_state) {
  $value = strtolower(trim($element['#value']));
  if ($value === '') {
    form_set_value($element, '', $form_state);
    return;
  }
  bootstrap_native_color_load_palette();
  $rgb = bootstrap_native_color_parse($value);
  // A preview update (a color field's AJAX) only shows the problem in the
  // note; Save reports it.
  $preview = isset($form_state['triggering_element']['#type']) && $form_state['triggering_element']['#type'] == 'textfield';
  if (!$rgb && $preview) {
    return;
  }
  if (!$rgb) {
    form_error($element, t('%name: use # and 6 digits (#1d3557) or a Bootstrap color name (indigo, indigo-600), or leave it empty for automatic.', array('%name' => $element['#title'])));
    return;
  }
  form_set_value($element, bootstrap_native_color_is_name($value) && bootstrap_native_color_named($value) ? $value : bootstrap_native_color_hex($rgb), $form_state);
}

/**
 * AJAX callback: the preview, the notes and the summaries for the colors
 * in the form.
 */
function bootstrap_native_color_preview_ajax($form, $form_state) {
  $theme = isset($form_state['build_info']['args'][0]) ? $form_state['build_info']['args'][0] : 'bootstrap_native';
  bootstrap_native_color_load_palette();
  $values = bootstrap_native_color_form_values($form_state, $theme);
  $tokens = bootstrap_native_color_tokens(bootstrap_native_color_valid($values));
  $view = isset($form_state['input']['bn_preview_view']) && in_array($form_state['input']['bn_preview_view'], array('page', 'form', 'messages')) ? $form_state['input']['bn_preview_view'] : 'page';
  $commands = array(ajax_command_replace('#bootstrap-native-preview', bootstrap_native_color_preview($tokens, $view)));
  foreach (array_keys(bootstrap_native_color_fields()) as $key) {
    $commands[] = ajax_command_replace('#bn-note-' . $key, bootstrap_native_settings_color_note($key, $values, $tokens));
  }
  foreach (array('brand', 'page', 'links') as $step) {
    $commands[] = ajax_command_html('#bn-sum-' . $step, bootstrap_native_settings_summary($step, $values, $theme));
  }
  return array('#type' => 'ajax', '#commands' => $commands);
}

/**
 * Form submit: Reset to Bootstrap's colors (the six colors only).
 */
function bootstrap_native_color_reset_submit($form, &$form_state) {
  $theme = $form_state['build_info']['args'][0];
  $settings = variable_get('theme_' . $theme . '_settings', array());
  foreach (array_keys(bootstrap_native_color_fields()) as $key) {
    $settings['bootstrap_native_color_' . $key] = '';
  }
  variable_set('theme_' . $theme . '_settings', $settings);
  drupal_set_message(t("The colors are back to Bootstrap's own."));
}

/**
 * The preview pane: the sample switch, the frame, a line about it.
 */
function bootstrap_native_settings_preview_pane(array $values, array $tokens, $view = 'page') {
  $views = array('page' => t('Page'), 'form' => t('Form'), 'messages' => t('Messages'));
  $buttons = '';
  foreach ($views as $key => $label) {
    $buttons .= '<button type="button" class="bn-view" data-view="' . $key . '" aria-pressed="' . ($key == $view ? 'true' : 'false') . '">' . $label . '</button>';
  }
  return array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bn-preview-pane'), 'aria-label' => t('Preview'), 'role' => 'region'),
    'bar' => array('#markup' => '<div class="bn-preview-bar"><h2 class="bn-preview-title">' . t('Preview') . '</h2><div class="bn-views" role="group" aria-label="' . t('Preview sample') . '">' . $buttons . '</div><button type="button" class="bn-preview-close">' . t('Close') . '</button></div>'),
    'frame' => array('#markup' => bootstrap_native_color_preview($tokens, $view)),
    'help' => array('#markup' => '<p class="bn-help">' . t('A sample, not the live site: nothing in it is clickable. Colors update as you change them; Save applies them to every page.') . '</p>'),
  );
}

/**
 * The preview: sample page parts in a frame with the site's stylesheet.
 */
function bootstrap_native_color_preview(array $tokens, $view = 'page') {
  // The stylesheet the site uses (a build from the Bootstrap updates page,
  // or the theme's own).
  $files = function_exists('bootstrap_native_update_files') ? bootstrap_native_update_files() : NULL;
  $css_url = file_create_url($files ? $files['css'] : drupal_get_path('theme', 'bootstrap_native') . '/css/bootstrap_native.css') . '?' . variable_get('css_js_query_string', '0');
  $document = '<!DOCTYPE html><html lang="en" data-bs-theme="' . $tokens['scheme'] . '" data-preview="' . $view . '"><head><meta charset="utf-8">'
    . '<link rel="stylesheet" href="' . check_plain(file_create_url('modules/system/system.base.css')) . '">'
    . '<link rel="stylesheet" href="' . check_plain($css_url) . '">'
    . '<style>' . bootstrap_native_color_css($tokens) . bootstrap_native_color_preview_css() . '</style></head><body>'
    . bootstrap_native_color_preview_sample() . '</body></html>';
  // The frame grows to its content (srcdoc frames are same-origin); the
  // script (js/theme-settings.js) switches samples and keeps clicks in it.
  return '<div id="bootstrap-native-preview" class="bootstrap-native-preview"><iframe title="' . t('Color preview') . '" srcdoc="' . check_plain($document) . '"></iframe></div>';
}

/**
 * Styles for the sample page (only inside the preview frame).
 */
function bootstrap_native_color_preview_css() {
  return 'body{min-height:0;margin:0}'
    . '.bn-s-header{display:flex;flex-wrap:wrap;gap:.5rem 1.25rem;align-items:center;padding:.75rem 1.25rem;border-bottom:1px solid var(--bs-border-color)}'
    . '.bn-s-logo{width:84px;height:28px;border-radius:4px;background:var(--bs-accent-base,#6610f2);color:#fff;font-size:11px;display:flex;align-items:center;justify-content:center}'
    . '.bn-s-menu{margin:0 0 0 auto;display:flex;gap:1rem;list-style:none;padding:0;font-size:.9rem}.bn-s-menu li{margin:0}'
    . '.bn-s-body{padding:1.25rem}'
    . '[data-sample]{display:none}html[data-preview=page] [data-sample=page],html[data-preview=form] [data-sample=form],html[data-preview=messages] [data-sample=messages]{display:block}'
    . 'a.hover{color:var(--bs-link-hover-color)}';
}

/**
 * Sample markup for the preview, written the way Drupal core writes it.
 */
function bootstrap_native_color_preview_sample() {
  $site = check_plain(variable_get('site_name', 'Drupal'));
  return <<<HTML
<div class="bn-s-header"><span class="bn-s-logo">[LOGO]</span><strong>$site</strong><ul class="bn-s-menu"><li><a href="#">About</a></li><li><a href="#">News</a></li><li><a href="#">Contact</a></li></ul></div>
<div class="bn-s-body">
<div data-sample="page">
<nav class="breadcrumb-wrapper"><ol class="breadcrumb"><li class="breadcrumb-item"><a href="#">Home</a></li><li class="breadcrumb-divider" aria-hidden="true">›</li><li class="breadcrumb-item">News</li></ol></nav>
<h1 class="page-title">[Page title]</h1>
<ul class="tabs primary nav nav-tabs"><li class="nav-item active"><a href="#" class="nav-link active">View</a></li><li class="nav-item"><a href="#" class="nav-link">Edit</a></li></ul>
<p>Body text with <a href="#">a link</a> and <a href="#" class="hover">a hovered link</a>. <span class="fg-3">Muted text for dates and notes.</span></p>
<div class="card"><div class="card-body"><p style="margin:0"><strong>A card</strong> on a panel shade, with <a href="#">a link</a>.</p></div></div>
</div>
<div data-sample="form">
<form onsubmit="return false">
<div class="form-item form-type-textfield"><label for="p-name">Name <span class="form-required" title="This field is required.">*</span></label><input type="text" id="p-name" value="Jane Doe" size="30" class="form-text required"><div class="description">As it should appear on the order.</div></div>
<div class="form-item form-type-select"><label for="p-size">Size</label><select id="p-size" class="form-select"><option>Medium</option></select></div>
<div class="form-item form-type-checkbox"><input type="checkbox" id="p-news" checked class="form-checkbox"> <label class="option" for="p-news">Email me about new products </label></div>
<div class="form-item form-type-radio"><input type="radio" id="p-r1" name="p-r" checked class="form-radio"> <label class="option" for="p-r1">Business </label></div>
<div class="form-actions"><input type="submit" value="Save" class="form-submit btn-solid theme-primary"> <input type="submit" value="Preview" class="form-submit btn-outline theme-secondary"> <button type="button" class="btn-solid theme-accent">Accent</button></div>
</form>
</div>
<div data-sample="messages">
<div class="messages status" role="status"><h2 class="element-invisible">Status message</h2>Your changes have been saved. <a href="#">Undo</a></div>
<div class="messages warning" role="status"><h2 class="element-invisible">Warning message</h2>Your session ends in 5 minutes.</div>
<div class="messages error" role="alert"><h2 class="element-invisible">Error message</h2>The email address is not valid.</div>
<div class="table-responsive"><table class="table"><thead><tr><th>Order</th><th>Status</th></tr></thead><tbody><tr class="odd"><td>[ORDER]</td><td>Shipped</td></tr><tr class="even"><td>[ORDER]</td><td>Processing</td></tr></tbody></table></div>
</div>
</div>
HTML;
}
