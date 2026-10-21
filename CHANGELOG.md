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
- 2026-10-21: Missing or damaged build files repair themselves: a page
  with its build's files gone uses the theme's own files and flags a
  repair; cache clears and cron check the active and previous builds
  (size and fingerprint) and rebuild what doesn't match, re-downloading
  the release only if it's gone too. Nothing is touched when all is in
  order. "Measure again" link on the components page.
- 2026-10-21: Bootstrap Native is hidden: a foundation, not a site theme.
  Sites create their own theme (a sub-theme) from the prompt on Appearance,
  the module's pages and the status report (pre-filled with the site's
  name, made the default); a site on Bootstrap Native itself is offered
  "Move to your own theme" (settings, blocks; the base theme is disabled).
  The color settings preview uses the active build's stylesheet.
- 2026-10-21: Theme settings redesigned (approved mockup): five steps
  (brand, page, links, header, browser icon) beside a sticky preview with
  Page / Form / Messages samples (opens over the page on a phone), notes
  under each color (automatic hex, adjustments), Set / Automatic, a save
  bar with unsaved changes and Reset as a link. Colors take Bootstrap's
  names (orange, indigo-600), kept as typed; the update check flags a name
  a release drops.
- 2026-10-21: The automatic link hover color is a clearer step from the link
  (0.18 in OKLCH lightness, was 0.1; the other way for a link near black).
  The logo hint says PNG, JPG or GIF (Drupal 7 refuses SVG uploads there).
- 2026-10-21: Menus, tabs, the navbar and the breadcrumb follow the link
  and hover color settings (`--bn-nav-link-color`, `--bn-nav-link-hover-color`);
  with no link colors they keep Bootstrap's greys. Tests: the update tests
  use a new fixture folder each run and send `no-store`, so a caching proxy
  in front of the site can't serve removed fixtures.
- 2026-10-21: Theme settings: "Reset to Bootstrap's colors" only empties
  the six color fields (nothing saved, no question) and offers Undo; the
  save bar has "Discard changes" while anything is unsaved (a change undone
  by hand counts as saved). Without JavaScript, Reset shows the form again
  with the colors empty. SVG logos (and SVG browser icons, which core took
  as uploaded) are cleaned on upload by `includes/svg.inc`: SVG elements on
  a list, no event attributes or scripts, links only within the file, no
  styles that load anything; files with entities are refused.
