#!/bin/bash
# Bootstrap updates, end to end on a test site, with test releases from
# tests/make_fixtures.py served from the site's own files folder.
#
#   tests/update_test.sh
#
# Checks each test release's verdict, apply, roll back, the rebuild after a
# theme change (and the fallback when that fails), cleanup after 30 days,
# the sub-theme generator, and that the module can't be disabled. Leaves
# the site on the shipped release with no test files.
set -u
DIR="$(cd "$(dirname "$0")/.." && pwd)"
: "${DRUPAL_ROOT:?}" "${BASE:?}" "${DRUSH:?}"
D() { "$DRUSH" --root="$DRUPAL_ROOT" --uri="$BASE" "$@"; }
EV() { D ev "module_load_include('inc', 'bootstrap_native_update', 'includes/releases'); $1" 2>&1; }
failures=0
ok() { if [ "$1" = 0 ]; then echo "ok    $2"; else echo "FAIL  $2"; failures=$((failures + 1)); fi; }
has() { case "$1" in *"$2"*) return 0 ;; *) return 1 ;; esac; }

FILES="$(D ev 'echo variable_get("file_public_path", conf_path() . "/files");')"
FIX="$FILES/bootstrap-native-test"
SUB="bnu_test_sub"

cleanup() {
  EV "bootstrap_native_update_activate(''); foreach (array_keys(bootstrap_native_update_versions()) as \$v) { if (strpos(\$v, '-test.') !== FALSE) bootstrap_native_update_discard(\$v); } variable_del('bootstrap_native_update_registry'); variable_del('bootstrap_native_update_fallback'); variable_set('bootstrap_native_update_releases', array());" >/dev/null
  D ev "theme_disable(array('$SUB')); variable_del('theme_${SUB}_settings'); db_delete('system')->condition('name', '$SUB')->execute();" >/dev/null 2>&1
  /bin/rm -rf "${DRUPAL_ROOT:?}/sites/all/themes/$SUB" "${DRUPAL_ROOT:?}/${FIX:?}"
  sed -i '/^\/\/ bnu-test$/d' "$DIR/themes/bootstrap_native/scss/_layout.scss"
  D cc all >/dev/null 2>&1
}
trap cleanup EXIT
cleanup

python3 -I "$DIR/tests/make_fixtures.py" "$DIR/themes/bootstrap_native/vendor/bootstrap" "$DRUPAL_ROOT/$FIX" "$BASE/$FIX" >/dev/null
D vset bootstrap_native_update_registry "$BASE/$FIX/registry.json" >/dev/null 2>&1

echo "== Checks"
check() { D bootstrap-native-check "$1" 2>&1; }
out="$(check 6.0.1-test.1)"; has "$out" "Verdict: passed"; ok $? "6.0.1-test.1 (same as shipped): passed"
out="$(check 6.0.2-test.1)"; has "$out" "Verdict: breaks" && has "$out" "navbar-toggler-icon" && has "$out" "--bs-link-color"; ok $? "6.0.2-test.1: breaks (class removed, link color read in fewer places)"
out="$(check 6.0.3-test.1)"; has "$out" "Verdict: failed" && has "$out" "bootstrap/scss/badge"; ok $? "6.0.3-test.1: won't build, Sass's error shown"
out="$(check 6.0.4-test.1)"; has "$out" "refused"; ok $? "6.0.4-test.1: archive with ../ refused"
out="$(check 6.0.5-test.1)"; has "$out" "checksum"; ok $? "6.0.5-test.1: checksum mismatch refused"
out="$(EV 'echo implode(",", array_keys(bootstrap_native_update_versions()));')"
[ "$out" = "6.0.3-test.1,6.0.2-test.1,6.0.1-test.1" ]; ok $? "refused downloads leave no record ($out)"
[ ! -e "$DRUPAL_ROOT/$FILES/escaped.txt" ] && [ ! -e "$DRUPAL_ROOT/$FILES/../escaped.txt" ]; ok $? "nothing written outside the release folder"

echo "== Apply and roll back"
EV "bootstrap_native_update_save_version('6.0.1-test.1', array('applied' => REQUEST_TIME)); bootstrap_native_update_activate('6.0.1-test.1');" >/dev/null
page="$(curl -s "$BASE/user/login")"
has "$page" "bootstrap-native/builds/6.0.1-test.1/bootstrap_native.css" && has "$page" 'bootstrap-native\/builds\/6.0.1-test.1\/bootstrap.bundle.min.js'; ok $? "applied: pages use the build's stylesheet and script"
! has "$page" "themes/bootstrap_native/css/bootstrap_native.css"; ok $? "applied: the shipped stylesheet is gone from pages"
code="$(curl -s -o /dev/null -w '%{http_code}' "$BASE/$FILES/bootstrap-native/builds/6.0.1-test.1/bootstrap_native.css")"
[ "$code" = 200 ]; ok $? "applied: the build's stylesheet is served ($code)"
out="$(D pm-disable -y bootstrap_native_update 2>&1)"; has "$out" "no extensions that could be disabled"; ok $? "the module can't be disabled while the theme is enabled"

echo "== Theme changed (git pull, then caches cleared)"
stamp() { EV "\$v = bootstrap_native_update_versions(); echo \$v['6.0.1-test.1']['build']['stamp'];"; }
echo "// bnu-test" >> "$DIR/themes/bootstrap_native/scss/_layout.scss"
D cc all >/dev/null 2>&1
after="$(stamp)"
hash_ok="$(EV "\$v = bootstrap_native_update_versions(); echo \$v['6.0.1-test.1']['build']['theme_hash'] === bootstrap_native_update_theme_hash() ? 'yes' : 'no';")"
[ "$hash_ok" = yes ] && [ -n "$after" ]; ok $? "the active build is rebuilt for the changed theme"
active="$(EV 'echo bootstrap_native_update_active();')"
[ "$active" = "6.0.1-test.1" ]; ok $? "still on 6.0.1-test.1 after the rebuild"
# A rebuild that fails: the release's files are gone.
/bin/rm -rf "$DRUPAL_ROOT/$FILES/bootstrap-native/releases/6.0.1-test.1"
echo "// bnu-test" >> "$DIR/themes/bootstrap_native/scss/_layout.scss"
D cc all >/dev/null 2>&1
out="$(EV "echo bootstrap_native_update_active(), '|', variable_get('bootstrap_native_update_fallback') ? 'fallback' : 'none';")"
[ "$out" = "|fallback" ]; ok $? "a failed rebuild falls back to the shipped release and says so ($out)"
status="$(D core-requirements --severity=1 2>&1)"
has "$status" "could not be rebuilt"; ok $? "the status report explains the fallback"
sed -i '/^\/\/ bnu-test$/d' "$DIR/themes/bootstrap_native/scss/_layout.scss"
page="$(curl -s "$BASE/user/login")"
has "$page" "themes/bootstrap_native/css/bootstrap_native.css"; ok $? "after the fallback pages use the shipped stylesheet"

echo "== Cleanup after 30 days"
EV "bootstrap_native_update_save_version('6.0.2-test.1', array('inactive_since' => REQUEST_TIME - 31 * 86400)); bootstrap_native_update_save_version('6.0.3-test.1', array('inactive_since' => REQUEST_TIME - 29 * 86400));" >/dev/null
D cron >/dev/null 2>&1
out="$(EV 'echo implode(",", array_keys(bootstrap_native_update_versions()));')"
! has "$out" "6.0.2-test.1" && has "$out" "6.0.3-test.1"; ok $? "cron removes releases inactive for 30+ days only ($out)"
[ ! -d "$DRUPAL_ROOT/$FILES/bootstrap-native/releases/6.0.2-test.1" ] && [ ! -d "$DRUPAL_ROOT/$FILES/bootstrap-native/builds/6.0.2-test.1" ]; ok $? "its files are removed too"

echo "== Sub-theme"
out="$(D bootstrap-native-subtheme Bad-Name 2>&1)"; has "$out" "machine name"; ok $? "refuses a bad machine name"
out="$(D bootstrap-native-subtheme bootstrap_native 2>&1)"; has "$out" "already the name"; ok $? "refuses an existing name"
out="$(D bootstrap-native-subtheme $SUB --path=../../etc 2>&1)"; has "$out" "sites/all/themes"; ok $? "refuses a folder outside sites/*/themes"
out="$(D bootstrap-native-subtheme $SUB --name='Test "Sub" \ Theme' --enable 2>&1)"; has "$out" "Created sites/all/themes/$SUB"; ok $? "creates sites/all/themes/$SUB"
T="$DRUPAL_ROOT/sites/all/themes/$SUB"
info="$(cat "$T/$SUB.info" 2>/dev/null)"
has "$info" 'name = "Test Sub Theme"' && has "$info" "base theme = bootstrap_native" && has "$info" "regions[sidebar_first]" && has "$info" "features[] = main_menu"; ok $? ".info: cleaned name, base theme, regions and features copied"
[ -f "$T/template.php" ] && [ -f "$T/css/$SUB.css" ] && /usr/bin/grep -q "${SUB}_preprocess_page" "$T/template.php" && ! /usr/bin/grep -rq STARTERKIT "$T"; ok $? "files renamed, STARTERKIT replaced everywhere"
out="$(D ev "\$t = list_themes(TRUE); echo \$t['$SUB']->status, '|', \$t['$SUB']->info['base theme'], '|', count(\$t['$SUB']->info['regions']) == count(\$t['bootstrap_native']->info['regions']) ? 'same regions' : 'other regions';")"
[ "$out" = "1|bootstrap_native|same regions" ]; ok $? "enabled, built on Bootstrap Native, same regions ($out)"
out="$(D ev "echo json_encode(array_intersect_key(variable_get('theme_${SUB}_settings', array()), array('bootstrap_native_color_primary' => 1)) == array_intersect_key(variable_get('theme_bootstrap_native_settings', array()), array('bootstrap_native_color_primary' => 1)) ? 'same' : 'differs');")"
[ "$out" = '"same"' ]; ok $? "color settings copied from Bootstrap Native"
out="$(D bootstrap-native-subtheme $SUB 2>&1)"; has "$out" "already"; ok $? "refuses to overwrite it"
mkdir -p "$T/templates"
printf '<span class="navbar-toggler-icon"></span>\n' > "$T/templates/node--article.tpl.php"
out="$(check 6.0.2-test.1)"
has "$out" "Sub-theme: templates/node--article.tpl.php"; ok $? "the checks look at the sub-theme: its class shows as Check"

echo "== Pages (browser)"
EV "bootstrap_native_update_activate(''); foreach (array_keys(bootstrap_native_update_versions()) as \$v) bootstrap_native_update_discard(\$v); bootstrap_native_update_refresh_releases();" >/dev/null
SHOTS="${OUT:-$(mktemp -d)}"
LOGIN="$(D uli --name="${TEST_USER:-claude}" --no-browser 2>/dev/null)" PLAYWRIGHT_BROWSERS_PATH="${PLAYWRIGHT_BROWSERS_PATH:-/opt/playwright/browsers}" \
  "${PYTHON:-/opt/playwright/bin/python}" "$DIR/tests/update_ui.py" "$SHOTS" | sed '/^all passed$/d;/ failed$/d'
ok "${PIPESTATUS[0]}" "pages: all browser checks (screenshots: $SHOTS)"

echo "== Back to the shipped release"
cleanup
trap - EXIT
out="$(EV "echo bootstrap_native_update_active_version(), '|', count(bootstrap_native_update_versions());")"
[ "$out" = "6.0.0-alpha.1|0" ]; ok $? "shipped release active, no test releases left ($out)"

[ $failures = 0 ] && echo "all passed" || echo "$failures failed"
exit $failures
