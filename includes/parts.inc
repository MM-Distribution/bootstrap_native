<?php

/**
 * @file
 * Bootstrap's parts (Sass modules) and what a build includes.
 *
 * Plain PHP, no Drupal (tests/checks_test.php runs it directly).
 *
 * A part is one of Bootstrap's Sass modules, named by its path under scss/
 * (accordion, forms/otp-input, content/reboot). The list comes from each
 * release's own scss/bootstrap.scss (its @forward stack, folders expanded
 * through their index.scss), so a new release brings its own parts. The
 * utilities are one part in Bootstrap; here they are split into the groups
 * Bootstrap marks in _utilities.scss (scss-docs-start utils-*).
 *
 * A selection: array('parts' => array(ids), 'utilities' => array(groups)).
 * NULL means the theme's default (scss/_bootstrap-parts.scss).
 */

/**
 * Parts that are always built: Bootstrap's variables, its root, reboot.
 */
function bootstrap_native_update_base_parts() {
  return array('config', 'colors', 'theme', 'root', 'content/reboot');
}

/**
 * The @forward or @use targets in a Sass file, in order.
 */
function bootstrap_native_update_scss_targets($file, $rule = 'forward') {
  if (!is_file($file)) {
    return array();
  }
  $scss = preg_replace(array('#/\*.*?\*/#s', '#(?<!:)//[^\n]*#'), '', file_get_contents($file));
  preg_match_all('/@' . $rule . '\s+"([^"]+)"/', $scss, $m);
  return $m[1];
}

/**
 * A release's parts, in Bootstrap's order.
 *
 * @param string $scss_dir
 *   The release's bootstrap/scss folder.
 *
 * @return array
 *   Group (base, content, layout, forms, buttons, components, helpers,
 *   utilities), keyed by part.
 */
function bootstrap_native_update_release_parts($scss_dir) {
  $parts = array();
  $base = bootstrap_native_update_base_parts();
  foreach (bootstrap_native_update_scss_targets($scss_dir . '/bootstrap.scss') as $target) {
    // The banner is only Bootstrap's license comment.
    if ($target == 'banner') {
      continue;
    }
    if ($target == 'utilities/api') {
      $parts['utilities/api'] = 'utilities';
      continue;
    }
    if (is_file($scss_dir . '/' . $target . '/index.scss')) {
      foreach (bootstrap_native_update_scss_targets($scss_dir . '/' . $target . '/index.scss') as $child) {
        $parts[$target . '/' . $child] = $target;
      }
      continue;
    }
    $parts[$target] = in_array($target, $base) ? 'base' : 'components';
  }
  if (isset($parts['content/reboot'])) {
    $parts['content/reboot'] = 'base';
  }
  return $parts;
}

/**
 * The utility groups of a release, with their $utilities keys.
 *
 * @return array
 *   Keys of Bootstrap's $utilities map, keyed by group (vertical-align,
 *   margin, ...), in Bootstrap's order.
 */
function bootstrap_native_update_utility_groups($scss_dir) {
  $file = $scss_dir . '/_utilities.scss';
  $groups = array();
  if (!is_file($file)) {
    return $groups;
  }
  $group = 'other';
  foreach (explode("\n", file_get_contents($file)) as $line) {
    if (preg_match('/scss-docs-start utils-([a-z0-9-]+)/', $line, $m)) {
      $group = $m[1];
    }
    elseif (preg_match('/^    "([a-z0-9-]+)": \(/', $line, $m)) {
      $groups[$group][] = $m[1];
    }
  }
  return $groups;
}

/**
 * Parts the theme's own partials use (they are always built).
 */
function bootstrap_native_update_required_parts($theme_dir, $scss_dir) {
  $required = bootstrap_native_update_base_parts();
  foreach (glob($theme_dir . '/scss/_*.scss') as $file) {
    if (basename($file) == '_bootstrap-parts.scss') {
      continue;
    }
    foreach (bootstrap_native_update_scss_targets($file, 'use') as $target) {
      if (strpos($target, 'bootstrap/scss/') === 0) {
        $required = array_merge($required, bootstrap_native_update_expand_part(substr($target, strlen('bootstrap/scss/')), $scss_dir));
      }
    }
  }
  return array_values(array_unique($required));
}

/**
 * A part, or the parts of a folder (buttons: buttons/button, ...).
 */
function bootstrap_native_update_expand_part($target, $scss_dir) {
  if (!is_file($scss_dir . '/' . $target . '/index.scss')) {
    return array($target);
  }
  $parts = array();
  foreach (bootstrap_native_update_scss_targets($scss_dir . '/' . $target . '/index.scss') as $child) {
    $parts[] = $target . '/' . $child;
  }
  return $parts;
}

/**
 * The theme's default selection (scss/_bootstrap-parts.scss).
 */
function bootstrap_native_update_default_selection($theme_dir, $scss_dir) {
  $parts = array();
  $utilities = array();
  foreach (bootstrap_native_update_scss_targets($theme_dir . '/scss/_bootstrap-parts.scss', 'use') as $target) {
    $target = substr($target, strlen('bootstrap/scss/'));
    if ($target == 'utilities/api') {
      $utilities = array_keys(bootstrap_native_update_utility_groups($scss_dir));
      continue;
    }
    $parts = array_merge($parts, bootstrap_native_update_expand_part($target, $scss_dir));
  }
  return array('parts' => $parts, 'utilities' => $utilities);
}

/**
 * What a build includes: the selection, plus what must be there, minus what
 * can't be, in Bootstrap's order.
 *
 * @param array|null $selection
 *   The site's selection (NULL: the theme's default).
 * @param array $context
 *   Keys: parts (the release's, from bootstrap_native_update_release_parts()),
 *   utility_groups, default (the theme's selection), required, needed
 *   (parts sub-themes list), exclude (part => reason).
 *
 * @return array
 *   A selection with keys parts and utilities.
 */
function bootstrap_native_update_effective_selection($selection, array $context) {
  if ($selection === NULL) {
    $selection = $context['default'];
  }
  $wanted = array_flip(array_merge($selection['parts'], $context['required'], $context['needed']));
  $parts = array();
  foreach ($context['parts'] as $part => $group) {
    if ($part != 'utilities/api' && isset($wanted[$part]) && !isset($context['exclude'][$part])) {
      $parts[] = $part;
    }
  }
  $utilities = array_values(array_intersect(array_keys($context['utility_groups']), $selection['utilities']));
  return array('parts' => $parts, 'utilities' => $utilities);
}

/**
 * A short name for a selection plus extra Sass ('default' for the theme's).
 */
function bootstrap_native_update_selection_key(array $effective, array $default_effective, array $extra_sass = array()) {
  if ($effective == $default_effective && !$extra_sass) {
    return 'default';
  }
  return substr(md5(serialize(array($effective, $extra_sass))), 0, 8);
}

/**
 * The generated Sass for a build.
 *
 * @param array $effective
 *   From bootstrap_native_update_effective_selection().
 * @param array $utility_groups
 *   The release's groups (bootstrap_native_update_utility_groups()).
 * @param array $extra_sass
 *   Sass modules from add-ons or sub-themes (load-path names), after
 *   Bootstrap's parts and before the theme's Drupal rules.
 *
 * @return array
 *   File contents keyed by file name: bootstrap_native.scss (the entry) and,
 *   when some utility groups are left out, _bnu-utilities.scss.
 */
function bootstrap_native_update_entry_files(array $effective, array $utility_groups, array $extra_sass = array()) {
  $files = array();
  $uses = array();
  $removed = array();
  foreach ($utility_groups as $group => $keys) {
    if (!in_array($group, $effective['utilities'])) {
      $removed = array_merge($removed, $keys);
    }
  }
  if ($effective['utilities'] && $removed) {
    // Runs before Bootstrap's utilities API reads the map (an @use can't
    // follow an assignment in the same file, so it is its own partial).
    // $utilities belongs to config; _utilities.scss fills it in, so it is
    // loaded first.
    $files['_bnu-utilities.scss'] = "// Generated: the utility groups left out of this build.\n@use \"sass:map\";\n@use \"bootstrap/scss/config\" as c;\n@use \"bootstrap/scss/utilities\";\nc.\$utilities: map.remove(c.\$utilities, \"" . implode('", "', $removed) . "\");\n";
    $uses[] = 'bnu-utilities';
  }
  foreach ($effective['parts'] as $part) {
    $uses[] = 'bootstrap/scss/' . $part;
  }
  if ($effective['utilities']) {
    $uses[] = 'bootstrap/scss/utilities/api';
  }
  foreach ($extra_sass as $module) {
    $uses[] = $module;
  }
  $uses[] = 'drupal';
  $entry = "// Generated by Bootstrap Native for one build: Bootstrap's parts, add-ons,\n// then the theme's Drupal rules.\n";
  foreach ($uses as $use) {
    $entry .= '@use "' . $use . '"' . ($use == 'drupal' ? '' : ' as ' . 'bnu' . substr(md5($use), 0, 6)) . ";\n";
  }
  $files['bootstrap_native.scss'] = $entry;
  return $files;
}

/**
 * The docs pages a getbootstrap.com docs page links to (its sidebar).
 *
 * @param string $html
 *   A docs page (https://getbootstrap.com/docs/6.0/components/alert/).
 * @param string $docs
 *   The docs version (6.0).
 *
 * @return array
 *   Paths under the docs (components/alert), as keys.
 */
function bootstrap_native_update_docs_pages($html, $docs) {
  preg_match_all('#href="/docs/' . preg_quote($docs, '#') . '/([a-z-]+/[a-z0-9-]+)/?"#', $html, $m);
  return array_flip($m[1]);
}

/**
 * The docs page for a part or utility group, or ''.
 *
 * @param string $part
 *   A part (forms/otp-input) or a utility group (utilities:margin).
 * @param array $pages
 *   From bootstrap_native_update_docs_pages().
 */
function bootstrap_native_update_docs_page($part, array $pages) {
  // Names Bootstrap's docs don't share with its files.
  $aliases = array(
    'buttons/button' => 'components/button',
    'buttons/button-group' => 'components/button-group',
    'buttons/close' => 'components/close-button',
    'chip' => 'forms/chips',
    'forms/chip-input' => 'forms/chips',
    'forms/labels' => 'forms/overview',
    'helpers/vr' => 'helpers/vertical-rule',
    'content/lists' => 'content/typography',
    'content/blockquote' => 'content/typography',
    'utilities:bg-color' => 'utilities/background',
    'utilities:underline' => 'utilities/text-decoration',
    'utilities:zindex' => 'utilities/z-index',
    'utilities:transition' => 'utilities/motion',
    'utilities:animation' => 'utilities/motion',
    'utilities:container' => 'utilities/container-queries',
    'utilities:color' => 'utilities/colors',
    'utilities:borders' => 'utilities/border',
    'utilities:links' => 'utilities/link',
    'utilities:shadow-color' => 'utilities/shadows',
    'utilities:shadow-opacity' => 'utilities/shadows',
    'utilities:text-break' => 'utilities/text-wrapping',
    'config' => 'customize/sass',
    'colors' => 'customize/color',
    'theme' => 'customize/theme',
    'root' => 'getting-started/css-variables',
    'transitions' => 'customize/transitions',
    'helpers/theme-colors' => 'customize/theme',
  );
  if (isset($aliases[$part]) && isset($pages[$aliases[$part]])) {
    return $aliases[$part];
  }
  if (strpos($part, 'utilities:') === 0) {
    $name = substr($part, 10);
    $sections = array('utilities');
  }
  else {
    $name = basename($part);
    $folder = dirname($part) == '.' ? 'components' : dirname($part);
    $sections = array_unique(array($folder == 'buttons' ? 'components' : $folder, 'components', 'forms', 'content', 'helpers', 'layout'));
  }
  $short = preg_replace('/^form-/', '', $name);
  foreach ($sections as $section) {
    foreach (array_unique(array($name, $short, $name . 's')) as $slug) {
      if (isset($pages[$section . '/' . $slug])) {
        return $section . '/' . $slug;
      }
    }
  }
  // Close names in the part's own section: text-wrap and text-wrapping,
  // strength and password-strength.
  foreach (array_keys($pages) as $page) {
    list($section, $slug) = explode('/', $page);
    if ($section == $sections[0] && strlen($short) >= 4 && (strpos($slug, $short) === 0 || substr($slug, -strlen($short)) === $short)) {
      return $page;
    }
  }
  return '';
}
