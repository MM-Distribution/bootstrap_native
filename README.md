# Bootstrap Native

A Drupal 7 base theme: Bootstrap 6 styling on Drupal's own markup.

## Why

Drupal Bootstrap (7.x-3.x, Bootstrap 3) rebuilds Drupal's form markup to
match Bootstrap's: checkboxes inside their labels, descriptions as
`.help-block`, fieldsets as panels, inputs as `<button>`, extra input-group
wrappers. Modules and their JavaScript expect core's markup, so sites ended
up undoing parts of it (asd_bs's `template.php` moves inputs back out of
labels).

This theme keeps Drupal's markup (structure, classes, IDs) and brings
Bootstrap's look to it instead:

- The stylesheet extends Bootstrap's rules onto Drupal's classes
  (`.form-text` gets `.form-control`'s styles, `.form-checkbox` gets
  `.check`'s, `div.messages.error` gets `.alert.theme-danger`'s). See
  `scss/_drupal-forms.scss` and `scss/_drupal-ui.scss`.
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
`includes/color.inc` (OKLCH math, output as Bootstrap's CSS variables):

- Brand colors used as backgrounds stay exact; the text on them is white or
  black, whichever reads.
- Colors used as text are darkened or lightened just enough for WCAG AA
  (4.5:1) on the base color; the settings page lists each adjustment.
- A dark base color switches the page to Bootstrap's dark scheme
  (`data-bs-theme`). Without one, pages are light even when the visitor's
  system is dark (sites aren't designed for both).

## Building

The compiled `css/bs_native.css` and `js/bootstrap.bundle.min.js` are in
the repository; sites need no build tools. To change the SCSS or update
Bootstrap:

    build/build.sh          # compressed
    build/build.sh --dev    # readable

Needs [Dart Sass](https://sass-lang.com/install) (standalone, no Node;
here `~/tools/dart-sass`) and Python 3. `build/postprocess.py` adds
Bootstrap's `--bs-` prefix to custom properties (as Bootstrap's own build
does) and renames `.menu`; it fails if Bootstrap changed in a way it
doesn't expect.

Bootstrap: 6.0.0-alpha.1 in `vendor/bootstrap` (`scss/`, the minified
bundle, LICENSE). Needs Chrome/Edge 130+, Firefox 132+, Safari/iOS 18+.

Bootstrap's JavaScript is an ES module: loaded with `<script type="module">`
(D7's `drupal_add_js()` can't), exposed as `window.bootstrap` after the
`bs-native:ready` event. Core's jQuery stays for Drupal's own scripts.

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
  plugin templates (asd uses them; written for Bootstrap 3).
- Panels / Panelizer: layouts on CSS grid, a card pane style.
- Paragraphs: spacing between paragraph items, bundle templates.
- Ubercart pages that aren't uc_mc's (catalog, product attributes).
- Trim the stylesheet (Bootstrap's full utility set is most of its size).
- Sub-theme starter and the ASD sub-theme.
