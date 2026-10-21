<?php

/**
 * @file
 * Theme settings: colors, with a live preview.
 *
 * Only a few colors to choose; the theme works out the rest (shades,
 * borders, text on buttons, readable link colors; see includes/color.inc).
 * Every color is optional: empty means Bootstrap's own.
 */

require_once dirname(__FILE__) . '/includes/color.inc';

/**
 * Implements hook_form_FORM_ID_alter() for system_theme_settings.
 */
function bootstrap_native_form_system_theme_settings_alter(&$form, &$form_state, $form_id = NULL) {
  // Runs once per base theme as well (sub-themes): add the settings once.
  if (isset($form['bootstrap_native_colors'])) {
    return;
  }
  // AJAX rebuilds load the form from cache; this file must come along.
  $form_state['build_info']['files']['bootstrap_native'] = drupal_get_path('theme', 'bootstrap_native') . '/theme-settings.php';
  $theme = isset($form_state['build_info']['args'][0]) ? $form_state['build_info']['args'][0] : 'bootstrap_native';

  $fields = bootstrap_native_color_fields();
  $form['bootstrap_native_colors'] = array(
    '#type' => 'fieldset',
    '#title' => t('Colors'),
    '#description' => t('Pick a few colors; the theme works out the rest (lighter and darker shades, borders, the text on buttons). Empty fields use Bootstrap\'s own colors. Colors used as text are adjusted, if needed, so they stay readable on the base color; the preview lists any adjustment.'),
    '#weight' => -20,
    '#attributes' => array('class' => array('bootstrap-native-colors')),
    '#attached' => array(
      'css' => array(drupal_get_path('theme', 'bootstrap_native') . '/css/theme-settings.css'),
      'js' => array(drupal_get_path('theme', 'bootstrap_native') . '/js/theme-settings.js'),
    ),
  );
  $form['bootstrap_native_colors']['fields'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bootstrap-native-color-fields')),
  );
  foreach ($fields as $key => $info) {
    $name = 'bootstrap_native_color_' . $key;
    $value = isset($form_state['values'][$name]) ? $form_state['values'][$name] : theme_get_setting($name, $theme);
    $form['bootstrap_native_colors']['fields'][$name] = array(
      '#type' => 'textfield',
      '#title' => $info['title'],
      '#description' => $info['description'],
      '#default_value' => $value,
      '#size' => 9,
      '#maxlength' => 7,
      '#attributes' => array(
        'class' => array('bootstrap-native-color'),
        'placeholder' => $info['placeholder'],
        'data-default' => $info['default'],
        'spellcheck' => 'false',
        'autocomplete' => 'off',
      ),
      '#element_validate' => array('bootstrap_native_color_validate'),
      '#ajax' => array(
        'callback' => 'bootstrap_native_color_preview_ajax',
        'wrapper' => 'bootstrap-native-preview',
        'event' => 'change',
        'progress' => array('type' => 'none'),
      ),
    );
  }

  $form['bootstrap_native_colors']['preview'] = array(
    '#type' => 'markup',
    '#markup' => bootstrap_native_color_preview($form_state, $theme),
  );
}

/**
 * The color fields: title, description, placeholder, default swatch.
 */
function bootstrap_native_color_fields() {
  return array(
    'primary' => array(
      'title' => t('Primary color'),
      'description' => t('The main brand color: buttons, checkboxes, focus outlines, active menu items, links (unless set below).'),
      'placeholder' => t('Bootstrap blue'),
      'default' => '#0d6efd',
    ),
    'secondary' => array(
      'title' => t('Secondary color'),
      'description' => t('A second brand color for accents (Bootstrap\'s "accent" classes, e.g. a <code>btn-solid theme-accent</code> button).'),
      'placeholder' => t('Bootstrap indigo'),
      'default' => '#6610f2',
    ),
    'base' => array(
      'title' => t('Base color'),
      'description' => t('The page background. Panels, borders and muted text are shades between it and the text color. A dark base color switches the theme to its dark scheme.'),
      'placeholder' => t('White'),
      'default' => '#ffffff',
    ),
    'text' => array(
      'title' => t('Text color'),
      'description' => t('Empty: near black on a light base, near white on a dark one.'),
      'placeholder' => t('Automatic'),
      'default' => '#212529',
    ),
    'link' => array(
      'title' => t('Link color'),
      'description' => t('Empty: the primary color (darkened or lightened if needed to stay readable).'),
      'placeholder' => t('Automatic'),
      'default' => '#0d6efd',
    ),
    'link_hover' => array(
      'title' => t('Link hover color'),
      'description' => t('Empty: a step darker than the link color (lighter on a dark base).'),
      'placeholder' => t('Automatic'),
      'default' => '#0a58ca',
    ),
  );
}

/**
 * Element validation: a hex color or empty; stored as lowercase #rrggbb.
 */
function bootstrap_native_color_validate($element, &$form_state) {
  $value = trim($element['#value']);
  if ($value === '') {
    form_set_value($element, '', $form_state);
    return;
  }
  $rgb = bootstrap_native_color_parse($value);
  if (!$rgb) {
    form_error($element, t('%name must be a hex color like #1d3557 (or empty).', array('%name' => $element['#title'])));
    return;
  }
  form_set_value($element, bootstrap_native_color_hex($rgb), $form_state);
}

/**
 * AJAX callback: the preview for the colors in the form.
 */
function bootstrap_native_color_preview_ajax($form, $form_state) {
  return $form['bootstrap_native_colors']['preview'];
}

/**
 * The preview: sample page parts in a frame with the theme's stylesheet,
 * and the list of colors the theme adjusted.
 */
function bootstrap_native_color_preview($form_state, $theme) {
  $settings = array();
  foreach (array_keys(bootstrap_native_color_fields()) as $key) {
    $name = 'bootstrap_native_color_' . $key;
    $value = isset($form_state['values'][$name]) ? $form_state['values'][$name] : (isset($form_state['input'][$name]) ? $form_state['input'][$name] : theme_get_setting($name, $theme));
    $settings[$key] = bootstrap_native_color_parse($value) ? $value : '';
  }
  $tokens = bootstrap_native_color_tokens($settings);

  // The stylesheet the site uses (a build from the Bootstrap updates page,
  // or the theme's own).
  $files = function_exists('bootstrap_native_update_files') ? bootstrap_native_update_files() : NULL;
  $css_url = file_create_url($files ? $files['css'] : drupal_get_path('theme', 'bootstrap_native') . '/css/bootstrap_native.css') . '?' . variable_get('css_js_query_string', '0');
  $document = '<!DOCTYPE html><html lang="en" data-bs-theme="' . $tokens['scheme'] . '"><head><meta charset="utf-8">'
    . '<link rel="stylesheet" href="' . check_plain(file_create_url('modules/system/system.base.css')) . '">'
    . '<link rel="stylesheet" href="' . check_plain($css_url) . '">'
    . '<style>' . bootstrap_native_color_css($tokens) . 'body{min-height:0;padding:1.25rem}a.hover{color:var(--bs-link-hover-color)}</style></head><body>'
    . bootstrap_native_color_preview_sample() . '</body></html>';

  $fields = bootstrap_native_color_fields();
  $notes = array();
  foreach ($tokens['notes'] as $key => $note) {
    $notes[] = t('@name: text in @to instead of @from, so it reads well on the base color (contrast @ratio:1).', array(
      '@name' => $fields[$key]['title'],
      '@from' => $note['from'],
      '@to' => $note['to'],
      '@ratio' => $note['ratio'],
    )) . ' <span class="bootstrap-native-swatch" style="background:' . $note['from'] . '"></span><span class="bootstrap-native-swatch" style="background:' . $note['to'] . '"></span>';
  }

  $output = '<div id="bootstrap-native-preview" class="bootstrap-native-preview">';
  $output .= '<h3>' . t('Preview') . '</h3>';
  // The frame grows to its content (srcdoc frames are same-origin).
  $output .= '<iframe title="' . t('Color preview') . '" srcdoc="' . check_plain($document) . '" onload="this.style.height = this.contentDocument.documentElement.scrollHeight + \'px\'"></iframe>';
  if ($notes) {
    $output .= '<div class="bootstrap-native-notes" role="status"><p>' . t('Adjusted to stay readable:') . '</p><ul><li>' . implode('</li><li>', $notes) . '</li></ul></div>';
  }
  $output .= '</div>';
  return $output;
}

/**
 * Sample markup for the preview, written the way Drupal core writes it.
 */
function bootstrap_native_color_preview_sample() {
  return <<<'HTML'
<div class="messages status" role="status"><h2 class="element-invisible">Status message</h2>Your changes have been saved. <a href="#">Undo</a></div>
<p>Body text with <a href="#">a link</a> and <a href="#" class="hover">a hovered link</a>. <span class="fg-3">Muted text for dates and notes.</span></p>
<ul class="tabs primary nav nav-tabs"><li class="nav-item active"><a href="#" class="nav-link active">View</a></li><li class="nav-item"><a href="#" class="nav-link">Edit</a></li></ul>
<form onsubmit="return false" style="margin-top:1rem">
<div class="form-item form-type-textfield"><label for="p-name">Name <span class="form-required" title="This field is required.">*</span></label><input type="text" id="p-name" value="Jane Doe" size="30" class="form-text required"><div class="description">As it should appear on the order.</div></div>
<div class="form-item form-type-checkbox"><input type="checkbox" id="p-news" checked class="form-checkbox"> <label class="option" for="p-news">Email me about new products </label></div>
<div class="form-item form-type-radio"><input type="radio" id="p-r1" name="p-r" checked class="form-radio"> <label class="option" for="p-r1">Business </label></div>
<div class="form-actions"><input type="submit" value="Save" class="form-submit btn-solid theme-primary"> <input type="submit" value="Preview" class="form-submit btn-outline theme-secondary"> <button type="button" class="btn-solid theme-accent">Accent</button></div>
</form>
<div class="card" style="margin-top:.5rem"><div class="card-body"><strong>A card</strong> on a panel shade, with <a href="#">a link</a>.</div></div>
HTML;
}
