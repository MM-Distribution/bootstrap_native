#!/bin/bash
# Builds themes/bootstrap_native/css/bootstrap_native.css (from scss/) and
# js/bootstrap.bundle.min.js (from vendor/bootstrap), then checks the CSS
# custom properties against Bootstrap's own list.
#
#   build/build.sh          compressed CSS
#   build/build.sh --dev    expanded CSS (readable, for debugging)
#
# Needs Dart Sass (https://sass-lang.com/install; standalone release, no
# Node): SASS=/path/to/sass, sites/all/libraries/dart-sass, `sass` on the
# PATH or ~/tools/dart-sass. The work is in includes/build.inc, shared with
# the Bootstrap updates page.
DIR="$(cd "$(dirname "$0")" && pwd)"
PHP="${PHP:-$(command -v php84 || command -v php)}"
exec "$PHP" -d log_errors=0 "$DIR/build.php" "$@"
