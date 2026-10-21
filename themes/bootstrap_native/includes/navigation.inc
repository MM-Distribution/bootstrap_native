<?php

/**
 * @file
 * Tabs, breadcrumb and tables: core's markup plus Bootstrap's classes.
 */

/**
 * Overrides theme_menu_local_tasks(): tabs as Bootstrap's nav.
 *
 * Core's ul.tabs.primary / ul.tabs.secondary stay; primary tabs are
 * nav-tabs, secondary ones nav-pills.
 */
function bootstrap_native_menu_local_tasks(&$variables) {
  $output = '';
  if (!empty($variables['primary'])) {
    $variables['primary']['#prefix'] = '<h2 class="element-invisible">' . t('Primary tabs') . '</h2>';
    $variables['primary']['#prefix'] .= '<ul class="tabs primary nav nav-tabs">';
    $variables['primary']['#suffix'] = '</ul>';
    $output .= drupal_render($variables['primary']);
  }
  if (!empty($variables['secondary'])) {
    $variables['secondary']['#prefix'] = '<h2 class="element-invisible">' . t('Secondary tabs') . '</h2>';
    $variables['secondary']['#prefix'] .= '<ul class="tabs secondary nav nav-pills">';
    $variables['secondary']['#suffix'] = '</ul>';
    $output .= drupal_render($variables['secondary']);
  }
  return $output ? '<div class="tabs-wrapper">' . $output . '</div>' : '';
}

/**
 * Overrides theme_menu_local_task(): li.nav-item, a.nav-link.
 */
function bootstrap_native_menu_local_task($variables) {
  $link = $variables['element']['#link'];
  $link_text = $link['title'];
  $link['localized_options']['attributes']['class'][] = 'nav-link';

  if (!empty($variables['element']['#active'])) {
    // Add text to indicate active tab for non-visual users.
    $active = '<span class="element-invisible">' . t('(active tab)') . '</span>';

    // If the link does not contain HTML already, check_plain() it now.
    // After we set 'html'=TRUE the link will not be sanitized by l().
    if (empty($link['localized_options']['html'])) {
      $link['title'] = check_plain($link['title']);
    }
    $link['localized_options']['html'] = TRUE;
    $link_text = t('!local-task-title!active', array('!local-task-title' => $link['title'], '!active' => $active));
    $link['localized_options']['attributes']['class'][] = 'active';
    $link['localized_options']['attributes']['aria-current'] = 'page';
  }

  return '<li class="nav-item' . (!empty($variables['element']['#active']) ? ' active' : '') . '">' . l($link_text, $link['href'], $link['localized_options']) . "</li>\n";
}

/**
 * Overrides theme_breadcrumb(): Bootstrap 6's breadcrumb markup.
 */
function bootstrap_native_breadcrumb($variables) {
  $breadcrumb = $variables['breadcrumb'];
  if (empty($breadcrumb)) {
    return '';
  }
  $items = array();
  foreach (array_values($breadcrumb) as $i => $crumb) {
    if ($i) {
      $items[] = '<li class="breadcrumb-divider" aria-hidden="true"></li>';
    }
    // Crumbs are rendered links from menu_get_active_breadcrumb() (or plain
    // text from a module); give links Bootstrap's class.
    if (preg_match('/^<a\b[^>]*\bclass="/', $crumb)) {
      $crumb = preg_replace('/^(<a\b[^>]*\bclass=")/', '$1breadcrumb-link ', $crumb, 1);
    }
    else {
      $crumb = preg_replace('/^<a\b/', '<a class="breadcrumb-link"', $crumb, 1);
    }
    $items[] = '<li class="breadcrumb-item">' . $crumb . '</li>';
  }
  return '<nav class="breadcrumb-wrapper" aria-label="' . t('You are here') . '"><ol class="breadcrumb">' . implode('', $items) . '</ol></nav>';
}

/**
 * Overrides theme_table(): Bootstrap's .table, in a scrolling wrapper.
 *
 * The table itself is core's (theme_table()), so tabledrag, sticky headers
 * and table sorting work as usual.
 */
function bootstrap_native_table($variables) {
  $variables['attributes']['class'][] = 'table';
  return '<div class="table-responsive">' . theme_table($variables) . '</div>';
}
