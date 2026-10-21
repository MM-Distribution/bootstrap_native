<?php

/**
 * @file
 * Bootstrap Native Admin: the administration theme.
 *
 * Its page is a band (View site, breadcrumb, title, actions, primary tabs)
 * over the content; everything else comes from Bootstrap Native.
 */

/**
 * Implements hook_preprocess_page().
 *
 * Primary tabs go in the band, secondary ones under it.
 */
function bootstrap_native_admin_preprocess_page(&$variables) {
  $tabs = isset($variables['tabs']) ? $variables['tabs'] : array();
  $variables['tabs_primary'] = $tabs;
  $variables['tabs_secondary'] = $tabs;
  if (is_array($tabs)) {
    $variables['tabs_primary']['#secondary'] = array();
    $variables['tabs_secondary']['#primary'] = array();
  }
}
