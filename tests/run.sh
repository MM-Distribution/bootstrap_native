#!/bin/bash
# Runs all the tests.
#
#   tests/run.sh          everything: unit tests, theme pages, Bootstrap updates
#   tests/run.sh colors   only the plain-PHP tests (color math, build, checks)
#   tests/run.sh pages    plain-PHP tests and the theme's pages
#   tests/run.sh update   plain-PHP tests and the Bootstrap updates (with
#                         test releases; leaves the site on the shipped one)
#
# DRUPAL_ROOT defaults to clauded7, BASE to its URL; screenshots go to OUT
# (default: a temporary folder, printed at the end).
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
export DRUPAL_ROOT="${DRUPAL_ROOT:-$HOME/public_html/dev/claude.dev}"
export BASE="${BASE:-https://claude.dev.asphaltsealcoatingdirect.com}"
case "$DRUPAL_ROOT ${BASE:-}" in
  *public_html/live/*|*public_html/h/*|*//asphaltsealcoatingdirect.com*|*//www.asphaltsealcoatingdirect.com*)
    echo "Refusing to run tests against the live site ($DRUPAL_ROOT ${BASE:-})." >&2; exit 1 ;;
esac
PHP="${PHP:-$(command -v php84 || echo /opt/cpanel/ea-php84/root/usr/bin/php)}"
DRUSH="${DRUSH:-$(command -v drush84 || command -v drush)}"
status=0

echo "== Colors"
"$PHP" -d log_errors=0 "$DIR/color_test.php" || status=1
echo "== Build and checks"
"$PHP" -d log_errors=0 "$DIR/checks_test.php" || status=1
export PLAYWRIGHT_BROWSERS_PATH="${PLAYWRIGHT_BROWSERS_PATH:-/opt/playwright/browsers}"

if [ "${1:-}" = "" ] || [ "${1:-}" = "pages" ]; then
  echo "== Pages"
  OUT="${OUT:-$(mktemp -d)}"
  "${PYTHON:-/opt/playwright/bin/python}" "$DIR/screens.py" "$OUT" / /user /user/register || status=1
  LOGIN="$("$DRUSH" --root="$DRUPAL_ROOT" --uri="$BASE" uli --name="${TEST_USER:-claude}" --no-browser 2>/dev/null)" \
    "${PYTHON:-/opt/playwright/bin/python}" "$DIR/screens.py" "$OUT" ${TEST_PATHS:-/node/add/article /node/7} || status=1
  echo "Screenshots: $OUT"
fi

if [ "${1:-}" = "" ] || [ "${1:-}" = "update" ]; then
  echo "== Bootstrap updates"
  export DRUSH
  "$DIR/update_test.sh" || status=1
fi
exit $status
