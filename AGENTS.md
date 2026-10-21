# Bootstrap Native: notes for AI agents

Read this before changing anything here. It explains what the project is,
where things are, the rules that keep it working, and how to test. The
README is the human guide; this file is the map. Sub-themes created from
this project have their own `AGENTS.md` (from `starterkit/AGENTS.md`)
that points back here.

## What it is

A Drupal 7 project in one folder (`sites/all/modules/bootstrap_native`):

- **The theme** `bootstrap_native` (`themes/bootstrap_native/`): Bootstrap
  6 styling on Drupal's *own* markup. It never rewrites form markup the way
  Drupal Bootstrap 7.x-3.x does; it maps Bootstrap's CSS onto Drupal's
  classes with Sass `@extend`.
- **The module** `bootstrap_native_update` (shown as "Bootstrap Native"):
  registers the theme, and provides the Bootstrap updates page, the
  components page, the sub-theme generator and drush commands.

The module must stay enabled while the theme (or a sub-theme of it) is
enabled; it marks itself required then (`hook_system_info_alter()`).

**Bootstrap Native is hidden** (`hidden = TRUE`): a foundation, not a site
theme. A site uses a sub-theme of it, made with the generator;
`bootstrap_native_update_site_theme_state()` says whether it has one ('' =
yes; 'base' = the default is Bootstrap Native itself, from before it was
hidden; 'none'). Without one, Appearance (`hook_help`), the module's
pages and the status report offer to create it, and
`bootstrap_native_update_subtheme_enable()` makes it the default. Enabling
it first copies the default theme's blocks; a move off Bootstrap Native
disables the base theme. Theme settings are per theme in D7: the
generator copies Bootstrap Native's into the new theme.

Targets: Drupal 7 core, PHP 7.4 to 8.4 (no PHP 8-only syntax), browsers
that run Bootstrap 6 (Chrome/Edge 130+, Firefox 132+, Safari 18+). Code
should port to Backdrop CMS later: plain Drupal 7 APIs, no Composer.

## Layout

    bootstrap_native_update.info|module|install|drush.inc   the module
    includes/
      build.inc        compile + post-process (plain PHP, no Drupal)
      parts.inc        Bootstrap's parts, the generated Sass entry, docs matching (plain PHP)
      checks.inc       the checks a build goes through (plain PHP)
      releases.inc     registry, downloads, builds, apply, roll back, cleanup, rebuild
      admin.inc        Bootstrap updates page, results, preview bar, one-click actions
      components.inc   Bootstrap components page, measuring parts, built-files download
      sass.inc         installing Dart Sass (drush only)
      subtheme.inc     sub-theme generator; subtheme_form.inc is its page
    css/, js/          the module's admin pages
    starterkit/        what a new sub-theme starts from (STARTERKIT is replaced)
    themes/bootstrap_native/
      bootstrap_native.info   regions, features, settings, bootstrap_native[exclude]
      template.php            loads includes/{color,page,form,navigation}.inc
      includes/               theme functions and preprocess (see below)
      templates/              html.tpl.php, page.tpl.php
      scss/                   bootstrap_native.scss = _bootstrap-parts.scss + _drupal.scss
      css/bootstrap_native.css, js/bootstrap.bundle.min.js   prebuilt, committed
      vendor/bootstrap/       the shipped Bootstrap release (never edit)
      theme-settings.php      color settings with live preview
    build/build.sh     developer build of the prebuilt files
    tests/run.sh       all tests

## Rules that keep it working

- **Keep Drupal's markup.** Theme functions only add classes or attributes
  and call core's own function for the rest. Modules and their JavaScript
  expect core's structure and classes.
- **Never edit `vendor/`.** Bootstrap changes come in as releases (the
  updates page) or by replacing the folder and rebuilding.
- **After changing anything in `scss/`, run `build/build.sh`** and commit
  the rebuilt `css/` and `js/` files. Sites need no build tools.
- **Class names Drupal and Bootstrap share:**
  - `.menu` (Bootstrap's dropdown) is renamed `.bs-menu` in Bootstrap's
    CSS and JS at build. Use `class="bs-menu"` for a Bootstrap dropdown.
  - `.placeholder`, `.progress`, `.form-text` and floating labels are never
    built (`bootstrap_native[exclude]` in the theme's .info).
- **Cascade layers:** Bootstrap's CSS is in layers. The theme's Drupal
  rules sit in Bootstrap's `custom` layer (above components, below
  utilities). Unlayered CSS (modules, sub-themes) wins over all of it
  without `!important`.
- **Custom properties** get Bootstrap's `--bs-` prefix at build. The
  theme's own use `--bn-`.
- **Colors** come from theme settings (`includes/color.inc`: OKLCH math,
  readable text worked out); don't hard-code brand colors in CSS. A
  setting is hex or a Bootstrap name (`indigo-600`), kept as typed;
  `bootstrap_native_color_load_palette()` reads the release in use's
  `_colors.scss` (module: `bootstrap_native_update_palette_file()`) before
  `bootstrap_native_color_parse()` / `_tokens()` see a name. The settings
  form (`theme-settings.php`) moves core's logo, toggles and favicon into
  its steps (their names unchanged) and creates `$form['actions']` itself:
  core adds Save after the theme's alter.
- **New settings default to off**, and module UI gets a mockup first.

## How a page gets Bootstrap

- **Stylesheet:** the theme's `css/bootstrap_native.css`, unless a build
  is active. The module's `hook_css_alter()` swaps in
  `public://bootstrap-native/builds/<build>/bootstrap_native.css`.
- **Script:** `includes/page.inc` writes an import map
  (`{"imports": {"bootstrap": <bundle URL>}}`), then
  `import("bootstrap")`. That sets `window.bootstrap` and fires the
  `bootstrap-native:ready` event on `document`. Sub-theme plugins load
  after it as `<script type="module">` and can
  `import { Tooltip } from "bootstrap"`.
- **jQuery:** core's jQuery stays for Drupal's own scripts. Bootstrap 6
  doesn't use it.

## Builds and state

- **A build** is a Bootstrap release built with a choice of parts, named
  `<version>~<key>`: key `default` for the theme's own choice, else 8 hex
  characters (a hash of the choice and any extra Sass).
- **`''` means the theme's prebuilt files:** the shipped release with
  default parts. It never needs building and is always the fallback.

Variables (all `bootstrap_native_update_*`):

| Variable | What |
|---|---|
| `active`, `previous` | build names in use / before ('' = prebuilt files) |
| `selection` | the site's choice of parts: NULL = the theme's, else `array('parts' => ids, 'utilities' => groups)` |
| `builds` | records per build: version, input, effective, sass, meta (size, stamp, theme_hash, counts), check, applied, inactive_since, error |
| `versions` | downloaded releases: downloaded, uid, inactive_since, error, build |
| `releases`, `checked` | Bootstrap 6 releases from the npm registry, when last fetched |
| `last_build` | the build the last release check made |
| `part_info` | per release: part sizes, docs pages (measured once) |
| `fallback`, `needs_build` | set when a rebuild or repair failed (`reason` = `files` for a repair) / a sub-theme's additions couldn't be built |
| `repair` | set by a page that found the build's files missing; cleared by the repair |
| `prereleases` (1), `daily` (0), `sass_path` (''), `registry` (npm) | settings; `registry` is for tests |

Files:
- downloads: `public://bootstrap-native/releases/<version>/bootstrap/`;
- builds: `public://bootstrap-native/builds/<build>/`;
- Dart Sass: `sites/all/libraries/dart-sass` (or the path setting, or the PATH).

## Functions worth knowing (includes/releases.inc unless noted)

- `bootstrap_native_update_active()`, `_active_version()`,
  `_shipped_version()`: what's in use.
- `bootstrap_native_update_plan_build($version, $selection = FALSE)`:
  which build a choice makes, without making it. FALSE means the site's
  choice; NULL means the theme's.
- `bootstrap_native_update_make_build($version, $selection)`: compile it
  and record it. Returns `array('build', 'error')`.
- `bootstrap_native_update_check_build($build)`: run the checks against
  the active build and record the result.
- `bootstrap_native_update_activate($build)`: switch the site (it also
  sets the selection, starts the 30 days for what stopped being used and
  clears CSS/JS/page caches).
- `bootstrap_native_update_discard_build($build)`,
  `_discard_release($version)`, `_cleanup()` (cron: removes what has been
  inactive for 30 days).
- `bootstrap_native_update_rebuild_if_stale()` (`hook_flush_caches()`):
  after a code update, rebuilds the active build. If that can't be done,
  it falls back to `''`.
- `bootstrap_native_update_repair()` (`hook_flush_caches()`, so also
  cron): if the active or previous build's files are missing or don't
  match their recorded size and stamp
  (`bootstrap_native_update_build_intact()`), rebuilds them with
  `_restore_build()`. That re-downloads the release with
  `_fetch_release()` only if its files are gone. When everything matches
  it touches nothing. `bootstrap_native_update_files()` falls back to
  `''` on a page whose build files are missing and sets the `repair`
  flag.
- `bootstrap_native_update_files()` (.module): CSS path and JS URL for the
  current page (the admin's preview, else the active build), or NULL.
- `bootstrap_native_update_theme_scripts($theme)` (.module): the
  `bootstrap_native[js]` URLs of a theme and its base themes.
- `bootstrap_native_update_run_checks($old, $new, $themes, $colors)`
  (checks.inc): the checks. Verdict `passed`, `check`, `breaks` or
  `failed`, with rows of `code` plus `values` (worded in `admin.inc`'s
  `bootstrap_native_update_row_text()`).
- `bootstrap_native_update_build($sass, $theme_dir, $load_paths, $dev, $entry)`
  (build.inc): compile + `--bs-` prefix + `.menu` rename.
- `bootstrap_native_update_entry_files()`,
  `_effective_selection()`, `_release_parts()`, `_utility_groups()`
  (parts.inc): the parts model.
- `bootstrap_native_update_subtheme_create($machine, $name, $parent)`,
  `_subtheme_enable($machine, $default)`, `_subtheme_suggest($name)`
  (subtheme.inc).

## Hooks the module implements

| Hook | Purpose |
|---|---|
| `system_theme_info` | registers the theme |
| `system_info_alter` | marks the module required while the theme is in use |
| `permission` | the module's two permissions |
| `menu` | the module's pages |
| `theme` | the components table |
| `css_alter` | swaps in the build's stylesheet |
| `page_build` | adds the preview bar |
| `form_system_theme_settings_alter` | update notice on theme settings |
| `cron` | daily release check (if on), cleanup |
| `flush_caches` | rebuild after code changes |
| `requirements` | status report row |
| `uninstall` | removes the module's variables and files |

The theme implements:
- `css_alter`, which drops core's system.theme, system.menus,
  system.messages and vertical-tabs CSS;
- `preprocess_html`, `process_html` and `preprocess_page`;
- the theme functions `button`, `status_messages`, `menu_local_tasks`,
  `menu_local_task`, `breadcrumb` and `table`;
- `form_system_theme_settings_alter`, for the color settings.

## Pages, permissions, drush

- `admin/appearance/bootstrap-updates` (and `/result/<version|build>`,
  `/action/<op>/<target>?token=`): permission *administer bootstrap native
  updates*.
- `admin/appearance/bootstrap-components`: same permission.
- `admin/appearance/bootstrap-native-subtheme`: permission *create
  bootstrap native sub-themes*.
- One-click actions take a `drupal_get_token()` token. Everything else is
  a form post.
- drush:
  - `bootstrap-native-sass [--install|--update] [--force]`;
  - `bootstrap-native-check [version]`;
  - `bootstrap-native-subtheme <machine> [--name] [--path] [--enable|--default]`.

## What sub-themes can declare (.info)

    base theme = bootstrap_native
    bootstrap_native[parts][] = carousel           ; a Bootstrap part to build in
    bootstrap_native[sass][] = scss/_extras.scss   ; a Sass module, after Bootstrap's parts
    bootstrap_native[js][] = js/plugin.js          ; an ES module plugin, after Bootstrap's script

- **When they take effect:** after a cache clear. The build is redone,
  which needs Dart Sass.
- **Paths:** must stay inside the sub-theme (`..` is ignored).
- **Regions and features:** a D7 sub-theme doesn't inherit them, so the
  generator copies them.

## Tests

    tests/run.sh            # everything (defaults to the clauded7 test site)
    tests/run.sh colors     # plain PHP: color math, build, parts, checks
    tests/run.sh pages      # + the theme's pages in a browser (Playwright)
    tests/run.sh update     # + updates/components/sub-themes, with test releases

- **Test releases:** `tests/make_fixtures.py` makes them, served from the
  site's files folder: pass, breaks, won't build, tampered archive, bad
  checksum. The tests leave the site on `''` with no test files.
- **Before committing:** run what fits the change.
- **Never run the tests against a live site:** run.sh refuses known live
  paths.

## Security notes

- Only drush installs or replaces the Sass compiler. Web pages never
  download a program they later run.
- Releases come from npm and are checked against npm's integrity
  (sha512). Archives with links or paths outside `package/` are refused,
  and only `scss/`, the bundle, LICENSE and package.json are unpacked.
- The sub-theme name is cleaned to one line of plain text and never goes
  into PHP. The machine name is `[a-z][a-z0-9_]{1,31}`.

## More

- Drupal 7 theming: https://www.drupal.org/docs/7/theming
- Drupal 7 API: https://api.drupal.org/api/drupal/7.x
- Bootstrap 6 docs: https://getbootstrap.com/docs/6.0/
