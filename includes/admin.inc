<?php

/**
 * @file
 * The Bootstrap updates page (Appearance > Bootstrap updates).
 */

module_load_include('inc', 'bootstrap_native_update', 'includes/releases');

/**
 * Page callback: the overview.
 */
function bootstrap_native_update_overview_page() {
  $build = array('#attached' => array('css' => array(drupal_get_path('module', 'bootstrap_native_update') . '/css/bootstrap_native_update.css')));
  $active = bootstrap_native_update_active();
  $versions = bootstrap_native_update_versions();
  $sass = bootstrap_native_update_sass();
  $available = bootstrap_native_update_available();

  if ($fallback = variable_get('bootstrap_native_update_fallback')) {
    $build['fallback'] = bootstrap_native_update_notice('error', t('Bootstrap Native was updated and Bootstrap @version could not be rebuilt for it, so the site went back to the release the theme ships with (@shipped). Check @version again to see why.', array(
      '@version' => $fallback['version'],
      '@shipped' => bootstrap_native_update_shipped_version(),
    )), $fallback['error']);
  }
  if ($available) {
    $build['notice'] = bootstrap_native_update_notice('warning', t('A new Bootstrap release is available: <strong>@version</strong>@pre. Check it before applying; the site keeps its current version until you do.', array(
      '@version' => $available['version'],
      '@pre' => $available['prerelease'] ? ' ' . t('(pre-release)') : '',
    )));
  }

  // Installed.
  $record = $active !== '' ? $versions[$active] : NULL;
  $build['installed'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bnu-section')),
    'title' => array('#markup' => '<h2>' . t('Installed') . '</h2>'),
    'facts' => array(
      '#type' => 'container',
      '#attributes' => array('class' => array('bnu-card', 'bnu-facts')),
      'active' => bootstrap_native_update_fact(t('Active version'), '<strong class="bnu-big">' . check_plain(bootstrap_native_update_active_version()) . '</strong>', $record ? t('Downloaded @date', array('@date' => format_date($record['downloaded'], 'short'))) : t('Shipped with Bootstrap Native @version', array('@version' => bootstrap_native_update_module_version()))),
      'sass' => bootstrap_native_update_fact(t('Sass compiler'),
        $sass ? '<span class="bnu-ok">' . t('Found: Dart Sass @version', array('@version' => $sass['version'])) . '</span>' : '<span class="bnu-bad">' . t('Not found') . '</span>',
        $sass ? check_plain(bootstrap_native_update_relative_path(dirname($sass['path']))) : t('Needed to check and apply updates: run <code>drush bootstrap-native-sass --install</code>, or unpack Dart Sass to <code>sites/all/libraries/dart-sass</code>.')),
      'checked' => bootstrap_native_update_fact(t('Last checked for releases'),
        ($checked = variable_get('bootstrap_native_update_checked', 0)) ? check_plain(format_date($checked, 'short')) . (variable_get('bootstrap_native_update_daily', 0) ? ' ' . t('(cron)') : '') : t('Never'),
        l(t('Check now'), bootstrap_native_update_action_path('refresh', 'all'), array('query' => bootstrap_native_update_action_query('refresh', 'all')))),
    ),
  );

  // Available.
  $releases = bootstrap_native_update_releases();
  $build['available'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bnu-section')),
    'title' => array('#markup' => '<h2>' . t('Available') . '</h2>'),
  );
  if (!$releases) {
    $build['available']['none'] = array('#markup' => '<p>' . ($checked ? t('No Bootstrap 6 releases found.') : t('Not checked yet: use <em>Check now</em> above.')) . '</p>');
  }
  else {
    $build['available']['newest'] = drupal_get_form('bootstrap_native_update_check_form', reset($releases), !$available, (bool) $sass);
    $older = array_slice($releases, 1, NULL, TRUE);
    if ($older) {
      $build['available']['older'] = drupal_get_form('bootstrap_native_update_older_form', $older, (bool) $sass);
    }
    $build['available']['help'] = array('#markup' => '<p class="bnu-muted">' . t("Checking downloads the release, verifies it against npm's checksum, builds a test stylesheet with the theme and looks for anything that would break. Takes about a minute. Visitors see no change.") . '</p>');
  }

  // Installed versions.
  $rows = array();
  $previous = variable_get('bootstrap_native_update_previous', '');
  foreach ($versions as $version => $record) {
    $rows[] = bootstrap_native_update_version_row($version, $record, $active, $previous);
  }
  $rows[] = bootstrap_native_update_version_row('', NULL, $active, $previous);
  $build['versions'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bnu-section')),
    'title' => array('#markup' => '<h2 id="bnu-versions">' . t('Installed versions') . '</h2>'),
    'table' => array(
      '#theme' => 'table',
      '#prefix' => '<div class="bnu-table">',
      '#suffix' => '</div>',
      '#header' => array(t('Version'), t('Where it came from'), t('Status'), t('Actions')),
      '#rows' => $rows,
    ),
    'help' => array('#markup' => '<p class="bnu-muted">' . t('Rolling back switches the site to the previous cached version; nothing is downloaded or rebuilt. Inactive versions are removed automatically after 30 days. To re-install an older version, use <a href="#bnu-older">Older versions</a>.') . '</p>'),
  );

  $build['settings'] = drupal_get_form('bootstrap_native_update_settings_form');
  return $build;
}

/**
 * The module's version (from its .info file).
 */
function bootstrap_native_update_module_version() {
  $info = system_get_info('module', 'bootstrap_native_update');
  return isset($info['version']) ? $info['version'] : '';
}

/**
 * A path from the Drupal root, if it is inside it.
 */
function bootstrap_native_update_relative_path($path) {
  return strpos($path, DRUPAL_ROOT . '/') === 0 ? substr($path, strlen(DRUPAL_ROOT) + 1) : $path;
}

/**
 * A message box.
 */
function bootstrap_native_update_notice($type, $html, $detail = '') {
  return array(
    '#markup' => '<div class="messages ' . $type . '"' . ($type == 'error' ? ' role="alert"' : ' role="status"') . '>' . $html . ($detail !== '' ? '<pre class="bnu-pre">' . check_plain($detail) . '</pre>' : '') . '</div>',
  );
}

/**
 * One fact in the Installed card (label, value, line under it).
 */
function bootstrap_native_update_fact($label, $value, $under) {
  return array('#markup' => '<div class="bnu-fact"><span class="bnu-label">' . $label . '</span><span>' . $value . '</span><span class="bnu-small">' . $under . '</span></div>');
}

/**
 * A badge (pre-release, and the check results).
 */
function bootstrap_native_update_badge($status) {
  $labels = array(
    'prerelease' => t('Pre-release'),
    'breaks' => t('Breaks'),
    'check' => t('Check'),
    'info' => t('Info'),
    'failed' => t("Won't build"),
  );
  return '<span class="bnu-badge bnu-badge-' . $status . '">' . $labels[$status] . '</span>';
}

/**
 * A row of the Installed versions table ('' = the shipped release).
 */
function bootstrap_native_update_version_row($version, $record, $active, $previous) {
  $releases = variable_get('bootstrap_native_update_releases', array());
  $name = $version === '' ? bootstrap_native_update_shipped_version() : $version;
  $label = '<strong>' . check_plain($name) . '</strong>';
  if (preg_match('/-/', $name)) {
    $label .= ' ' . bootstrap_native_update_badge('prerelease');
  }
  if ($version === '') {
    $from = t('Shipped with the theme (always kept as the fallback)');
  }
  else {
    $account = $record['uid'] ? user_load($record['uid']) : NULL;
    $from = $account
      ? t('Downloaded @date by !user', array('@date' => format_date($record['downloaded'], 'short'), '!user' => theme('username', array('account' => $account))))
      : t('Downloaded @date with drush', array('@date' => format_date($record['downloaded'], 'short')));
    $from .= '<div class="bnu-small">' . bootstrap_native_update_verdict_label($record) . '</div>';
  }
  $actions = array();
  if ($version === $active) {
    $status = '<span class="bnu-ok">' . t('Active') . '</span>';
  }
  else {
    $status = $version === $previous ? t('Previous') : t('Inactive');
    if ($version !== '' && !empty($record['inactive_since'])) {
      $status .= '<div class="bnu-small">' . t('Removed @date', array('@date' => format_date($record['inactive_since'] + BOOTSTRAP_NATIVE_UPDATE_KEEP, 'custom', 'Y-m-d'))) . '</div>';
    }
    if ($version === '' || (!empty($record['build']) && !empty($record['check']) && $record['check']['verdict'] != 'failed' && !empty($record['applied']))) {
      $actions[] = l(t('Roll back to this version'), bootstrap_native_update_action_path('rollback', $version === '' ? 'shipped' : $version), array(
        'query' => bootstrap_native_update_action_query('rollback', $version === '' ? 'shipped' : $version),
        'attributes' => array('class' => array('button')),
      ));
    }
  }
  if ($version !== '') {
    if (!empty($record['check'])) {
      $actions[] = l(t('Results'), 'admin/appearance/bootstrap-updates/result/' . $version);
    }
    if ($version !== $active) {
      $actions[] = l(t('Discard'), bootstrap_native_update_action_path('discard', $version), array('query' => bootstrap_native_update_action_query('discard', $version)));
    }
  }
  return array($label, $from, $status, $actions ? '<span class="bnu-actions">' . implode(' ', $actions) . '</span>' : '<span class="bnu-muted">—</span>');
}

/**
 * A short line for a release's check result.
 */
function bootstrap_native_update_verdict_label($record) {
  if (empty($record['check'])) {
    return !empty($record['error']) ? t('Check stopped: @error', array('@error' => $record['error'])) : t('Not checked');
  }
  $labels = array(
    'passed' => t('Check: passed'),
    'check' => t('Check: passed, with things to look at'),
    'breaks' => t('Check: would break parts of the theme'),
    'failed' => t("Check: doesn't build with the theme"),
  );
  return $labels[$record['check']['verdict']];
}

/**
 * Form: the newest release, with "Check this update".
 */
function bootstrap_native_update_check_form($form, &$form_state, $release, $current, $has_sass) {
  $form['#attributes']['class'][] = 'bnu-card';
  $form['#attributes']['class'][] = 'bnu-release';
  $form['release'] = array(
    '#markup' => '<div class="bnu-release-text"><div><strong class="bnu-big">' . check_plain($release['version']) . '</strong> ' . ($release['prerelease'] ? bootstrap_native_update_badge('prerelease') : '') . '</div>'
    . '<span class="bnu-muted">' . ($release['date'] ? t('Released @date', array('@date' => format_date($release['date'], 'custom', 'Y-m-d'))) . ' · ' : '')
    . l(t('Release notes'), 'https://github.com/twbs/bootstrap/releases/tag/v' . $release['version']) . '</span></div>',
  );
  $form['version'] = array('#type' => 'value', '#value' => $release['version']);
  if ($current) {
    $form['current'] = array('#markup' => '<span class="bnu-muted">' . t('This site runs the newest release.') . '</span>');
    return $form;
  }
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Check this update'),
    '#disabled' => !$has_sass,
    '#attributes' => array('class' => array('button--primary')),
  );
  $form['#submit'][] = 'bootstrap_native_update_start_check';
  return $form;
}

/**
 * Form: older releases, with "Check this version".
 */
function bootstrap_native_update_older_form($form, &$form_state, $releases, $has_sass) {
  $active = bootstrap_native_update_active_version();
  $options = array();
  $disabled = array();
  foreach ($releases as $version => $release) {
    $options[$version] = $version . ($release['date'] ? ' · ' . t('released @date', array('@date' => format_date($release['date'], 'custom', 'Y-m-d'))) : '');
    if ($version === $active) {
      $options[$version] = $version . ' · ' . t('active');
    }
  }
  $form['#attributes']['class'][] = 'bnu-older';
  $form['#attributes']['id'] = 'bnu-older';
  $form['version'] = array(
    '#type' => 'select',
    '#title' => t('Older versions'),
    '#options' => $options,
    '#empty_option' => t('- Choose a version -'),
    '#required' => TRUE,
    '#description' => t('Earlier Bootstrap 6 releases, newest first. Use it to go back to a version that was removed.'),
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Check this version'), '#disabled' => !$has_sass);
  $form['#validate'][] = 'bootstrap_native_update_older_form_validate';
  $form['#submit'][] = 'bootstrap_native_update_start_check';
  return $form;
}

/**
 * Form validation: not the active release.
 */
function bootstrap_native_update_older_form_validate($form, &$form_state) {
  if ($form_state['values']['version'] === bootstrap_native_update_active_version()) {
    form_set_error('version', t('The site already runs @version.', array('@version' => $form_state['values']['version'])));
  }
}

/**
 * Form submit: starts a check (a batch, with its progress page).
 */
function bootstrap_native_update_start_check($form, &$form_state) {
  $version = $form_state['values']['version'];
  $releases = variable_get('bootstrap_native_update_releases', array());
  if (!isset($releases[$version]) || !bootstrap_native_update_sass()) {
    return;
  }
  $operations = array();
  foreach (array_keys(bootstrap_native_update_check_steps()) as $step) {
    $operations[] = array('bootstrap_native_update_batch_step', array($step, $version));
  }
  batch_set(array(
    'title' => t('Checking Bootstrap @version', array('@version' => $version)),
    'operations' => $operations,
    'init_message' => bootstrap_native_update_steps_html($version, 'download'),
    'progress_message' => '',
    'error_message' => t('The check stopped with an error.'),
    'finished' => 'bootstrap_native_update_batch_finished',
    'file' => drupal_get_path('module', 'bootstrap_native_update') . '/includes/admin.inc',
    'css' => array(drupal_get_path('module', 'bootstrap_native_update') . '/css/bootstrap_native_update.css'),
  ));
  $form_state['redirect'] = 'admin/appearance/bootstrap-updates/result/' . $version;
}

/**
 * The list of steps for the progress page (done, current, waiting).
 */
function bootstrap_native_update_steps_html($version, $current) {
  $items = '';
  $state = 'done';
  foreach (bootstrap_native_update_check_steps() as $step => $label) {
    if ($step == $current) {
      $state = 'current';
    }
    $items .= '<li class="bnu-step bnu-step-' . $state . '">' . $label . '</li>';
    if ($state == 'current') {
      $state = 'waiting';
    }
  }
  $cancel = l(t('Cancel'), bootstrap_native_update_action_path('cancel', $version), array('query' => bootstrap_native_update_action_query('cancel', $version)));
  return '<ol class="bnu-steps">' . $items . '</ol><p class="bnu-small">' . t('Visitors see no change while this runs.') . ' ' . $cancel . '</p>';
}

/**
 * Batch operation: one step of a check.
 */
function bootstrap_native_update_batch_step($step, $version, &$context) {
  if (!empty($context['results']['error'])) {
    return;
  }
  $error = bootstrap_native_update_check_step($step, $version);
  $context['results']['version'] = $version;
  if ($error) {
    $context['results']['error'] = $error;
    if (bootstrap_native_update_valid_version($version) && array_key_exists($version, bootstrap_native_update_versions())) {
      bootstrap_native_update_save_version($version, array('error' => $error));
    }
  }
  $steps = array_keys(bootstrap_native_update_check_steps());
  $next = array_search($step, $steps) + 1;
  $context['message'] = bootstrap_native_update_steps_html($version, isset($steps[$next]) ? $steps[$next] : '');
}

/**
 * Batch finished: errors go to the result page as a message.
 */
function bootstrap_native_update_batch_finished($success, $results, $operations) {
  if (!$success) {
    drupal_set_message(t('The check stopped with an error.'), 'error');
  }
  elseif (!empty($results['error'])) {
    drupal_set_message($results['error'], 'error');
  }
}

/**
 * Title callback: the result page.
 */
function bootstrap_native_update_result_title($version) {
  return t('Bootstrap @version: check results', array('@version' => $version));
}

/**
 * Page callback: the result of a check.
 */
function bootstrap_native_update_result_page($version) {
  $versions = bootstrap_native_update_versions();
  if (!bootstrap_native_update_valid_version($version) || empty($versions[$version]['check'])) {
    if (!bootstrap_native_update_valid_version($version)) {
      return MENU_NOT_FOUND;
    }
    return array(
      'none' => array('#markup' => '<p>' . t('No check result for @version.', array('@version' => $version)) . '</p>'),
      'back' => array('#markup' => '<p>' . l(t('Back to Bootstrap updates'), 'admin/appearance/bootstrap-updates') . '</p>'),
    );
  }
  drupal_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Administration'), 'admin'),
    l(t('Appearance'), 'admin/appearance'),
    l(t('Bootstrap updates'), 'admin/appearance/bootstrap-updates'),
  ));
  $record = $versions[$version];
  $result = $record['check'];
  $build = array('#attached' => array('css' => array(drupal_get_path('module', 'bootstrap_native_update') . '/css/bootstrap_native_update.css')));
  $active = $version === bootstrap_native_update_active();

  switch ($result['verdict']) {
    case 'passed':
      $build['notice'] = bootstrap_native_update_notice('status', t('<strong>Passed every check.</strong> Nothing the theme relies on changed. Preview it on the site, then apply.'));
      break;

    case 'check':
      $build['notice'] = bootstrap_native_update_notice('warning', t('<strong>Passed, with things to look at.</strong> Nothing in Bootstrap Native itself breaks; the rows marked Check are worth a look in the preview.'));
      break;

    case 'breaks':
      $build['notice'] = bootstrap_native_update_notice('error', t('<strong>Updating will break parts of the theme.</strong> Bootstrap Native needs an update before this release works fully. The problems below may not matter for this site: preview to judge, or apply anyway.'));
      break;

    case 'failed':
      $build['notice'] = bootstrap_native_update_notice('error', t("<strong>This release doesn't build with Bootstrap Native.</strong> Something the theme relies on is gone, so there is nothing to preview or apply. Bootstrap Native needs an update for it."));
      break;
  }
  if ($result['against'] !== bootstrap_native_update_active_version()) {
    $build['stale'] = bootstrap_native_update_notice('warning', t('These results compare with @old; the site now runs @current. Check again for an up-to-date comparison.', array('@old' => $result['against'], '@current' => bootstrap_native_update_active_version())));
  }

  if (in_array($result['verdict'], array('passed', 'check'))) {
    $rows = array();
    foreach ($result['rows'] as $row) {
      $text = bootstrap_native_update_row_text($row);
      $rows[] = array(
        array('data' => bootstrap_native_update_result_icon($row['status']), 'class' => array('bnu-icon-cell')),
        '<strong>' . $text['check'] . '</strong>' . ($row['status'] == 'check' ? ' ' . bootstrap_native_update_badge('check') : ''),
        $text['details'] . ($text['where'] ? '<div class="bnu-small">' . $text['where'] . '</div>' : ''),
      );
    }
    $build['table'] = array(
      '#theme' => 'table',
      '#prefix' => '<div class="bnu-table">',
      '#suffix' => '</div>',
      '#header' => array(array('data' => '<span class="element-invisible">' . t('Result') . '</span>'), t('Check'), t('Details')),
      '#rows' => $rows,
    );
    $build['help'] = array('#markup' => '<p class="bnu-muted">' . t("These checks catch structural breaks. They can't see whether something looks different; the preview shows you.") . '</p>');
  }
  else {
    $rows = array();
    // Most serious first.
    $order = array('failed' => 0, 'breaks' => 1, 'check' => 2, 'info' => 3);
    $problems = array_filter($result['rows'], function ($row) use ($order) {
      return isset($order[$row['status']]);
    });
    usort($problems, function ($a, $b) use ($order) {
      return $order[$a['status']] - $order[$b['status']];
    });
    foreach ($problems as $row) {
      $text = bootstrap_native_update_row_text($row);
      $rows[] = array(
        bootstrap_native_update_badge($row['status']),
        $text['changed'],
        $text['affects'] . ($text['where'] ? '<div class="bnu-small">' . $text['where'] . '</div>' : ''),
      );
    }
    $build['heading'] = array('#markup' => '<h2>' . t('What breaks') . '</h2>');
    $build['table'] = array(
      '#theme' => 'table',
      '#prefix' => '<div class="bnu-table">',
      '#suffix' => '</div>',
      '#header' => array(t('Severity'), t('What changed in Bootstrap'), t('What it affects on the site')),
      '#rows' => $rows,
    );
    if ($result['verdict'] == 'breaks') {
      $build['help'] = array('#markup' => '<p class="bnu-muted">' . t('Rows marked as !badge come from the theme itself: the fix is a Bootstrap Native update. Until then you can keep the current version, or apply this one and live with the list.', array('!badge' => bootstrap_native_update_badge('breaks'))) . '</p>');
    }
  }

  if ($active) {
    $build['active'] = array('#markup' => '<p><strong>' . t('This release is active.') . '</strong> ' . l(t('Back to Bootstrap updates'), 'admin/appearance/bootstrap-updates') . '</p>');
  }
  elseif ($result['verdict'] != 'failed') {
    $build['apply'] = drupal_get_form('bootstrap_native_update_apply_form', $version, $result['verdict']);
  }
  else {
    $build['discard'] = array('#markup' => '<p>' . l(t('Discard'), bootstrap_native_update_action_path('discard', $version), array('query' => bootstrap_native_update_action_query('discard', $version))) . ' · ' . l(t('Back to Bootstrap updates'), 'admin/appearance/bootstrap-updates') . '</p>');
  }
  return $build;
}

/**
 * The icon in the passed table.
 */
function bootstrap_native_update_result_icon($status) {
  if ($status == 'info') {
    return '<svg role="img" aria-label="' . t('For information') . '" class="bnu-icon bnu-icon-info" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 8h.01"/></svg>';
  }
  if ($status == 'check') {
    return '<svg role="img" aria-label="' . t('Look at this') . '" class="bnu-icon bnu-icon-check" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 9v4M12 17h.01"/><path d="M10.3 3.9 1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z"/></svg>';
  }
  return '<svg role="img" aria-label="' . t('Passed') . '" class="bnu-icon bnu-icon-passed" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M20 6 9 17l-5-5"/></svg>';
}

/**
 * Words for a check row.
 *
 * @return array
 *   Keys: check (name), details (passed table), changed, affects, where
 *   (problem table). HTML; every value from outside is escaped.
 */
function bootstrap_native_update_row_text(array $row) {
  $v = $row['values'];
  $text = array('check' => '', 'details' => '', 'changed' => '', 'affects' => '', 'where' => '');
  switch ($row['code']) {
    case 'build_ok':
      $text['check'] = t('Builds with the theme');
      $text['details'] = t("Every Bootstrap style the theme applies to Drupal's forms, messages and menus still exists");
      break;

    case 'build_failed':
      $text['check'] = t('Builds with the theme');
      $text['changed'] = t("Something the theme uses from Bootstrap is gone (a file, a mixin or a class it extends), so the stylesheet can't be built");
      $text['affects'] = t('Nothing on the site: there is nothing to apply.') . '<pre class="bnu-pre">' . check_plain($v['error']) . '</pre>';
      $text['where'] = t('Theme: scss/');
      break;

    case 'menu_ok':
      $text['check'] = t('Menu rename');
      $text['details'] = t("Bootstrap's dropdown renamed so Drupal menus stay visible: @css places in the styles, @js in the scripts", array('@css' => $v['css'], '@js' => $v['js']));
      break;

    case 'menu_rename':
      $text['check'] = t('Menu rename');
      $text['changed'] = t("Bootstrap's dropdown (<code>.menu</code>) changed: @css places renamed in the styles and @js in the scripts (now @old_css and @old_js)", array('@css' => $v['css'], '@js' => $v['js'], '@old_css' => $v['old_css'], '@old_js' => $v['old_js']));
      $text['affects'] = t("Drupal's menus may be hidden, or dropdowns stop opening");
      $text['where'] = t('Theme: includes/build.inc');
      break;

    case 'colors_ok':
      $text['check'] = t('Color settings');
      $text['details'] = t('All variables the color settings set are still used');
      break;

    case 'color_property':
      $text['check'] = t('Color settings');
      $text['changed'] = t('Color variable <code>@property</code> no longer used', array('@property' => $v['property']));
      $text['affects'] = t("The theme's color settings no longer reach what used it; it falls back to Bootstrap's own color");
      $text['where'] = t('Theme: includes/color.inc');
      break;

    case 'color_property_fewer':
      $text['check'] = t('Color settings');
      $text['changed'] = t('Color variable <code>@property</code> read in @new places (now @old)', array('@property' => $v['property'], '@new' => $v['new'], '@old' => $v['old']));
      $text['affects'] = t("Some parts stop following the theme's color settings and show Bootstrap's own color; look at links, buttons and borders in the preview");
      $text['where'] = t('Theme: includes/color.inc');
      break;

    case 'classes_ok':
      $text['check'] = t('Classes in templates');
      $text['details'] = count($v['themes']) > 1 ? t('None removed that the theme or its sub-themes use') : t('None removed that the theme uses');
      break;

    case 'class_removed':
      $text['check'] = t('Classes in templates');
      $text['changed'] = t('Class <code>@class</code> removed', array('@class' => $v['class']));
      if ($v['theme'] == 'bootstrap_native') {
        $text['affects'] = t('Markup the theme writes with it loses its Bootstrap style');
        $text['where'] = t('Theme: @file', array('@file' => $v['file']));
      }
      else {
        $text['affects'] = t('Used in a sub-theme template; look at a page that uses it in the preview');
        $text['where'] = t('Sub-theme: @file', array('@file' => $v['file']));
      }
      break;

    case 'undefined_properties':
      $text['check'] = t('Variables');
      $text['changed'] = t('Variables used but defined nowhere: @list', array('@list' => implode(', ', $v['properties'])));
      $text['affects'] = t("Whatever uses them falls back to the browser's default");
      break;

    case 'size':
      $text['check'] = t('Stylesheet size');
      $text['changed'] = t('Stylesheet size');
      $text['details'] = $text['affects'] = t('@new KB (now @old KB)', array('@new' => round($v['new'] / 1024), '@old' => round($v['old'] / 1024)));
      break;
  }
  return $text;
}

/**
 * Form: Apply, Preview, Discard (results page).
 */
function bootstrap_native_update_apply_form($form, &$form_state, $version, $verdict) {
  $form['version'] = array('#type' => 'value', '#value' => $version);
  $form['verdict'] = array('#type' => 'value', '#value' => $verdict);
  if ($verdict == 'breaks') {
    $form['#attributes']['class'][] = 'bnu-confirm';
    $form['understood'] = array(
      '#type' => 'checkbox',
      '#title' => t('I understand that the parts listed above will break until Bootstrap Native is updated.') . ' <span class="bnu-muted">' . t('You can roll back to @version at any time.', array('@version' => bootstrap_native_update_active_version())) . '</span>',
    );
  }
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['apply'] = array(
    '#type' => 'submit',
    '#value' => $verdict == 'breaks' ? t('Apply anyway') : t('Apply update'),
    '#attributes' => array('class' => array($verdict == 'breaks' ? 'bnu-danger' : 'button--primary')),
    '#submit' => array('bootstrap_native_update_apply_form_apply'),
  );
  if ($verdict == 'breaks') {
    $form['actions']['apply']['#states'] = array('disabled' => array(':input[name="understood"]' => array('checked' => FALSE)));
  }
  $form['actions']['preview'] = array(
    '#type' => 'submit',
    '#value' => t('Preview on the site'),
    '#submit' => array('bootstrap_native_update_apply_form_preview'),
    '#limit_validation_errors' => array(),
  );
  $form['actions']['discard'] = array(
    '#markup' => l(t('Discard'), bootstrap_native_update_action_path('discard', $version), array('query' => bootstrap_native_update_action_query('discard', $version), 'attributes' => array('class' => array('bnu-link')))),
  );
  $form['actions']['help'] = array('#markup' => '<div class="bnu-small bnu-full">' . t('Preview: only you see the new version while browsing the site. Discard removes the download.') . '</div>');
  return $form;
}

/**
 * Form submit: Apply (or Apply anyway, once confirmed).
 */
function bootstrap_native_update_apply_form_apply($form, &$form_state) {
  $version = $form_state['values']['version'];
  $versions = bootstrap_native_update_versions();
  if (empty($versions[$version]['build']) || empty($versions[$version]['check']) || $versions[$version]['check']['verdict'] == 'failed') {
    drupal_set_message(t('@version has no build to apply. Check it again.', array('@version' => $version)), 'error');
    return;
  }
  if ($versions[$version]['check']['verdict'] == 'breaks' && empty($form_state['values']['understood'])) {
    drupal_set_message(t('Tick "I understand" to apply a release that breaks parts of the theme.'), 'error');
    $form_state['redirect'] = 'admin/appearance/bootstrap-updates/result/' . $version;
    return;
  }
  bootstrap_native_update_apply($version);
  $form_state['redirect'] = 'admin/appearance/bootstrap-updates';
}

/**
 * Switches the site to a checked release and says so.
 */
function bootstrap_native_update_apply($version) {
  bootstrap_native_update_save_version($version, array('applied' => REQUEST_TIME));
  bootstrap_native_update_activate($version);
  unset($_SESSION['bootstrap_native_update_preview']);
  drupal_set_message(t('<strong>Bootstrap @version is now active.</strong> The previous version stays installed: roll back below if anything looks wrong. Open pages show the old version until they are reloaded.', array('@version' => $version)));
}

/**
 * Form submit: Preview.
 */
function bootstrap_native_update_apply_form_preview($form, &$form_state) {
  // From the form's arguments: #limit_validation_errors leaves no values.
  $_SESSION['bootstrap_native_update_preview'] = $form_state['build_info']['args'][0];
  $form_state['redirect'] = '<front>';
}

/**
 * The preview bar at the top of every page while previewing.
 */
function bootstrap_native_update_preview_bar($version) {
  return array(
    '#attached' => array('css' => array(drupal_get_path('module', 'bootstrap_native_update') . '/css/bootstrap_native_update.css')),
    '#prefix' => '<div role="region" aria-label="' . t('Bootstrap preview') . '" class="bnu-preview">',
    '#suffix' => '</div>',
    'text' => array('#markup' => '<div class="bnu-preview-text"><svg aria-hidden="true" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/></svg><span>' . t('<strong>Previewing Bootstrap @version.</strong> Only you see this; visitors still get @current.', array('@version' => $version, '@current' => bootstrap_native_update_active_version())) . '</span></div>'),
    'form' => drupal_get_form('bootstrap_native_update_preview_form', $version),
  );
}

/**
 * Form: the preview bar's buttons.
 */
function bootstrap_native_update_preview_form($form, &$form_state, $version) {
  $versions = bootstrap_native_update_versions();
  $form['#attributes']['class'][] = 'bnu-preview-actions';
  $form['version'] = array('#type' => 'value', '#value' => $version);
  $form['back'] = array('#markup' => l(t('Back to the results'), 'admin/appearance/bootstrap-updates/result/' . $version));
  // A release that breaks things is applied from its results page, where
  // "I understand" is.
  if (isset($versions[$version]['check']['verdict']) && $versions[$version]['check']['verdict'] != 'breaks') {
    $form['apply'] = array('#type' => 'submit', '#value' => t('Apply'), '#submit' => array('bootstrap_native_update_preview_form_apply'));
  }
  $form['stop'] = array('#type' => 'submit', '#value' => t('Stop preview'), '#submit' => array('bootstrap_native_update_preview_form_stop'));
  return $form;
}

/**
 * Form submit: Apply from the preview bar.
 */
function bootstrap_native_update_preview_form_apply($form, &$form_state) {
  $version = $form_state['values']['version'];
  $versions = bootstrap_native_update_versions();
  if (!empty($versions[$version]['build']) && isset($versions[$version]['check']['verdict']) && in_array($versions[$version]['check']['verdict'], array('passed', 'check'))) {
    bootstrap_native_update_apply($version);
  }
  $form_state['redirect'] = 'admin/appearance/bootstrap-updates';
}

/**
 * Form submit: Stop preview.
 */
function bootstrap_native_update_preview_form_stop($form, &$form_state) {
  unset($_SESSION['bootstrap_native_update_preview']);
  drupal_set_message(t('Preview stopped.'));
}

/**
 * Form: settings.
 */
function bootstrap_native_update_settings_form($form, &$form_state) {
  $form['#attributes']['class'][] = 'bnu-section';
  $form['title'] = array('#markup' => '<h2>' . t('Settings') . '</h2>');
  $form['bootstrap_native_update_prereleases'] = array(
    '#type' => 'checkbox',
    '#title' => t('Offer pre-releases (alpha, beta, release candidates)'),
    '#description' => t('On while Bootstrap 6 has no stable release. Turn off later to see stable releases only.'),
    '#default_value' => variable_get('bootstrap_native_update_prereleases', 1),
  );
  $form['bootstrap_native_update_daily'] = array(
    '#type' => 'checkbox',
    '#title' => t('Check for new releases daily (cron)'),
    '#description' => t('Only looks: nothing is downloaded or changed until someone presses a button here.'),
    '#default_value' => variable_get('bootstrap_native_update_daily', 0),
  );
  $form['bootstrap_native_update_sass_path'] = array(
    '#type' => 'textfield',
    '#title' => t('Sass compiler path'),
    '#description' => t('Only if Dart Sass is somewhere else than <code>sites/all/libraries/dart-sass</code> or the PATH: the full path to its <code>sass</code> program.'),
    '#default_value' => variable_get('bootstrap_native_update_sass_path', ''),
    '#size' => 60,
  );
  $form['#validate'][] = 'bootstrap_native_update_settings_form_validate';
  return system_settings_form($form);
}

/**
 * Form validation: the compiler path must run.
 */
function bootstrap_native_update_settings_form_validate($form, &$form_state) {
  $path = trim($form_state['values']['bootstrap_native_update_sass_path']);
  $form_state['values']['bootstrap_native_update_sass_path'] = $path;
  if ($path !== '') {
    module_load_include('inc', 'bootstrap_native_update', 'includes/build');
    if (!bootstrap_native_update_sass_version($path)) {
      form_set_error('bootstrap_native_update_sass_path', t("@path isn't a Dart Sass program that runs.", array('@path' => $path)));
    }
  }
}

/**
 * The path of a one-click action.
 */
function bootstrap_native_update_action_path($op, $version) {
  return 'admin/appearance/bootstrap-updates/action/' . $op . '/' . $version;
}

/**
 * The token for a one-click action.
 */
function bootstrap_native_update_action_query($op, $version) {
  return array('token' => drupal_get_token('bootstrap_native_update:' . $op . ':' . $version));
}

/**
 * Page callback: a one-click action (with its token).
 */
function bootstrap_native_update_action($op, $version) {
  if (!isset($_GET['token']) || !drupal_valid_token($_GET['token'], 'bootstrap_native_update:' . $op . ':' . $version)) {
    return MENU_ACCESS_DENIED;
  }
  $target = 'admin/appearance/bootstrap-updates';
  switch ($op) {
    case 'refresh':
      $error = bootstrap_native_update_refresh_releases();
      drupal_set_message($error ? $error : t('Release list updated.'), $error ? 'error' : 'status');
      break;

    case 'rollback':
      $versions = bootstrap_native_update_versions();
      if ($version === 'shipped') {
        bootstrap_native_update_activate('');
        drupal_set_message(bootstrap_native_update_rollback_message(bootstrap_native_update_shipped_version()));
      }
      elseif (bootstrap_native_update_valid_version($version) && !empty($versions[$version]['build'])) {
        bootstrap_native_update_activate($version);
        drupal_set_message(bootstrap_native_update_rollback_message($version));
      }
      break;

    case 'discard':
      if (bootstrap_native_update_valid_version($version) && $version !== bootstrap_native_update_active()) {
        bootstrap_native_update_discard($version);
        if (isset($_SESSION['bootstrap_native_update_preview']) && $_SESSION['bootstrap_native_update_preview'] === $version) {
          unset($_SESSION['bootstrap_native_update_preview']);
        }
        drupal_set_message(t('Bootstrap @version was removed.', array('@version' => $version)));
      }
      break;

    case 'cancel':
      // The batch page stops polling once the browser leaves it; what was
      // downloaded so far goes (never the active release).
      if (bootstrap_native_update_valid_version($version) && $version !== bootstrap_native_update_active()) {
        file_unmanaged_delete('temporary://bootstrap-native-' . $version . '.tgz');
        $versions = bootstrap_native_update_versions();
        if (isset($versions[$version]) && empty($versions[$version]['applied'])) {
          bootstrap_native_update_discard($version);
        }
        drupal_set_message(t('Check cancelled. Nothing was changed.'));
      }
      break;

    default:
      return MENU_NOT_FOUND;
  }
  drupal_goto($target);
}

/**
 * The message after a roll back.
 */
function bootstrap_native_update_rollback_message($version) {
  $previous = variable_get('bootstrap_native_update_previous', '');
  $previous = $previous === '' ? bootstrap_native_update_shipped_version() : $previous;
  return t('Rolled back to Bootstrap @version. @previous is still installed if you want to apply it again.', array('@version' => $version, '@previous' => $previous));
}
