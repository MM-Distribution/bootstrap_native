STARTERKIT_NAME
====================

A sub-theme of Bootstrap Native, created by
`drush bootstrap-native-subtheme`.

- css/STARTERKIT.css: this site's styles (loaded after Bootstrap Native's).
- template.php: preprocess hooks and theme functions (STARTERKIT_...).
- templates/: template overrides.

Bootstrap's parts, Sass and plugins: see the end of STARTERKIT.info.
- bootstrap_native[parts][] adds a Bootstrap part to the site's build.
- bootstrap_native[parts_fixed] = TRUE builds this theme with only its
  parts[] (and bootstrap_native[utilities][] groups), whatever the site
  chooses: for a theme made for its own pages. Otherwise its settings page
  (step 6) can give it its own parts.
- bootstrap_native[sass][] builds a Sass module of this theme in after
  Bootstrap (it can @use "bootstrap/scss/..." and Bootstrap's variables).
- bootstrap_native[js][] loads an ES module after Bootstrap's script; it can
  import Bootstrap by name: import { Tooltip } from "bootstrap".
Changing them needs a cache clear (the build is redone then) and a Sass
compiler (drush bootstrap-native-sass --install).

Regions and features were copied from Bootstrap Native when this theme was
created (a sub-theme in Drupal 7 doesn't inherit them); keep them in step if
Bootstrap Native adds a region.

Bootstrap Native updates (git pull, or the Bootstrap updates page) never
touch this folder.
