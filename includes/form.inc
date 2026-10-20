<?php

/**
 * @file
 * Forms: Drupal's markup kept; buttons get Bootstrap's classes.
 *
 * Fields, labels, checkboxes, descriptions and fieldsets are not
 * overridden at all: the stylesheet styles core's markup (see
 * scss/_drupal-forms.scss).
 */

/**
 * Overrides theme_button(): Bootstrap's button classes.
 *
 * The style follows what the button does, from its form key (the same in
 * every language, unlike its label):
 * - delete, remove: outlined, danger color;
 * - submit, save and the other "go ahead" keys: solid, primary color;
 * - anything else (preview, cancel, add another, upload): outlined.
 * A button that already has a Bootstrap class (btn-*) keeps it.
 */
function bs_native_button($variables) {
  $element = &$variables['element'];
  $classes = isset($element['#attributes']['class']) ? (array) $element['#attributes']['class'] : array();
  if (!preg_grep('/^btn-/', $classes)) {
    $element['#attributes']['class'] = array_merge($classes, bs_native_button_classes($element));
  }
  return theme_button($variables);
}

/**
 * The Bootstrap classes for a form button.
 */
function bs_native_button_classes(array $element) {
  $key = '';
  if (!empty($element['#array_parents'])) {
    $key = end($element['#array_parents']);
  }
  elseif (!empty($element['#parents'])) {
    $key = end($element['#parents']);
  }
  $key = strtolower((string) $key);
  if (preg_match('/(^|_)(delete|remove)($|_)/', $key)) {
    return array('btn-outline', 'theme-danger');
  }
  $primary = array('submit', 'save', 'next', 'continue', 'finish', 'confirm', 'login', 'register', 'send', 'search', 'apply', 'checkout', 'update');
  if (in_array($key, $primary) || (isset($element['#button_type']) && $element['#button_type'] == 'primary')) {
    return array('btn-solid', 'theme-primary');
  }
  return array('btn-outline', 'theme-secondary');
}

/**
 * Overrides theme_status_messages(): core's markup plus a role.
 *
 * Errors are announced at once (role="alert"), others politely
 * (role="status").
 */
function bs_native_status_messages($variables) {
  $display = $variables['display'];
  $output = '';

  $status_heading = array(
    'status' => t('Status message'),
    'error' => t('Error message'),
    'warning' => t('Warning message'),
  );
  foreach (drupal_get_messages($display) as $type => $messages) {
    $output .= '<div class="messages ' . drupal_html_class($type) . '" role="' . ($type == 'error' ? 'alert' : 'status') . '">' . "\n";
    if (!empty($status_heading[$type])) {
      $output .= '<h2 class="element-invisible">' . $status_heading[$type] . "</h2>\n";
    }
    if (count($messages) > 1) {
      $output .= " <ul>\n";
      foreach ($messages as $message) {
        $output .= '  <li>' . $message . "</li>\n";
      }
      $output .= " </ul>\n";
    }
    else {
      $output .= reset($messages);
    }
    $output .= "</div>\n";
  }
  return $output;
}
