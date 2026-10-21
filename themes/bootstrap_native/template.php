<?php

/**
 * @file
 * Bootstrap Native: Bootstrap 6 on Drupal's own markup.
 *
 * The rule of this theme: keep the markup Drupal core and modules write
 * (structure, classes, IDs), since modules and their JavaScript rely on
 * it. Bootstrap's look comes from the stylesheet, which extends Bootstrap's
 * rules onto Drupal's classes (scss/). Overrides here only add Bootstrap
 * classes or accessibility attributes, and call core's own function for the
 * rest wherever possible.
 */

foreach (array('color', 'page', 'form', 'navigation') as $include) {
  require_once dirname(__FILE__) . '/includes/' . $include . '.inc';
}
