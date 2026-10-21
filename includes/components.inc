<?php

/**
 * @file
 * Appearance > Bootstrap components: which of Bootstrap's parts the site's
 * build includes, sizes and docs links per part, and the built files as a
 * download.
 */

module_load_include('inc', 'bootstrap_native_update', 'includes/releases');
module_load_include('inc', 'bootstrap_native_update', 'includes/admin');
module_load_include('inc', 'bootstrap_native_update', 'includes/parts');

/**
 * Sizes and docs pages per part of a release (measured once per release).
 *
 * @return array|null
 *   Keys: sizes (bytes, keyed by part or utilities:<group>), docs (path
 *   under the docs, keyed the same), docs_base (URL), measured; NULL when
 *   not measured yet.
 */
function bootstrap_native_update_part_info($version) {
  $info = variable_get('bootstrap_native_update_part_info', array());
  return isset($info[$version]) ? $info[$version] : NULL;
}

/**
 * The docs for a release: https://getbootstrap.com/docs/<major.minor>/.
 */
function bootstrap_native_update_docs_base($version) {
  return 'https://getbootstrap.com/docs/' . implode('.', array_slice(explode('.', $version), 0, 2)) . '/';
}

/**
 * Form: the components page.
 */
function bootstrap_native_update_components_form($form, &$form_state) {
  $version = bootstrap_native_update_active_version();
  $sass = bootstrap_native_update_sass();
  $info = bootstrap_native_update_part_info($version);
  if (!$info && $sass && empty($form_state['input'])) {
    // First visit for this release: measure sizes and find docs pages.
    bootstrap_native_update_measure_batch($version);
    batch_process('admin/appearance/bootstrap-components');
  }
  $context = bootstrap_native_update_parts_context($version);
  $current = bootstrap_native_update_plan_build($version);
  $chosen = array_flip(array_merge($current['effective']['parts'], array_map(function ($g) {
    return 'utilities:' . $g;
  }, $current['effective']['utilities'])));
  $required = array_flip($context['required']);
  $used = bootstrap_native_update_parts_in_templates($version, $context);
  $sizes = $info ? $info['sizes'] : array();
  $docs = $info ? $info['docs'] : array();

  $form['#attached'] = bootstrap_native_update_admin_css();
  $form['#attached']['js'][] = drupal_get_path('module', 'bootstrap_native_update') . '/js/components.js';
  $form['#attributes']['class'][] = 'bnu-components';
  $form['intro'] = array('#markup' => '<p class="bnu-intro">' . t("What goes into this site's Bootstrap build. The list comes from the active release (@version), so a new release brings its own parts. Changes are built and checked like an update: preview first, apply, roll back.", array('@version' => $version)) . '</p>');
  if (!$sass) {
    $form['nosass'] = bootstrap_native_update_notice('warning', t('No Sass compiler found, so this page only shows the current choices. Run <code>drush bootstrap-native-sass --install</code> to change them.'));
  }

  // Styles: one row per part, in groups.
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
  $form['styles'] = array(
    '#theme' => 'bootstrap_native_update_components_table',
    '#tree' => TRUE,
    '#groups' => $groups,
  );
  $rows = array();
  foreach ($context['parts'] as $part => $group) {
    if ($part == 'utilities/api') {
      foreach ($context['utility_groups'] as $name => $keys) {
        $rows['utilities:' . $name] = 'utilities';
      }
      continue;
    }
    $rows[$part] = $group;
  }
  foreach ($rows as $id => $group) {
    $state = 'optional';
    $why = '';
    if (isset($context['exclude'][$id])) {
      $state = 'clash';
      $why = $id == 'forms/floating-labels' && module_exists('floating_labels') ? t('The Floating Labels module does this') : check_plain($context['exclude'][$id]);
    }
    elseif (isset($required[$id])) {
      $state = 'required';
      $why = $group == 'base' ? t("Bootstrap's variables and base styles") : t("The theme's Drupal rules use it");
    }
    elseif (isset($context['needed_by'][$id])) {
      $state = 'needed';
      $why = t('Listed in @themes', array('@themes' => implode(', ', $context['needed_by'][$id])));
    }
    elseif (isset($used[$id])) {
      $state = 'used';
      $why = t('@theme: @file', array('@theme' => $used[$id]['theme'], '@file' => $used[$id]['file']));
    }
    $form['styles'][$id] = array(
      '#type' => 'checkbox',
      '#title' => bootstrap_native_update_part_name($id),
      '#title_display' => 'invisible',
      '#default_value' => isset($chosen[$id]) || in_array($state, array('required', 'needed')),
      '#disabled' => !$sass || in_array($state, array('required', 'clash', 'needed')),
      '#attributes' => array('data-size' => isset($sizes[$id]) ? $sizes[$id] : 0),
      '#bnu_group' => $group,
      '#bnu_state' => $state,
      '#bnu_why' => $why,
      '#bnu_size' => isset($sizes[$id]) ? $sizes[$id] : NULL,
      '#bnu_docs' => !empty($docs[$id]) ? $info['docs_base'] . $docs[$id] . '/' : '',
    );
  }

  // Scripts: for information (one bundle).
  $form['scripts'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bnu-section')),
    'title' => array('#markup' => '<h2>' . t('Scripts (JavaScript plugins)') . '</h2>'),
    'text' => array('#markup' => '<p class="bnu-muted">' . t("Bootstrap's script is loaded as one bundle with every plugin (@size KB, loaded without holding up the page). Splitting it into single plugins needs their outside libraries too (Floating UI, Vanilla Calendar); until that's built, this list is for information. Plugins from sub-themes load next to it and can import it as <code>bootstrap</code>.", array('@size' => round(filesize(bootstrap_native_update_release_dir($version) . '/bootstrap/dist/js/bootstrap.bundle.min.js') / 1024))) . '</p>'),
    'list' => array('#markup' => '<div class="bnu-chips">' . implode('', array_map(function ($name) {
      return '<span class="bnu-badge bnu-badge-info">' . check_plain($name) . '</span>';
    }, bootstrap_native_update_bundle_plugins($version))) . '</div>'),
  );

  // Add-ons: what sub-themes bring; other packages come later.
  $extras = array();
  foreach ($context['sass'] as $file => $theme) {
    $extras[] = t('Sass module @file (from @theme)', array('@file' => bootstrap_native_update_relative_path($file), '@theme' => $theme));
  }
  foreach (bootstrap_native_update_theme_objects() as $name => $theme) {
    if (!empty($theme->info['bootstrap_native']['js'])) {
      foreach ((array) $theme->info['bootstrap_native']['js'] as $file) {
        $extras[] = t('Plugin @file (from @theme)', array('@file' => $file, '@theme' => $theme->info['name']));
      }
    }
  }
  $form['addons'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('bnu-section')),
    'title' => array('#markup' => '<h2>' . t('Add-ons') . '</h2>'),
    'text' => array('#markup' => $extras ? theme('item_list', array('items' => $extras)) : '<p class="bnu-muted">' . t('None installed. Add-ons (Sass modules and plugins from other packages) will be managed on their own page; a sub-theme can bring its own now (<code>bootstrap_native[sass][]</code> and <code>bootstrap_native[js][]</code> in its .info file).') . '</p>'),
  );

  $now = bootstrap_native_update_active() === '' ? filesize(bootstrap_native_update_theme_dir() . '/css/bootstrap_native.css') : bootstrap_native_update_builds()[bootstrap_native_update_active()]['meta']['size'];
  $form['summary'] = array(
    '#markup' => '<p class="bnu-size-summary"><strong>' . t('Stylesheet:') . '</strong> ' . t('@now KB now', array('@now' => round($now / 1024))) . ($info ? ' → <strong><span class="bnu-size-new" data-now="' . (int) $now . '" data-base="' . (int) $info['base_size'] . '">' . round($now / 1024) . '</span> KB</strong> ' . t('with these choices (about)') : '') . '</p>',
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['build'] = array(
    '#type' => 'submit',
    '#value' => t('Build and check'),
    '#disabled' => !$sass,
    '#attributes' => array('class' => array('button--primary')),
  );
  $form['actions']['reset'] = array(
    '#type' => 'submit',
    '#value' => t("Reset to the theme's defaults"),
    '#disabled' => !$sass,
    '#submit' => array('bootstrap_native_update_components_form_reset'),
  );
  $form['actions']['download'] = array(
    '#markup' => l(t('Download the built files (.tar.gz)'), bootstrap_native_update_action_path('download', 'active'), array('query' => bootstrap_native_update_action_query('download', 'active'), 'attributes' => array('class' => array('bnu-download')))),
  );
  $form['help'] = array('#markup' => '<p class="bnu-small">' . t("Docs: an icon linking to the part's page on getbootstrap.com, for the active release's docs; a part with no page found has none. Build and check runs the same checks as an update (a part the theme needs can't be left out; one a template uses shows as Check), then offers preview and apply. Download: the active build's stylesheet and script, for a site without a compiler or a sub-theme's repository.") . '</p>');
  return $form;
}

/**
 * A part's name for people (forms/otp-input: OTP input).
 */
function bootstrap_native_update_part_name($id) {
  $names = array(
    'otp-input' => 'OTP input',
    'vr' => 'Vertical rule',
    'zindex' => 'Z-index',
    'bg-color' => 'Background color',
    'rtl' => 'RTL',
  );
  $name = strpos($id, 'utilities:') === 0 ? substr($id, 10) : basename($id);
  return isset($names[$name]) ? $names[$name] : drupal_ucfirst(str_replace('-', ' ', $name));
}

/**
 * Plugins in a release's bundle (its export list).
 */
function bootstrap_native_update_bundle_plugins($version) {
  $bundle = (string) @file_get_contents(bootstrap_native_update_release_dir($version) . '/bootstrap/dist/js/bootstrap.bundle.min.js');
  if (!preg_match_all('/export\s*\{([^}]*)\}/', $bundle, $m)) {
    return array();
  }
  $names = array();
  foreach (explode(',', end($m[1])) as $item) {
    $parts = preg_split('/\s+as\s+/', trim($item));
    $names[] = trim(end($parts));
  }
  return array_filter($names);
}

/**
 * Optional parts whose classes a theme's templates use.
 *
 * @return array
 *   Keys theme, file, keyed by part.
 */
function bootstrap_native_update_parts_in_templates($version, array $context) {
  module_load_include('inc', 'bootstrap_native_update', 'includes/checks');
  $scss = bootstrap_native_update_release_dir($version) . '/bootstrap/scss';
  $used = array();
  $themes = bootstrap_native_update_check_themes();
  $theme_classes = array();
  foreach ($themes as $name => $dir) {
    $theme_classes[$name] = bootstrap_native_update_theme_classes($dir);
  }
  $list = list_themes();
  foreach ($context['parts'] as $part => $group) {
    $file = $scss . '/' . dirname($part) . '/_' . basename($part) . '.scss';
    if (dirname($part) == '.') {
      $file = $scss . '/_' . $part . '.scss';
    }
    if (!is_file($file)) {
      continue;
    }
    $tmp = file_directory_temp() . '/bnu-part-' . getmypid();
    @mkdir($tmp);
    copy($file, $tmp . '/_x.scss');
    // Only the part's own classes (.carousel, .carousel-item), not shared
    // ones it also styles (.active, .show).
    $stem = preg_replace('/s$/', '', basename($part));
    $classes = array();
    foreach (bootstrap_native_update_source_classes($tmp) as $class => $yes) {
      if (strpos($class, $stem) === 0) {
        $classes[$class] = TRUE;
      }
    }
    file_unmanaged_delete_recursive($tmp);
    foreach ($theme_classes as $name => $used_classes) {
      if ($hit = array_intersect_key($used_classes, $classes)) {
        $used[$part] = array('theme' => isset($list[$name]) ? $list[$name]->info['name'] : $name, 'file' => reset($hit));
        break;
      }
    }
  }
  return $used;
}

/**
 * The selection the form's checkboxes make.
 */
function bootstrap_native_update_components_selection(array $values) {
  $selection = array('parts' => array(), 'utilities' => array());
  foreach ($values as $id => $on) {
    if (!$on) {
      continue;
    }
    if (strpos($id, 'utilities:') === 0) {
      $selection['utilities'][] = substr($id, 10);
    }
    else {
      $selection['parts'][] = $id;
    }
  }
  return $selection;
}

/**
 * Form submit: Build and check.
 */
function bootstrap_native_update_components_form_submit($form, &$form_state) {
  bootstrap_native_update_components_start(bootstrap_native_update_components_selection($form_state['values']['styles']), $form_state);
}

/**
 * Form submit: Reset to the theme's defaults.
 */
function bootstrap_native_update_components_form_reset($form, &$form_state) {
  bootstrap_native_update_components_start(NULL, $form_state);
}

/**
 * Builds and checks the active release with a selection (a batch).
 */
function bootstrap_native_update_components_start($selection, &$form_state) {
  $version = bootstrap_native_update_active_version();
  $plan = bootstrap_native_update_plan_build($version, $selection);
  if ($plan['build'] === bootstrap_native_update_active()) {
    drupal_set_message(t('Those are the parts the site already uses.'));
    return;
  }
  if ($plan['build'] === '') {
    // The theme's own files: nothing to build or check.
    bootstrap_native_update_activate('');
    drupal_set_message(t("Back to the theme's own parts (Bootstrap @version).", array('@version' => $version)));
    return;
  }
  $steps = array('build' => t('Build a test stylesheet with these parts'), 'check' => t('Check for breaks'));
  batch_set(array(
    'title' => t('Building Bootstrap @version with these parts', array('@version' => $version)),
    'operations' => array(
      array('bootstrap_native_update_components_step', array('build', $version, $selection)),
      array('bootstrap_native_update_components_step', array('check', $version, $selection)),
    ),
    'init_message' => bootstrap_native_update_steps_html($steps, 'build'),
    'progress_message' => '',
    'error_message' => t('The build stopped with an error.'),
    'finished' => 'bootstrap_native_update_batch_finished',
    'file' => drupal_get_path('module', 'bootstrap_native_update') . '/includes/components.inc',
    'css' => array(drupal_get_path('module', 'bootstrap_native_update') . '/css/bootstrap_native_update.css'),
  ));
  $form_state['redirect'] = 'admin/appearance/bootstrap-updates/result/' . $plan['build'];
}

/**
 * Batch operation: build, then check, with a selection.
 */
function bootstrap_native_update_components_step($step, $version, $selection, &$context) {
  if (!empty($context['results']['error'])) {
    return;
  }
  $steps = array('build' => t('Build a test stylesheet with these parts'), 'check' => t('Check for breaks'));
  if ($step == 'build') {
    $made = bootstrap_native_update_make_build($version, $selection);
    $context['results']['build'] = $made['build'];
    $context['message'] = bootstrap_native_update_steps_html($steps, 'check');
    return;
  }
  $plan = bootstrap_native_update_plan_build($version, $selection);
  bootstrap_native_update_check_build($plan['build']);
  $context['message'] = bootstrap_native_update_steps_html($steps, '');
}

/**
 * Sets the batch that measures a release's parts and finds their docs.
 */
function bootstrap_native_update_measure_batch($version) {
  $context = bootstrap_native_update_parts_context($version);
  $ids = array();
  foreach (array_keys($context['parts']) as $part) {
    if ($part != 'utilities/api') {
      $ids[] = $part;
    }
  }
  foreach (array_keys($context['utility_groups']) as $group) {
    $ids[] = 'utilities:' . $group;
  }
  $operations = array(array('bootstrap_native_update_measure_step', array($version, array('base'))));
  foreach (array_chunk($ids, 12) as $chunk) {
    $operations[] = array('bootstrap_native_update_measure_step', array($version, $chunk));
  }
  $operations[] = array('bootstrap_native_update_measure_step', array($version, array('docs')));
  batch_set(array(
    'title' => t('Measuring the parts of Bootstrap @version', array('@version' => $version)),
    'operations' => $operations,
    'init_message' => t('Building each part on its own to see its size (once per release).'),
    'progress_message' => t('@current of @total'),
    'error_message' => t('Measuring stopped with an error.'),
    'file' => drupal_get_path('module', 'bootstrap_native_update') . '/includes/components.inc',
  ));
}

/**
 * Batch operation: measure some parts (or the base, or find docs pages).
 */
function bootstrap_native_update_measure_step($version, array $ids, &$context) {
  $all = variable_get('bootstrap_native_update_part_info', array());
  $info = isset($all[$version]) ? $all[$version] : array('sizes' => array(), 'docs' => array(), 'base_size' => 0, 'utilities_base_size' => 0);
  if ($ids == array('docs')) {
    $info['docs_base'] = bootstrap_native_update_docs_base($version);
    $info['docs'] = bootstrap_native_update_find_docs($version, $info['docs_base']);
    $info['measured'] = REQUEST_TIME;
  }
  else {
    $parts = bootstrap_native_update_parts_context($version);
    if ($ids == array('base')) {
      $info['base_size'] = bootstrap_native_update_measure($version, array(), array(), $parts);
      $info['utilities_base_size'] = bootstrap_native_update_measure($version, array(), array('-'), $parts);
    }
    else {
      foreach ($ids as $id) {
        if (strpos($id, 'utilities:') === 0) {
          $size = bootstrap_native_update_measure($version, array(), array(substr($id, 10)), $parts);
          $info['sizes'][$id] = max(0, $size - $info['utilities_base_size']);
        }
        else {
          $size = bootstrap_native_update_measure($version, array($id), array(), $parts);
          $info['sizes'][$id] = max(0, $size - $info['base_size']);
        }
      }
    }
  }
  $all[$version] = $info;
  variable_set('bootstrap_native_update_part_info', $all);
  $context['message'] = t('Measured @count parts', array('@count' => count($info['sizes'])));
}

/**
 * The size of a stylesheet with only Bootstrap's base and the given parts.
 *
 * @param array $utilities
 *   Utility groups to keep; array('-') for the utilities API with none.
 */
function bootstrap_native_update_measure($version, array $parts, array $utilities, array $context) {
  $sass = bootstrap_native_update_sass();
  $base = array('config', 'colors', 'theme', 'root');
  $effective = array('parts' => array_merge($base, $parts), 'utilities' => $utilities);
  $files = bootstrap_native_update_entry_files($effective, $context['utility_groups']);
  // Only Bootstrap: no theme rules.
  $files['bootstrap_native.scss'] = str_replace("@use \"drupal\";\n", '', $files['bootstrap_native.scss']);
  $tmp = file_directory_temp() . '/bootstrap-native-measure-' . getmypid() . '-' . mt_rand();
  mkdir($tmp, 0700);
  foreach ($files as $name => $text) {
    file_put_contents($tmp . '/' . $name, $text);
  }
  $compiled = bootstrap_native_update_compile($sass['path'], $tmp . '/bootstrap_native.scss', array($tmp, bootstrap_native_update_release_dir($version)));
  file_unmanaged_delete_recursive($tmp);
  return $compiled['css'] === NULL ? 0 : strlen($compiled['css']);
}

/**
 * Finds the docs page of every part (one request: a docs page's sidebar
 * lists them all).
 */
function bootstrap_native_update_find_docs($version, $docs_base) {
  $docs = array();
  $version_docs = trim(substr($docs_base, strlen('https://getbootstrap.com/docs/')), '/');
  $pages = array();
  foreach (array('components/alert/', 'forms/form-control/', 'getting-started/install/') as $page) {
    $response = drupal_http_request($docs_base . $page, array('timeout' => 15));
    if ($response->code == 200 && ($pages = bootstrap_native_update_docs_pages($response->data, $version_docs))) {
      break;
    }
  }
  if (!$pages) {
    return $docs;
  }
  $context = bootstrap_native_update_parts_context($version);
  foreach (array_keys($context['parts']) as $part) {
    $docs[$part] = bootstrap_native_update_docs_page($part, $pages);
  }
  foreach (array_keys($context['utility_groups']) as $group) {
    $docs['utilities:' . $group] = bootstrap_native_update_docs_page('utilities:' . $group, $pages);
  }
  return array_filter($docs);
}

/**
 * Returns HTML for the parts table.
 */
function theme_bootstrap_native_update_components_table($variables) {
  $element = $variables['element'];
  $badges = array(
    'required' => array('bnu-badge-required', t('Required')),
    'clash' => array('bnu-badge-off', t('Clashes with Drupal')),
    'needed' => array('bnu-badge-needed', t('Needed by a sub-theme')),
    'used' => array('bnu-badge-check', t('Used in a template')),
  );
  $icon = '<svg aria-hidden="true" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 19.5V5a2 2 0 0 1 2-2h14v16H6a2 2 0 0 0-2 2z"/><path d="M8 7h8M8 11h6"/></svg>';
  $rows = array();
  $last_group = NULL;
  foreach (element_children($element) as $id) {
    $item = $element[$id];
    if ($item['#bnu_group'] !== $last_group) {
      $last_group = $item['#bnu_group'];
      $rows[] = array('data' => array(array('data' => $element['#groups'][$last_group], 'colspan' => 5)), 'class' => array('bnu-group'));
    }
    $why = isset($badges[$item['#bnu_state']]) ? '<span class="bnu-badge ' . $badges[$item['#bnu_state']][0] . '">' . $badges[$item['#bnu_state']][1] . '</span> ' : '<span class="bnu-small">' . t('Optional') . '</span> ';
    $why .= $item['#bnu_why'] !== '' ? '<span class="bnu-small">' . $item['#bnu_why'] . '</span>' : '';
    $size = $item['#bnu_size'] === NULL ? '—' : ($item['#bnu_state'] == 'clash' ? '—' : ($item['#bnu_size'] < 1024 ? t('<1 KB') : t('@kb KB', array('@kb' => round($item['#bnu_size'] / 1024)))));
    $docs = $item['#bnu_docs'] !== ''
      ? '<a class="bnu-docs" href="' . check_url($item['#bnu_docs']) . '" title="' . t('Docs and examples') . '" aria-label="' . t('Docs and examples: @part', array('@part' => $item['#title'])) . '">' . $icon . '</a>'
      : '<span class="bnu-muted">—</span>';
    $rows[] = array(
      array('data' => drupal_render($element[$id]), 'class' => array('bnu-check-cell')),
      '<label for="' . $element[$id]['#id'] . '"><strong>' . check_plain($item['#title']) . '</strong></label>',
      $why,
      array('data' => $size, 'class' => array('bnu-size')),
      array('data' => $docs, 'class' => array('bnu-docs-cell')),
    );
  }
  $header = array(
    array('data' => '<span class="element-invisible">' . t('Include') . '</span>'),
    t('Part'),
    t('Why'),
    t('Size'),
    t('Docs'),
  );
  return '<div class="bnu-section"><h2>' . t('Styles (Sass modules)') . '</h2><div class="bnu-table">' . theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('class' => array('bnu-parts')))) . '</div></div>';
}

/**
 * Sends the active build (or the theme's own files) as a .tar.gz.
 */
function bootstrap_native_update_download_build() {
  require_once DRUPAL_ROOT . '/modules/system/system.tar.inc';
  $active = bootstrap_native_update_active();
  $label = bootstrap_native_update_build_label($active);
  if ($active === '') {
    $css = bootstrap_native_update_theme_dir() . '/css/bootstrap_native.css';
    $js = bootstrap_native_update_theme_dir() . '/js/bootstrap.bundle.min.js';
  }
  else {
    $dir = drupal_realpath(bootstrap_native_update_build_uri($active));
    $css = $dir . '/bootstrap_native.css';
    $js = $dir . '/bootstrap.bundle.min.js';
  }
  $name = 'bootstrap-native-' . $label['version'] . ($active === '' ? '' : '-' . substr($active, strrpos($active, '~') + 1));
  $tmp = file_directory_temp() . '/' . $name . '-' . mt_rand();
  mkdir($tmp . '/' . $name, 0700, TRUE);
  copy($css, $tmp . '/' . $name . '/bootstrap_native.css');
  copy($js, $tmp . '/' . $name . '/bootstrap.bundle.min.js');
  file_put_contents($tmp . '/' . $name . '/README.txt', "Bootstrap Native's built files: Bootstrap " . $label['version'] . ' with ' . $label['parts'] . ".\nbootstrap_native.css: the theme's stylesheet. bootstrap.bundle.min.js: Bootstrap's script (an ES module).\n");
  $archive = $tmp . '/' . $name . '.tar.gz';
  $tar = new Archive_Tar($archive, 'gz');
  $tar->createModify(array($tmp . '/' . $name), '', $tmp);
  drupal_add_http_header('Content-Type', 'application/gzip');
  drupal_add_http_header('Content-Disposition', 'attachment; filename="' . $name . '.tar.gz"');
  drupal_add_http_header('Content-Length', filesize($archive));
  drupal_send_headers();
  readfile($archive);
  file_unmanaged_delete_recursive($tmp);
  drupal_exit();
}
