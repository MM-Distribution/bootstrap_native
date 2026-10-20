#!/bin/bash
# Runs the theme's tests.
#
#   tests/run.sh          color math, then screenshots + page checks
#   tests/run.sh colors   only the color math
#
# DRUPAL_ROOT defaults to clauded7, BASE to its URL; screenshots go to OUT
# (default: a temporary folder, printed at the end).
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
export DRUPAL_ROOT="${DRUPAL_ROOT:-$HOME/public_html/s/claude.dev}"
export BASE="${BASE:-https://claude.dev.asphaltsealcoatingdirect.com}"
case "$DRUPAL_ROOT ${BASE:-}" in
  *public_html/h/*|*//asphaltsealcoatingdirect.com*|*//www.asphaltsealcoatingdirect.com*)
    echo "Refusing to run tests against the live site ($DRUPAL_ROOT ${BASE:-})." >&2; exit 1 ;;
esac
PHP="${PHP:-$(command -v php84 || echo /opt/cpanel/ea-php84/root/usr/bin/php)}"
DRUSH="${DRUSH:-$(command -v drush84 || command -v drush)}"
status=0

echo "== Colors"
"$PHP" -d log_errors=0 "$DIR/color_test.php" || status=1

if [ "${1:-}" != "colors" ]; then
  echo "== Pages"
  OUT="${OUT:-$(mktemp -d)}"
  export PLAYWRIGHT_BROWSERS_PATH="${PLAYWRIGHT_BROWSERS_PATH:-/opt/playwright/browsers}"
  "${PYTHON:-/opt/playwright/bin/python}" "$DIR/screens.py" "$OUT" / /user /user/register || status=1
  LOGIN="$("$DRUSH" --root="$DRUPAL_ROOT" --uri="$BASE" uli --name="${TEST_USER:-claude}" --no-browser 2>/dev/null)" \
    "${PYTHON:-/opt/playwright/bin/python}" "$DIR/screens.py" "$OUT" ${TEST_PATHS:-/node/add/article /cart} || status=1
  echo "Screenshots: $OUT"
fi
exit $status
