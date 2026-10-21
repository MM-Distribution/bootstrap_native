<?php

/**
 * @file
 * Bootstrap's parts per theme.
 *
 * Every Bootstrap Native theme uses the site's one release. Which parts it
 * gets, first match wins:
 * 1. Fixed in its .info (bootstrap_native[parts_fixed] = TRUE, with
 *    bootstrap_native[parts][] and bootstrap_native[utilities][]): the
 *    admin theme, built for its own pages.
 * 2. Its own choice, saved with its theme settings (bootstrap_native_parts).
 * 3. The site-wide choice (Appearance > Bootstrap components).
 * Parts a theme's .info needs are always in, parts that clash with Drupal
 * always out. Themes with the same parts share a build.
 *
 * The site-wide build is bootstrap_native_update_active. Themes with own or
 * fixed parts have theirs in bootstrap_native_update_theme_builds (theme =>
 * build; '' = the theme's own files); a theme with its own choice that was
 * never applied uses the site-wide build. bootstrap_native_update_previous_
 * theme_builds is the map before the last switch (roll back).
 */

/**
 * Which parts a theme uses: 'fixed', 'own' or 'site'.
 */
function bootstrap_native_update_parts_mode($name) {
  $themes = list_themes();
  if (!isset($themes[$name])) {
    return 'site';
  }
  if (!empty($themes[$name]->info['bootstrap_native']['parts_fixed'])) {
    return 'fixed';
  }
  return bootstrap_native_update_own_selection($name) !== NULL ? 'own' : 'site';
}

/**
 * A theme's own choice, as saved with its settings (not inherited), or NULL.
 */
function bootstrap_native_update_own_selection($name) {
  $settings = variable_get('theme_' . $name . '_settings', array());
  $own = isset($settings['bootstrap_native_parts']) ? $settings['bootstrap_native_parts'] : NULL;
  return is_array($own) && isset($own['parts']) && is_array($own['parts']) ? $own + array('utilities' => array()) : NULL;
}

/**
 * The parts a theme's .info fixes.
 */
function bootstrap_native_update_fixed_selection($name) {
  $themes = list_themes();
  $own = isset($themes[$name]->info['bootstrap_native']) ? $themes[$name]->info['bootstrap_native'] : array();
  return array(
    'parts' => isset($own['parts']) ? array_values((array) $own['parts']) : array(),
    'utilities' => isset($own['utilities']) ? array_values((array) $own['utilities']) : array(),
  );
}

/**
 * The theme and its base themes, Bootstrap Native left out.
 */
function bootstrap_native_update_theme_chain($name) {
  $themes = list_themes();
  if (!isset($themes[$name])) {
    return array();
  }
  $chain = array_keys(drupal_find_base_themes($themes, $name));
  $chain[] = $name;
  return array_values(array_diff($chain, array('bootstrap_native')));
}

/**
 * Enabled Bootstrap Native themes with their own or fixed parts.
 *
 * @return array
 *   Modes ('own' or 'fixed'), keyed by theme name.
 */
function bootstrap_native_update_separate_themes() {
  $separate = array();
  foreach (array_keys(bootstrap_native_update_themes_in_use()) as $name) {
    $mode = bootstrap_native_update_parts_mode($name);
    if ($mode != 'site') {
      $separate[$name] = $mode;
    }
  }
  return $separate;
}

/**
 * Enabled Bootstrap Native themes that use the site-wide choice.
 */
function bootstrap_native_update_site_themes() {
  return array_values(array_diff(array_keys(bootstrap_native_update_themes_in_use()), array_keys(bootstrap_native_update_separate_themes())));
}

/**
 * The applied builds of themes with own or fixed parts (theme => build).
 */
function bootstrap_native_update_theme_builds() {
  return variable_get('bootstrap_native_update_theme_builds', array());
}

/**
 * The build a theme's pages use (with the site-wide build given, or the
 * active one).
 *
 * @param string $name
 *   The theme.
 * @param string|null $site_build
 *   A site-wide build being previewed: its theme builds are used.
 */
function bootstrap_native_update_build_for_theme($name, $site_build = NULL) {
  $builds = bootstrap_native_update_builds();
  if ($site_build !== NULL) {
    $map = isset($builds[$site_build]['theme_builds']) ? $builds[$site_build]['theme_builds'] : array();
  }
  else {
    $site_build = bootstrap_native_update_active();
    $map = bootstrap_native_update_theme_builds();
  }
  if (isset($map[$name]) && bootstrap_native_update_parts_mode($name) != 'site' && ($map[$name] === '' || !empty($builds[$map[$name]]['meta']))) {
    return $map[$name];
  }
  return $site_build;
}

/**
 * The folders the checks look at for a theme's build (classes in
 * templates): Bootstrap Native and the theme's chain; NULL for the
 * site-wide build (Bootstrap Native and the themes using it).
 */
function bootstrap_native_update_check_themes_for($name = NULL) {
  $names = $name === NULL ? bootstrap_native_update_site_themes() : bootstrap_native_update_theme_chain($name);
  $themes = array('bootstrap_native' => bootstrap_native_update_theme_dir());
  $list = list_themes();
  foreach ($names as $theme) {
    if ($theme != 'bootstrap_native' && isset($list[$theme])) {
      $themes[$theme] = DRUPAL_ROOT . '/' . dirname($list[$theme]->filename);
    }
  }
  return $themes;
}

/**
 * Builds a release for every theme with own or fixed parts.
 *
 * @return array
 *   Build names keyed by theme (a failed build is listed too; its check
 *   says why).
 */
function bootstrap_native_update_make_theme_builds($version) {
  $map = array();
  foreach (array_keys(bootstrap_native_update_separate_themes()) as $name) {
    $made = bootstrap_native_update_make_build($version, FALSE, $name);
    $map[$name] = $made['build'];
  }
  return $map;
}

/**
 * Checks each theme build of a site-wide build against what that theme
 * uses now.
 */
function bootstrap_native_update_check_theme_builds($site_build) {
  $builds = bootstrap_native_update_builds();
  if (empty($builds[$site_build]['theme_builds'])) {
    return;
  }
  foreach ($builds[$site_build]['theme_builds'] as $name => $build) {
    if ($build !== '' && $build !== $site_build) {
      bootstrap_native_update_check_build($build, bootstrap_native_update_build_for_theme($name), bootstrap_native_update_check_themes_for($name));
    }
  }
}

/**
 * The results of a site-wide build per theme.
 *
 * @return array
 *   Rows: themes (names), mode ('site', 'own', 'fixed'), build, check (or
 *   NULL). The site-wide one first.
 */
function bootstrap_native_update_results_per_theme($site_build) {
  $builds = bootstrap_native_update_builds();
  $list = list_themes();
  $rows = array();
  $site_names = array();
  foreach (bootstrap_native_update_site_themes() as $name) {
    $site_names[] = $list[$name]->info['name'];
  }
  $rows['site'] = array(
    'themes' => $site_names,
    'mode' => 'site',
    'build' => $site_build,
    'check' => isset($builds[$site_build]['check']) ? $builds[$site_build]['check'] : NULL,
  );
  if (!empty($builds[$site_build]['theme_builds'])) {
    foreach ($builds[$site_build]['theme_builds'] as $name => $build) {
      if (!isset($list[$name])) {
        continue;
      }
      $check = $build === $site_build ? $rows['site']['check'] : (isset($builds[$build]['check']) ? $builds[$build]['check'] : NULL);
      $rows[$name] = array(
        'themes' => array($list[$name]->info['name']),
        'mode' => bootstrap_native_update_parts_mode($name),
        'build' => $build,
        'check' => $check,
      );
    }
  }
  return $rows;
}

/**
 * The worst verdict of a set of per-theme results.
 */
function bootstrap_native_update_worst_verdict(array $rows) {
  $order = array('passed' => 0, 'check' => 1, 'breaks' => 2, 'failed' => 3);
  $worst = 'passed';
  foreach ($rows as $row) {
    $verdict = $row['check'] ? $row['check']['verdict'] : 'failed';
    if ($order[$verdict] > $order[$worst]) {
      $worst = $verdict;
    }
  }
  return $worst;
}

/**
 * Switches one theme to a build of the active release (its own parts,
 * once checked).
 */
function bootstrap_native_update_apply_theme_build($name, $build) {
  $map = bootstrap_native_update_theme_builds();
  $old = isset($map[$name]) ? $map[$name] : NULL;
  $map[$name] = $build;
  variable_set('bootstrap_native_update_theme_builds', $map);
  if ($build !== '') {
    bootstrap_native_update_save_build_record($build, array('inactive_since' => NULL));
  }
  bootstrap_native_update_mark_inactive(array($old));
  bootstrap_native_update_flush_assets();
}

/**
 * A theme goes back to the site-wide choice.
 */
function bootstrap_native_update_drop_theme_build($name) {
  $map = bootstrap_native_update_theme_builds();
  if (!array_key_exists($name, $map)) {
    return;
  }
  $old = $map[$name];
  unset($map[$name]);
  variable_set('bootstrap_native_update_theme_builds', $map);
  bootstrap_native_update_mark_inactive(array($old));
  bootstrap_native_update_flush_assets();
}

/**
 * Starts the 30 days of builds nothing uses any more.
 */
function bootstrap_native_update_mark_inactive(array $candidates) {
  $in_use = bootstrap_native_update_builds_in_use();
  $builds = bootstrap_native_update_builds();
  foreach ($candidates as $build) {
    if (is_string($build) && $build !== '' && !isset($in_use[$build]) && isset($builds[$build]) && empty($builds[$build]['inactive_since'])) {
      bootstrap_native_update_save_build_record($build, array('inactive_since' => REQUEST_TIME));
    }
  }
}

/**
 * Builds pages use now (the site-wide one and the themes').
 *
 * @return array
 *   Build names as keys ('' left out).
 */
function bootstrap_native_update_builds_in_use() {
  $in_use = array_flip(array_filter(bootstrap_native_update_theme_builds(), 'strlen'));
  $active = variable_get('bootstrap_native_update_active', '');
  if ($active !== '') {
    $in_use[$active] = TRUE;
  }
  return $in_use;
}

/**
 * Brings the builds of themes with own or fixed parts in line (cache clears,
 * themes enabled or disabled).
 *
 * Fixed parts are the theme's own: built and used without a check. A
 * theme's own choice is only used once applied; after that it is rebuilt
 * when the code or its needs change, with the parts it was applied with.
 */
function bootstrap_native_update_rebuild_theme_builds() {
  $version = bootstrap_native_update_active_version();
  $map = bootstrap_native_update_theme_builds();
  $separate = bootstrap_native_update_separate_themes();
  $builds = bootstrap_native_update_builds();
  $hash = bootstrap_native_update_theme_hash();
  $changed = FALSE;
  foreach ($map as $name => $build) {
    if (!isset($separate[$name])) {
      unset($map[$name]);
      $changed = TRUE;
    }
  }
  foreach ($separate as $name => $mode) {
    $current = isset($map[$name]) ? $map[$name] : NULL;
    if ($mode == 'own' && $current === NULL) {
      continue;
    }
    $selection = $mode == 'own' && $current !== '' && isset($builds[$current]['input']) ? $builds[$current]['input'] : FALSE;
    $plan = bootstrap_native_update_plan_build($version, $selection, $name);
    $stale = $current !== NULL && $current !== '' && (empty($builds[$current]['meta']) || $builds[$current]['meta']['theme_hash'] !== $hash);
    if ($plan['build'] === $current && !$stale) {
      continue;
    }
    if ($plan['build'] !== '' && (empty($builds[$plan['build']]['meta']) || $builds[$plan['build']]['meta']['theme_hash'] !== $hash)) {
      $made = bootstrap_native_update_make_build($version, $selection, $name);
      if ($made['error']) {
        watchdog('bootstrap_native', 'The Bootstrap build for @theme could not be made, so it uses the site-wide build: @error', array('@theme' => $name, '@error' => $made['error']), WATCHDOG_WARNING);
        if ($current !== NULL) {
          unset($map[$name]);
          $changed = TRUE;
        }
        continue;
      }
    }
    $map[$name] = $plan['build'];
    if ($plan['build'] !== '') {
      bootstrap_native_update_save_build_record($plan['build'], array('inactive_since' => NULL));
    }
    $changed = TRUE;
  }
  if ($changed) {
    $old = bootstrap_native_update_theme_builds();
    variable_set('bootstrap_native_update_theme_builds', $map);
    bootstrap_native_update_mark_inactive(array_values($old));
    bootstrap_native_update_flush_assets();
  }
}
