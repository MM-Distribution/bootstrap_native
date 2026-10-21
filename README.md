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

    AGENTS.md                   notes for AI agents (CLAUDE.md imports it);
                                new sub-themes get their own from starterkit/
    bootstrap_native_update.*   the module (and its drush commands)
    includes/                   the module's code (build.inc: the build,
                                parts.inc: Bootstrap's parts)
    themes/bootstrap_native/    the theme
    starterkit/                 what a new sub-theme starts from
    build/                      developer build
    tests/                      tests

## Bootstrap updates

Appearance > Bootstrap updates (permission: *Check and apply Bootstrap
updates*). New Bootstrap 6 releases come from npm; nothing happens until
someone presses a button.

1. **Check** a release (the newest, or one under *Older versions*):
   download, verify npm's checksum, unpack (only `scss/`, the script bundle,
   LICENSE, package.json; archives with links or paths outside the package
   are refused), build the theme against it, run the checks. A few
   seconds; visitors see no change.
2. **Results:** passed, passed with things to look at (Check), would break
   (Breaks: apply only after ticking *I understand*), or doesn't build
   (nothing to apply). The checks compare with the release in use:
   - the build itself (a removed partial, mixin or `@extend` target fails);
   - the `.menu` rename still reaching Bootstrap's dropdown;
   - every color variable the settings set still read where it was;
   - classes Bootstrap dropped that the theme or an enabled sub-theme uses
     (Breaks in the theme, Check in a sub-theme);
   - variables used but defined nowhere; stylesheet size.
3. **Preview:** only the admin previewing sees the new build (a bar on top
   of the site), visitors keep the current one.
4. **Apply** or **roll back** (instant, no rebuild). Inactive releases are
   removed by cron 30 days after they stop being active; the release the
   theme ships with is never removed and is always the fallback.

Where things live: downloads in `public://bootstrap-native/releases/`,
builds in `public://bootstrap-native/builds/<version>~<parts>/` (per site,
always writable); state in `bootstrap_native_update_*` variables. A build
is a release with a choice of parts: `~default` is the theme's own choice,
anything else a hash of the choice. The release the theme ships with,
with its own parts, is the theme's prebuilt files and needs no build.

**After a code update** (git pull, then clearing caches) the active
release is rebuilt against the new theme code; if that can't be done, the
site goes back to the shipped release and the status report says why.

**Missing or damaged files** (a site copied without its files folder, a
cleanup): a page whose build files are gone uses the theme's own files
instead, and flags a repair. Each cache clear and cron run checks the
active and previous builds against the size and fingerprint recorded when
they were made, and rebuilds any that don't match, downloading the release
from npm again (checksum checked) only if its files are gone too. When all
is in order nothing is touched. If a repair fails, the site stays on the
theme's own files and the status report says why.

**Settings:** offer pre-releases (on while Bootstrap 6 has no stable
release), check daily from cron (off by default), Sass compiler path.

## Bootstrap components

Appearance > Bootstrap components: which of Bootstrap's parts (Sass
modules) the site's stylesheet includes.

- **The list comes from each release:** its `scss/bootstrap.scss` (the
  `@forward` stack, folders expanded) and the utility groups Bootstrap
  marks in `_utilities.scss`. A new release brings its own parts.
- **States:** required (the theme's Drupal rules `@use` them, plus
  Bootstrap's variables and reboot), clashes with Drupal (locked off: the
  theme's `bootstrap_native[exclude]` in its .info), needed by a sub-theme,
  used in a template (a part's own classes found in a theme's files), or
  optional.
- **Sizes and docs** are measured once per release (each part built on
  its own; one request to getbootstrap.com, whose sidebar lists every docs
  page). The Docs column links to a part's page; no page found, no link.
- **Build and check** builds the active release with the choice and runs
  the update checks; then preview, apply, roll back as for an update.
  *Reset to the theme's defaults* goes back to `scss/_bootstrap-parts.scss`.
- **Download the built files** gives the active stylesheet and script as a
  .tar.gz.
- Scripts are one bundle (every plugin); the page lists them from the
  bundle's exports.

The build writes a generated entry (Bootstrap's parts in Bootstrap's order,
a partial that removes left-out utility groups from `$utilities`, add-ons,
then `scss/_drupal.scss`). The theme's default choice builds byte for byte
the stylesheet the theme ships.

### What a sub-theme can add

In its .info (the generated one has these commented out):

    bootstrap_native[parts][] = carousel           ; a Bootstrap part it needs
    bootstrap_native[sass][] = scss/_extras.scss   ; a Sass module, after Bootstrap
    bootstrap_native[js][] = js/plugin.js          ; an ES module, after Bootstrap's script

After a cache clear the site's build is redone to include them (needs the
compiler; the status report says when it can't). Scripts load through an
import map, so a plugin can `import { Tooltip } from "bootstrap"`. Paths
must stay inside the sub-theme. Add-ons from other packages (npm) will use
the same build entry and import map; their page comes once there are any.

### Sass compiler

Checks need [Dart Sass](https://sass-lang.com/install), looked for at the
path setting, `sites/all/libraries/dart-sass`, then the PATH. Without it
releases are announced but can't be checked or applied.

    drush bootstrap-native-sass             # what's installed, the newest release
    drush bootstrap-native-sass --install   # download and unpack to sites/all/libraries/dart-sass
    drush bootstrap-native-sass --update    # replace it; the old copy stays as dart-sass.previous

The right build for the server (Linux/macOS, x64/ARM, musl), checked
against the SHA-256 GitHub publishes, test-built with the theme before it
is switched in (`--force` to switch anyway), and an `.htaccess` keeps the
folder off the web. Only drush installs the compiler: web pages never
download a program they would later run.

    drush bootstrap-native-check [VERSION]  # the same check as the page

## Sub-themes

    drush bootstrap-native-subtheme mysite --name="My Site" [--enable|--default] [--path=sites/all/themes]

Or Appearance > *Create a Bootstrap Native sub-theme* (permission:
*Create Bootstrap Native sub-themes*), which can also give the files as a
.tar.gz instead of writing them.

Creates `sites/all/themes/mysite` from `starterkit/`: a stylesheet loaded
after Bootstrap Native's (unlayered, so its rules win over Bootstrap's), an
empty `template.php`, `templates/`. Regions and features are copied from
Bootstrap Native (a D7 sub-theme doesn't inherit them), so are its theme
settings (colors, logo). The machine name is checked
(`[a-z][a-z0-9_]{1,31}`, not taken, folder not there); the human name is
cleaned to one line of plain text and never goes into PHP.

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

    tests/run.sh            # everything (on clauded7 by default)
    tests/run.sh colors     # plain PHP only: color math, build, checks
    tests/run.sh pages      # ...and the theme's pages
    tests/run.sh update     # ...and Bootstrap updates

Page checks fail on JavaScript errors, 4xx/5xx requests, sideways scrolling
at phone width, or Bootstrap's JavaScript not loading.

The update tests serve test releases from the site's files folder
(`tests/make_fixtures.py`: one that passes, one that breaks, one that
doesn't build, a tampered archive, a bad checksum) and go through every
verdict, apply, roll back, the rebuild after a theme change and its
fallback, cleanup, the sub-theme generator, and the page in a browser
(check, preview, apply, roll back, the *I understand* lock, phone width).
They leave the site on the shipped release.

## Status and next steps

7.x-1.0-dev, being built on clauded7 (2026-10-20). Done: Bootstrap updates
and components pages, sub-theme generator (page and drush), the compiler
drush command. Theme: forms, fieldsets,
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
- The add-ons page (npm lookup) once Bootstrap 6 add-ons exist.
- Single plugins instead of the bundle (needs Floating UI and Vanilla
  Calendar pinned too).
