#!/bin/bash
# Builds css/bootstrap_native.css (from scss/) and js/bootstrap.bundle.min.js (from
# vendor/bootstrap), then checks the CSS custom properties against
# Bootstrap's own list.
#
#   build/build.sh          compressed CSS
#   build/build.sh --dev    expanded CSS (readable, for debugging)
#
# Needs Dart Sass (https://sass-lang.com/install; standalone release, no
# Node): SASS=/path/to/sass or `sass` on the PATH (here: ~/tools/dart-sass).
set -e
DIR="$(cd "$(dirname "$0")/.." && pwd)"
SASS="${SASS:-$(command -v sass || echo "$HOME/tools/dart-sass/sass")}"
STYLE=compressed
[ "${1:-}" = "--dev" ] && STYLE=expanded
TMP="$(mktemp -d)"
trap '/bin/rm -rf "${TMP:?}"' EXIT

"$SASS" --no-source-map --style="$STYLE" --quiet-deps \
  --silence-deprecation=import "$DIR/scss/bootstrap_native.scss" "$TMP/bootstrap_native.css"
python3 "$DIR/build/postprocess.py" css "$TMP/bootstrap_native.css" "$DIR/css/bootstrap_native.css"
python3 "$DIR/build/postprocess.py" js "$DIR/vendor/bootstrap/dist/js/bootstrap.bundle.min.js" "$DIR/js/bootstrap.bundle.min.js"

# Every --bs- property used must be one Bootstrap defines somewhere (a typo
# or a missed prefix would silently fall back): compare with its sources.
python3 - "$DIR" <<'EOF'
import re, sys, pathlib
d = pathlib.Path(sys.argv[1])
css = (d / 'css/bootstrap_native.css').read_text()
defined = set(re.findall(r'(--bs-[\w-]+)\s*:', css))
used = set(re.findall(r'var\((--bs-[\w-]+)', css))
# Set at runtime: by Bootstrap's JavaScript or the theme's color settings.
known = set()
for f in (d / 'vendor/bootstrap/scss').rglob('*.scss'):
    known |= {'--bs-' + m for m in re.findall(r'--([a-z][\w-]*)', f.read_text())}
missing = sorted(u for u in used - defined if u not in known and '#{' not in u)
if missing:
    print('Used but never defined:', ', '.join(missing))
print('css: %d KB, %d properties defined' % (len(css) // 1024, len(defined)))
EOF
