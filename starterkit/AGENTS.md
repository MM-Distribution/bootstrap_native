# STARTERKIT_NAME (STARTERKIT): notes for AI agents

This is a Drupal 7 **sub-theme of Bootstrap Native**. It holds this site's
own styles, templates and theme code. Bootstrap Native and its updates
never touch this folder, so this is where site changes go.

For how the base theme and its module work (builds, Bootstrap updates,
the components page, hooks, variables), read the module's notes. They are
in `AGENTS.md` in the Bootstrap Native module folder, usually
`sites/all/modules/bootstrap_native/AGENTS.md`. To find it:
`drush pm-info bootstrap_native_update`, or search for
`bootstrap_native_update.info`.

## Files

    STARTERKIT.info        name, base theme, regions, features, settings, Bootstrap additions
    css/STARTERKIT.css     this site's styles (loaded after Bootstrap Native's)
    template.php           preprocess hooks and theme functions: STARTERKIT_...
    templates/             template overrides (*.tpl.php)

## How to change things

- **Styles:** edit `css/STARTERKIT.css`.
  - The file isn't in a cascade layer and Bootstrap's CSS is, so plain
    rules here win over Bootstrap without `!important`.
  - Use Bootstrap's variables (`var(--bs-...)`) rather than fixed values.
  - Add another stylesheet with `stylesheets[all][] = css/other.css` in
    the .info file.
- **Colors:** set them on Appearance > Settings > STARTERKIT_NAME
  (primary, secondary, base, text, link, link hover). The base theme works
  out readable text, hover and dark-mode colors from them. Don't
  hard-code brand colors in CSS. To override other Bootstrap variables:
  `:root { --bs-border-radius: .25rem; }`.
- **Templates:**
  - Copy a template into `templates/` and edit it. Sources:
    - the base theme's `templates/` (in the module folder, under
      `themes/bootstrap_native`);
    - a module's own templates;
    - core's (`modules/node/node.tpl.php`).
  - Name it after the theme hook suggestion: `node--article.tpl.php`,
    `page--front.tpl.php`, `block--system--main.tpl.php`.
- **PHP:** functions in `template.php` are named `STARTERKIT_<hook>`:
  - `STARTERKIT_preprocess_page(&$variables)` runs after the base theme's
    preprocess.
  - `STARTERKIT_button($variables)` overrides the base theme's
    `bootstrap_native_button()`.
- **Bootstrap parts, Sass and plugins** (in the .info file):

      bootstrap_native[parts][] = carousel             ; a Bootstrap part the site's build must include
      bootstrap_native[sass][] = scss/_STARTERKIT.scss ; a Sass module, built in after Bootstrap
      bootstrap_native[js][] = js/STARTERKIT.js        ; an ES module, loaded after Bootstrap's script

  - A Sass module can `@use "bootstrap/scss/config" as *;` for Bootstrap's
    variables and mixins.
  - A plugin can `import { Tooltip } from "bootstrap"`. It can also wait
    for `document.addEventListener("bootstrap-native:ready", ...)` and use
    `window.bootstrap`.
  - These need the Sass compiler: `drush bootstrap-native-sass`, and
    `--install` if it's missing.
- **After any change** to the .info file, templates, `template.php` or
  Sass: clear the caches (`drush cc all`). The site's Bootstrap build is
  redone then if needed.

## Keep

- **Drupal's markup and classes.** Add Bootstrap classes alongside
  Drupal's, don't replace them; modules and their JavaScript rely on
  Drupal's structure.
- **`base theme = bootstrap_native`**, and the regions and features in
  the .info file. If Bootstrap Native gains a region, add it here too
  (sub-themes don't inherit regions in D7).
- **`.menu` is Drupal's menu class.** Bootstrap's dropdown is `.bs-menu`
  in this theme. `.placeholder`, `.progress` and `.form-text` are
  Drupal's too: Bootstrap's versions aren't built.

## Don't

- Edit Bootstrap Native's own files (in the module folder). Updates
  overwrite them. Override from here instead.
- Copy Bootstrap's CSS into this theme. Ask for a part with
  `bootstrap_native[parts][]`, or on Appearance > Bootstrap components.

## Docs

- Drupal 7 sub-themes: https://www.drupal.org/docs/7/theming/creating-a-sub-theme
- Theme .info files: https://www.drupal.org/docs/7/theming/writing-theme-info-files
- Overriding output (templates, theme functions): https://www.drupal.org/docs/7/theming/overriding-themable-output
- Template suggestions: https://www.drupal.org/docs/7/theming/template-theme-hook-suggestions
- Theme hooks and preprocess: https://api.drupal.org/api/drupal/modules%21system%21theme.api.php/7.x
- Bootstrap 6: https://getbootstrap.com/docs/6.0/
