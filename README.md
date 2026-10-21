# Bootstrap Native

A Drupal 7 base theme: Bootstrap 6 styling on Drupal's own markup.

## Installing

One folder, installed like a module: `sites/all/modules/bootstrap_native`
(a `git pull` or a new download updates everything). Enable the module
**Bootstrap Native** (`bootstrap_native_update`); the theme then appears
under Appearance. The theme lives in `themes/bootstrap_native/` inside the
folder, and the module tells Drupal about it (`hook_system_theme_info()`),
so the module can't be disabled while the theme or a sub-theme of it is
enabled.

    bootstrap_native_update.*   the module
    includes/                   the module's code (build.inc: the build)
    themes/bootstrap_native/    the theme
    build/                      developer build
    tests/                      tests

## Why

Drupal Bootstrap (7.x-3.x, Bootstrap 3) rebuilds Drupal's form markup to
match Bootstrap's: checkboxes inside their labels, descriptions as
`.help-block`, fieldsets as panels, inputs as `<button>`, extra input-group
wrappers. Modules and their JavaScript expect core's markup, so sites ended
up undoing parts of it in their sub-themes (moving inputs back out of
labels, for one).

This theme keeps Drupal's markup (structure, classes, IDs) and brings
Bootstrap's look to it instead:

- The stylesheet extends Bootstrap's rules onto Drupal's classes
  (`.form-text` gets `.form-control`'s styles, `.form-checkbox` gets
  `.check`'s, `div.messages.error` gets `.alert.theme-danger`'s). See
  `themes/bootstrap_native/scss/_drupal-forms.scss` and `_drupal-ui.scss`.
- Template overrides only add Bootstrap classes or accessibility attributes
  and call core's own function for the rest: buttons, tabs, breadcrumb,
  tables, status messages (`includes/`).
- Bootstrap 6's own checkbox markup is the same as Drupal's (input, then
  label, in one wrapper), so nothing has to move.
- Bootstrap's CSS sits in cascade layers, so any module stylesheet (not
  layered) wins over it without `!important`. The theme's Drupal rules sit
  in Bootstrap's `custom` layer: above its components, below its
  utilities.

## Class names Drupal and Bootstrap 6 share

| Class | Drupal | Bootstrap 6 | Here |
|---|---|---|---|
| `.menu` | every menu (`ul.menu`) | dropdown, `display: none` | Bootstrap's renamed `.bs-menu` (CSS and JS, at build) |
| `.placeholder` | `<em class="placeholder">` in messages | loading skeleton (gray block) | Bootstrap's left out |
| `.form-text` | text inputs | help text | Bootstrap's left out (help text is Drupal's `.description`) |
| `.progress` | progress bar (batch, uploads) | progress bar, other markup | Bootstrap's left out, Drupal's styled |

Use `class="bs-menu"` for a Bootstrap dropdown menu in your own markup.

## Colors

Settings: primary, secondary, base (background), text, link, link hover;
all optional (empty = Bootstrap's own). The theme works out the rest in
the theme's `includes/color.inc` (OKLCH math, output as Bootstrap's CSS variables):

- Brand colors used as backgrounds stay exact; the text on them is white or
  black, whichever reads.
- Colors used as text are darkened or lightened just enough for WCAG AA
  (4.5:1) on the base color; the settings page lists each adjustment.
- A dark base color switches the page to Bootstrap's dark scheme
  (`data-bs-theme`). Without one, pages are light even when the visitor's
  system is dark (sites aren't designed for both).

## Building

The compiled `css/bootstrap_native.css` and `js/bootstrap.bundle.min.js`
(in the theme folder) are in the repository; sites need no build tools. To
change the SCSS or the shipped Bootstrap:

    build/build.sh          # compressed
    build/build.sh --dev    # readable

Needs [Dart Sass](https://sass-lang.com/install) (standalone, no Node):
`SASS=/path/to/sass`, `sites/all/libraries/dart-sass`, `sass` on the PATH
or `~/tools/dart-sass`. The build (`includes/build.inc`, shared with the
Bootstrap updates page) adds Bootstrap's `--bs-` prefix to custom
properties (as Bootstrap's own build does) and renames `.menu`; it fails
if Bootstrap changed in a way it doesn't expect.

The SCSS loads Bootstrap as `bootstrap/scss/...` through Sass's load path:
the theme's `vendor/` folder, or a downloaded release.

Bootstrap: 6.0.0-alpha.1 in the theme's `vendor/bootstrap` (`scss/`, the minified
bundle, LICENSE). Needs Chrome/Edge 130+, Firefox 132+, Safari/iOS 18+.

Bootstrap's JavaScript is an ES module: loaded with `<script type="module">`
(D7's `drupal_add_js()` can't), exposed as `window.bootstrap` after the
`bootstrap-native:ready` event. Core's jQuery stays for Drupal's own scripts.

## Tests

    tests/run.sh            # color math + screenshots/page checks (clauded7)
    tests/run.sh colors     # color math only

Page checks fail on JavaScript errors, 4xx/5xx requests, sideways scrolling
at phone width, or Bootstrap's JavaScript not loading.

## Status and next steps

7.x-1.0-dev, being built on clauded7 (2026-10-20). Done: forms, fieldsets,
vertical tabs, messages, tabs, breadcrumb, pager, tables, menus, navbar
with a phone drawer, page layout (columns follow the sidebars that have
blocks), color settings with preview.

Next:
- Dropdowns in the main menu (expanded menu items as `.bs-menu`).
- Views: exposed filters inline, table and grid styles, the views_bootstrap
  plugin templates (written for Bootstrap 3).
- Panels / Panelizer: layouts on CSS grid, a card pane style.
- Paragraphs: spacing between paragraph items, bundle templates.
- Ubercart pages (catalog, product attributes).
- Trim the stylesheet (Bootstrap's full utility set is most of its size).
- Bootstrap updates page and sub-theme generator (in progress).
