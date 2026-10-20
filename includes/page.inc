<?php

/**
 * @file
 * Page level: stylesheets, Bootstrap's JavaScript, colors, navbar.
 */

/**
 * The color settings of the active theme (a sub-theme inherits them).
 *
 * @return array
 *   Keys primary, secondary, base, text, link, link_hover: hex or ''.
 */
function bs_native_color_settings($theme = NULL) {
  $settings = array();
  foreach (array('primary', 'secondary', 'base', 'text', 'link', 'link_hover') as $key) {
    $settings[$key] = (string) theme_get_setting('bs_native_color_' . $key, $theme);
  }
  return $settings;
}

/**
 * Implements hook_css_alter().
 *
 * Drops core's look-only stylesheets; the theme styles the same markup
 * (system.base.css stays: it holds what core's JavaScript needs).
 */
function bs_native_css_alter(&$css) {
  foreach (array('modules/system/system.theme.css', 'modules/system/system.menus.css', 'modules/system/system.messages.css', 'misc/vertical-tabs.css') as $file) {
    unset($css[$file]);
  }
}

/**
 * Implements hook_preprocess_html().
 */
function bs_native_preprocess_html(&$variables) {
  $theme_path = drupal_get_path('theme', 'bs_native');

  // Colors from the theme settings: Bootstrap's variables, after its CSS.
  $tokens = bs_native_color_tokens(bs_native_color_settings());
  if ($tokens['vars']) {
    drupal_add_css(bs_native_color_css($tokens), array(
      'type' => 'inline',
      'group' => CSS_THEME,
      'weight' => 1000,
    ));
  }
  // data-bs-theme picks Bootstrap's light or dark values; without it the
  // page would follow the visitor's system setting, which sites aren't
  // designed for.
  $variables['html_attributes_array'] = array(
    'lang' => $GLOBALS['language']->language,
    'dir' => $GLOBALS['language']->direction ? 'rtl' : 'ltr',
    'data-bs-theme' => $tokens['scheme'],
  );

  drupal_add_html_head(array(
    '#tag' => 'meta',
    '#attributes' => array(
      'name' => 'viewport',
      'content' => 'width=device-width, initial-scale=1',
    ),
  ), 'bs_native_viewport');

  // Bootstrap 6's JavaScript is an ES module (no jQuery). drupal_add_js()
  // can't write type="module", so it goes in the head as a tag; module
  // scripts never block the page.
  $query = '?' . variable_get('css_js_query_string', '0');
  drupal_add_html_head(array(
    '#tag' => 'script',
    '#attributes' => array(
      'type' => 'module',
      'src' => file_create_url($theme_path . '/js/bs_native.js') . $query,
    ),
    '#value' => '',
  ), 'bs_native_bootstrap');
}

/**
 * Implements hook_process_html().
 */
function bs_native_process_html(&$variables) {
  $variables['html_attributes'] = drupal_attributes($variables['html_attributes_array']);
}

/**
 * Implements hook_preprocess_page().
 */
function bs_native_preprocess_page(&$variables) {
  // Main and secondary menus as Bootstrap navbar links.
  foreach (array('main_menu' => 'system_main_menu', 'secondary_menu' => 'system_secondary_menu') as $key => $hook) {
    $variables[$key . '_nav'] = '';
    if (!empty($variables[$key])) {
      $links = $variables[$key];
      foreach ($links as &$link) {
        $link['attributes']['class'][] = 'nav-link';
      }
      unset($link);
      $variables[$key . '_nav'] = theme('links__' . $hook, array(
        'links' => $links,
        'attributes' => array('class' => array('links', 'nav', 'navbar-nav')),
        'heading' => array(
          'text' => $key == 'main_menu' ? t('Main menu') : t('Secondary menu'),
          'level' => 'h2',
          'class' => array('element-invisible'),
        ),
      ));
    }
  }
  $variables['has_navbar_drawer'] = $variables['main_menu_nav'] || $variables['secondary_menu_nav'] || !empty($variables['page']['navigation']);
}
