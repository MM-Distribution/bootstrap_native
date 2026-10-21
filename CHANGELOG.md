# Changelog

## 7.x-1.0-dev

- 2026-10-20: First build on clauded7. Bootstrap 6.0.0-alpha.1 from Sass
  (only the parts used), Drupal's markup kept and styled through `@extend`;
  `.menu` renamed `.bs-menu` in Bootstrap's CSS and JS. Forms, fieldsets,
  vertical tabs, messages (with roles), tabs, breadcrumb, pager, tables,
  menus, navbar with a phone drawer, layout following the sidebars. Color
  settings (primary, secondary, base, text, link, link hover) with a live
  preview and readability adjustments. Tests: color math, page checks.
- 2026-10-20: One folder installed as a module (`bootstrap_native_update`,
  shown as "Bootstrap Native"); the theme moved to `themes/bootstrap_native/`
  and is registered through `hook_system_theme_info()`. The module can't be
  disabled while the theme or a sub-theme is enabled. The build moved from
  Python to PHP (`includes/build.inc`, same output byte for byte); the SCSS
  loads Bootstrap through the load path (`bootstrap/scss/...`).
- 2026-10-20: Bootstrap updates (Appearance > Bootstrap updates): release
  list from npm, check (checksum, safe unpack, test build, checks against
  the release in use), results, per-admin preview, apply, roll back,
  cleanup after 30 days, rebuild after a theme change with fallback to the
  shipped release. Status report row and theme settings notice. Drush:
  `bootstrap-native-sass` (install/update Dart Sass with GitHub's checksum
  and a test build), `bootstrap-native-check`, `bootstrap-native-subtheme`
  (from `starterkit/`). The theme loads Bootstrap's script inline as a
  module import (the loader file is gone). Tests: checks_test.php,
  update_test.sh with test releases, update_ui.py.
- 2026-10-20: Bootstrap components (Appearance > Bootstrap components):
  choose Bootstrap's parts and utility groups (read from each release),
  with sizes and docs links, built and checked like an update; download of
  the built files. Builds are now a release plus a choice of parts
  (`<version>~<parts>`). Sub-themes can add parts, Sass modules and ES
  module plugins in their .info; scripts load through an import map.
  Create a sub-theme from Appearance (or as a .tar.gz). The theme's SCSS
  entry is split into `_bootstrap-parts.scss` and `_drupal.scss` (same
  output).
- 2026-10-21: AGENTS.md (notes for AI agents: layout, rules, builds and
  state, functions, hooks, pages, drush, sub-theme keys, tests), with a
  CLAUDE.md that imports it; new sub-themes get their own pair from
  starterkit/, pointing back to the module's. "For later" notes on the
  components page's scripts and add-ons sections.
