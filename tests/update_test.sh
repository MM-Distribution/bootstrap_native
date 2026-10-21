#!/bin/bash
# Bootstrap updates, end to end on a test site, with test releases from
# tests/make_fixtures.py served from the site's own files folder.
#
#   tests/update_test.sh
#
# Checks each test release's verdict, apply, roll back, the rebuild after a
# theme change (and the fallback when that fails), cleanup after 30 days,
# choosing parts, the sub-theme generator and what sub-themes add (parts,
# Sass, plugins), that the module can't be disabled, then the pages in a
# browser. Leaves the site on the theme's own files with no test files.
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
B1="6.0.1-test.1~default"
SHIPPED="$(cat "$DIR/themes/bootstrap_native/vendor/bootstrap/VERSION")"
DEFAULT="$(D ev 'echo variable_get("theme_default", "bartik");')"
BASE_STATUS="$(D ev '$t = list_themes(TRUE); echo $t["bootstrap_native"]->status;')"

cleanup() {
  D vset theme_default "$DEFAULT" >/dev/null 2>&1
  [ "$BASE_STATUS" = 1 ] || D ev "theme_disable(array('bootstrap_native'));" >/dev/null 2>&1
  EV "bootstrap_native_update_activate(''); foreach (array_keys(bootstrap_native_update_builds()) as \$b) bootstrap_native_update_discard_build(\$b); foreach (array_keys(bootstrap_native_update_versions()) as \$v) bootstrap_native_update_discard_release(\$v); foreach (array('registry', 'fallback', 'needs_build', 'last_build', 'selection', 'previous', 'part_info') as \$k) variable_del('bootstrap_native_update_' . \$k); variable_set('bootstrap_native_update_releases', array());" >/dev/null
  D ev "theme_disable(array('$SUB')); variable_del('theme_${SUB}_settings'); db_delete('system')->condition('name', array('$SUB', 'bnu_ui_sub'))->execute(); variable_del('theme_bnu_ui_sub_settings');" >/dev/null 2>&1
  /bin/rm -rf "${DRUPAL_ROOT:?}/sites/all/themes/$SUB" "${DRUPAL_ROOT:?}/sites/all/themes/bnu_ui_sub" "${DRUPAL_ROOT:?}/${FIX:?}"
  sed -i '/^\/\/ bnu-test$/d' "$DIR/themes/bootstrap_native/scss/_layout.scss"
  D cc all >/dev/null 2>&1
}
trap cleanup EXIT
cleanup

python3 -I "$DIR/tests/make_fixtures.py" "$DIR/themes/bootstrap_native/vendor/bootstrap" "$DRUPAL_ROOT/$FIX" "$BASE/$FIX" >/dev/null
D vset bootstrap_native_update_registry "$BASE/$FIX/registry.json" >/dev/null 2>&1

echo "== Checks"
check() { D bootstrap-native-check "$1" 2>&1; }
out="$(check 6.0.1-test.1)"; has "$out" "Verdict: passed" && has "$out" "Build: $B1"; ok $? "6.0.1-test.1 (same as shipped): passed, built as $B1"
out="$(check 6.0.2-test.1)"; has "$out" "Verdict: breaks" && has "$out" "navbar-toggler-icon" && has "$out" "--bs-link-color"; ok $? "6.0.2-test.1: breaks (class removed, link color read in fewer places)"
out="$(check 6.0.3-test.1)"; has "$out" "Verdict: failed" && has "$out" "bootstrap/scss/badge"; ok $? "6.0.3-test.1: won't build, Sass's error shown"
out="$(check 6.0.4-test.1)"; has "$out" "refused"; ok $? "6.0.4-test.1: archive with ../ refused"
out="$(check 6.0.5-test.1)"; has "$out" "checksum"; ok $? "6.0.5-test.1: checksum mismatch refused"
out="$(EV 'echo implode(",", array_keys(bootstrap_native_update_versions()));')"
[ "$out" = "6.0.3-test.1,6.0.2-test.1,6.0.1-test.1" ]; ok $? "refused downloads leave no record ($out)"
[ ! -e "$DRUPAL_ROOT/$FILES/escaped.txt" ] && [ ! -e "$DRUPAL_ROOT/$FILES/../escaped.txt" ]; ok $? "nothing written outside the release folder"

echo "== Apply and roll back"
EV "bootstrap_native_update_save_build_record('$B1', array('applied' => REQUEST_TIME)); bootstrap_native_update_activate('$B1');" >/dev/null
page="$(curl -s "$BASE/user/login")"
has "$page" "bootstrap-native/builds/$B1/bootstrap_native.css" && has "$page" "bootstrap-native\\/builds\\/$B1\\/bootstrap.bundle.min.js"; ok $? "applied: pages use the build's stylesheet and script"
has "$page" '<script type="importmap">'; ok $? "applied: the import map names the build's script"
! has "$page" "themes/bootstrap_native/css/bootstrap_native.css"; ok $? "applied: the shipped stylesheet is gone from pages"
code="$(curl -s -o /dev/null -w '%{http_code}' "$BASE/$FILES/bootstrap-native/builds/$B1/bootstrap_native.css")"
[ "$code" = 200 ]; ok $? "applied: the build's stylesheet is served ($code)"
out="$(D pm-disable -y bootstrap_native_update 2>&1)"; has "$out" "no extensions that could be disabled"; ok $? "the module can't be disabled while the theme is enabled"

echo "== Theme changed (git pull, then caches cleared)"
echo "// bnu-test" >> "$DIR/themes/bootstrap_native/scss/_layout.scss"
D cc all >/dev/null 2>&1
hash_ok="$(EV "\$b = bootstrap_native_update_builds(); echo \$b['$B1']['meta']['theme_hash'] === bootstrap_native_update_theme_hash() ? 'yes' : 'no';")"
[ "$hash_ok" = yes ]; ok $? "the active build is rebuilt for the changed theme"
active="$(EV 'echo bootstrap_native_update_active();')"
[ "$active" = "$B1" ]; ok $? "still on $B1 after the rebuild"
# A rebuild that fails: the release's files are gone.
/bin/rm -rf "$DRUPAL_ROOT/$FILES/bootstrap-native/releases/6.0.1-test.1"
echo "// bnu-test" >> "$DIR/themes/bootstrap_native/scss/_layout.scss"
D cc all >/dev/null 2>&1
out="$(EV "echo bootstrap_native_update_active(), '|', variable_get('bootstrap_native_update_fallback') ? 'fallback' : 'none';")"
[ "$out" = "|fallback" ]; ok $? "a failed rebuild falls back to the theme's own files and says so ($out)"
status="$(D core-requirements --severity=1 2>&1)"
has "$status" "could not be rebuilt"; ok $? "the status report explains the fallback"
sed -i '/^\/\/ bnu-test$/d' "$DIR/themes/bootstrap_native/scss/_layout.scss"
page="$(curl -s "$BASE/user/login")"
has "$page" "themes/bootstrap_native/css/bootstrap_native.css"; ok $? "after the fallback pages use the shipped stylesheet"

echo "== Cleanup after 30 days"
EV "bootstrap_native_update_save_build_record('6.0.2-test.1~default', array('inactive_since' => REQUEST_TIME - 31 * 86400)); bootstrap_native_update_save_version('6.0.2-test.1', array('inactive_since' => REQUEST_TIME - 31 * 86400)); bootstrap_native_update_save_build_record('6.0.3-test.1~default', array('inactive_since' => REQUEST_TIME - 29 * 86400));" >/dev/null
D cron >/dev/null 2>&1
out="$(EV 'echo implode(",", array_keys(bootstrap_native_update_builds())), "|", implode(",", array_keys(bootstrap_native_update_versions()));')"
! has "$out" "6.0.2-test.1" && has "$out" "6.0.3-test.1~default"; ok $? "cron removes builds and releases inactive for 30+ days only ($out)"
[ ! -d "$DRUPAL_ROOT/$FILES/bootstrap-native/releases/6.0.2-test.1" ] && [ ! -d "$DRUPAL_ROOT/$FILES/bootstrap-native/builds/6.0.2-test.1~default" ]; ok $? "their files are removed too"

echo "== Choosing parts"
EV "bootstrap_native_update_activate('');" >/dev/null
out="$(EV "module_load_include('inc', 'bootstrap_native_update', 'includes/parts'); \$c = bootstrap_native_update_parts_context('$SHIPPED'); \$s = \$c['default']; \$s['parts'] = array_diff(\$s['parts'], array('accordion', 'alert')); \$s['parts'][] = 'placeholder'; \$s['parts'][] = 'carousel'; \$s['utilities'] = array_diff(\$s['utilities'], array('margin', 'padding')); \$p = bootstrap_native_update_plan_build('$SHIPPED', \$s); echo \$p['build'], '|', in_array('alert', \$p['effective']['parts']) ? 'alert kept' : 'alert gone', '|', in_array('placeholder', \$p['effective']['parts']) ? 'placeholder in' : 'placeholder out', '|', in_array('carousel', \$p['effective']['parts']) ? 'carousel in' : 'carousel out'; variable_set('bnu_test_selection', \$s);")"
has "$out" "alert kept" && has "$out" "placeholder out" && has "$out" "carousel in"; ok $? "required parts stay, clashing parts stay out, optional ones come in ($out)"
PB="${out%%|*}"
case "$PB" in "$SHIPPED~"????????) true ;; *) false ;; esac; ok $? "the shipped release with chosen parts is its own build ($PB)"
out="$(EV "\$m = bootstrap_native_update_make_build('$SHIPPED', variable_get('bnu_test_selection')); \$r = bootstrap_native_update_check_build(\$m['build']); echo \$m['build'], '|', \$m['error'] ?: 'built', '|', \$r['verdict'];")"
has "$out" "$PB|built|"; ok $? "built and checked ($out)"
css="$(cat "$DRUPAL_ROOT/$FILES/bootstrap-native/builds/$PB/bootstrap_native.css" 2>/dev/null)"
! has "$css" ".accordion" && ! has "$css" ".mt-3{" && has "$css" ".carousel" && has "$css" ".alert"; ok $? "the build leaves out accordion and spacing utilities, has carousel and alert"
size_new=${#css}; size_old=$(wc -c < "$DIR/themes/bootstrap_native/css/bootstrap_native.css")
[ "$size_new" -lt "$size_old" ]; ok $? "and is smaller ($((size_new / 1024)) KB, was $((size_old / 1024)) KB)"
EV "bootstrap_native_update_save_build_record('$PB', array('applied' => REQUEST_TIME)); bootstrap_native_update_activate('$PB');" >/dev/null
out="$(EV "\$s = variable_get('bootstrap_native_update_selection'); echo is_array(\$s) && !in_array('accordion', \$s['parts']) ? 'selection saved' : 'no selection';")"
[ "$out" = "selection saved" ]; ok $? "applying it saves the site's selection"
page="$(curl -s "$BASE/user/login")"
has "$page" "bootstrap-native/builds/$PB/bootstrap_native.css"; ok $? "pages use it"
EV "bootstrap_native_update_activate(''); variable_del('bnu_test_selection');" >/dev/null
out="$(EV "echo var_export(variable_get('bootstrap_native_update_selection'), TRUE);")"
[ "$out" = "NULL" ]; ok $? "rolling back to the theme's own files resets the selection"

echo "== Missing or damaged files"
EV "variable_del('bootstrap_native_update_fallback');" >/dev/null
check 6.0.1-test.1 >/dev/null
EV "bootstrap_native_update_save_build_record('$B1', array('applied' => REQUEST_TIME)); bootstrap_native_update_activate('$B1');" >/dev/null
BD="$DRUPAL_ROOT/$FILES/bootstrap-native/builds/$B1"
RD="$DRUPAL_ROOT/$FILES/bootstrap-native/releases/6.0.1-test.1"
intact() { EV "echo bootstrap_native_update_build_intact('$B1') ? 'intact' : 'not intact', '|', bootstrap_native_update_active();"; }
before="$(stat -c %Y "$BD/bootstrap_native.css")"
sleep 1
D cc all >/dev/null 2>&1
[ "$(stat -c %Y "$BD/bootstrap_native.css")" = "$before" ] && [ "$(intact)" = "intact|$B1" ]; ok $? "a cache clear with everything in order touches nothing"
/bin/rm -rf "$BD"
page="$(curl -s "$BASE/user/login")"
has "$page" "themes/bootstrap_native/css/bootstrap_native.css" && ! has "$page" "builds/$B1"; ok $? "build files gone: pages fall back to the theme's own files"
out="$(EV "echo var_export(variable_get('bootstrap_native_update_repair', FALSE), TRUE);")"; [ "$out" = "true" ]; ok $? "and a repair is flagged ($out)"
D cc all >/dev/null 2>&1
[ "$(intact)" = "intact|$B1" ]; ok $? "a cache clear rebuilds them; still on $B1"
page="$(curl -s "$BASE/user/login")"
has "$page" "builds/$B1/bootstrap_native.css"; ok $? "pages use the build again"
echo "/* damaged */" >> "$BD/bootstrap_native.css"
D cc all >/dev/null 2>&1
[ "$(intact)" = "intact|$B1" ]; ok $? "a damaged stylesheet (size and fingerprint differ) is rebuilt"
downloaded="$(EV "\$v = bootstrap_native_update_versions(); echo \$v['6.0.1-test.1']['downloaded'];")"
/bin/rm -rf "$BD" "$RD"
sleep 1
D cc all >/dev/null 2>&1
[ -d "$RD/bootstrap/scss" ] && [ "$(intact)" = "intact|$B1" ]; ok $? "build and release both gone: downloaded again (checksum checked) and rebuilt"
[ "$(EV "\$v = bootstrap_native_update_versions(); echo \$v['6.0.1-test.1']['downloaded'];")" = "$downloaded" ]; ok $? "the release keeps its first download date"
mv "$DRUPAL_ROOT/$FIX/bootstrap-6.0.1-test.1.tgz" "$DRUPAL_ROOT/$FIX/held.tgz"
/bin/rm -rf "$BD" "$RD"
D cc all >/dev/null 2>&1
out="$(EV "\$f = variable_get('bootstrap_native_update_fallback'); echo bootstrap_native_update_active(), '|', isset(\$f['reason']) ? \$f['reason'] : 'none';")"
[ "$out" = "|files" ]; ok $? "the download fails: the site stays on the theme's own files and says why ($out)"
status="$(D core-requirements --severity=1 2>&1)"
has "$status" "missing or damaged"; ok $? "the status report explains it"
mv "$DRUPAL_ROOT/$FIX/held.tgz" "$DRUPAL_ROOT/$FIX/bootstrap-6.0.1-test.1.tgz"
EV "variable_del('bootstrap_native_update_fallback');" >/dev/null

echo "== Sub-theme"
out="$(D bootstrap-native-subtheme Bad-Name 2>&1)"; has "$out" "machine name"; ok $? "refuses a bad machine name"
out="$(D bootstrap-native-subtheme bootstrap_native 2>&1)"; has "$out" "already the name"; ok $? "refuses an existing name"
out="$(D bootstrap-native-subtheme $SUB --path=../../etc 2>&1)"; has "$out" "sites/all/themes"; ok $? "refuses a folder outside sites/*/themes"
out="$(D bootstrap-native-subtheme $SUB --name='Test "Sub" \ Theme' --enable 2>&1)"; has "$out" "Created sites/all/themes/$SUB"; ok $? "creates sites/all/themes/$SUB"
T="$DRUPAL_ROOT/sites/all/themes/$SUB"
info="$(cat "$T/$SUB.info" 2>/dev/null)"
has "$info" 'name = "Test Sub Theme"' && has "$info" "base theme = bootstrap_native" && has "$info" "regions[sidebar_first]" && has "$info" "features[] = main_menu" && has "$info" "; bootstrap_native[parts][] = carousel"; ok $? ".info: cleaned name, base theme, regions, features, commented parts/sass/js"
[ -f "$T/template.php" ] && [ -f "$T/css/$SUB.css" ] && /usr/bin/grep -q "${SUB}_preprocess_page" "$T/template.php" && ! /usr/bin/grep -rq STARTERKIT "$T"; ok $? "files renamed, STARTERKIT replaced everywhere"
/usr/bin/grep -q "# Test Sub Theme ($SUB): notes for AI agents" "$T/AGENTS.md" && /usr/bin/grep -q "bootstrap_native/AGENTS.md" "$T/AGENTS.md" && [ "$(cat "$T/CLAUDE.md")" = "@AGENTS.md" ]; ok $? "AGENTS.md (named, pointing to the module's) and CLAUDE.md"
out="$(D ev "\$t = list_themes(TRUE); echo \$t['$SUB']->status, '|', \$t['$SUB']->info['base theme'], '|', count(\$t['$SUB']->info['regions']) == count(\$t['bootstrap_native']->info['regions']) ? 'same regions' : 'other regions';")"
[ "$out" = "1|bootstrap_native|same regions" ]; ok $? "enabled, built on Bootstrap Native, same regions ($out)"
out="$(D ev "echo json_encode(array_intersect_key(variable_get('theme_${SUB}_settings', array()), array('bootstrap_native_color_primary' => 1)) == array_intersect_key(variable_get('theme_bootstrap_native_settings', array()), array('bootstrap_native_color_primary' => 1)) ? 'same' : 'differs');")"
[ "$out" = '"same"' ]; ok $? "color settings copied from Bootstrap Native"
out="$(D bootstrap-native-subtheme $SUB 2>&1)"; has "$out" "already"; ok $? "refuses to overwrite it"
mkdir -p "$T/templates"
printf '<span class="navbar-toggler-icon"></span>\n' > "$T/templates/node--article.tpl.php"
out="$(check 6.0.2-test.1)"
has "$out" "Sub-theme: templates/node--article.tpl.php"; ok $? "the checks look at the sub-theme: its class shows as Check"

echo "== What a sub-theme adds (parts, Sass, plugins)"
mkdir -p "$T/scss" "$T/js"
printf '@use "bootstrap/scss/config" as *;\n.bnu-sub-extra { color: red; }\n' > "$T/scss/_extras.scss"
printf 'import { Tooltip } from "bootstrap";\nwindow.bnuSubPlugin = typeof Tooltip;\n' > "$T/js/plugin.js"
printf '\nbootstrap_native[parts][] = carousel\nbootstrap_native[sass][] = scss/_extras.scss\nbootstrap_native[sass][] = ../outside.scss\nbootstrap_native[js][] = js/plugin.js\n' >> "$T/$SUB.info"
D vset theme_default "$SUB" >/dev/null 2>&1
D cc all >/dev/null 2>&1
active="$(EV 'echo bootstrap_native_update_active();')"
case "$active" in "$SHIPPED~"????????) true ;; *) false ;; esac; ok $? "a sub-theme asking for more gets its own build after a cache clear ($active)"
css="$(cat "$DRUPAL_ROOT/$FILES/bootstrap-native/builds/$active/bootstrap_native.css" 2>/dev/null)"
has "$css" ".carousel" && has "$css" ".bnu-sub-extra"; ok $? "the build has the part it needs and its Sass module"
out="$(EV "\$a = bootstrap_native_update_theme_additions(); echo count(\$a['sass']);")"
[ "$out" = "1" ]; ok $? "a Sass path outside the sub-theme is ignored"
page="$(curl -s "$BASE/user/login")"
has "$page" "themes/$SUB/js/plugin.js" && has "$page" '<script type="importmap">'; ok $? "pages load the sub-theme's plugin after the import map"
out="$(PLAYWRIGHT_BROWSERS_PATH="${PLAYWRIGHT_BROWSERS_PATH:-/opt/playwright/browsers}" "${PYTHON:-/opt/playwright/bin/python}" -c '
import sys
from playwright.sync_api import sync_playwright
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(); errors = []
    pg.on("pageerror", lambda e: errors.append(str(e)))
    pg.goto(sys.argv[1] + "/user/login")
    pg.wait_for_function("window.bnuSubPlugin !== undefined", timeout=5000)
    print(pg.evaluate("window.bnuSubPlugin"), "errors:", len(errors))
    b.close()
' "$BASE" 2>&1 | tail -1)"
[ "$out" = "function errors: 0" ]; ok $? "in a browser the plugin runs and imports Bootstrap by name ($out)"
D vset theme_default "$DEFAULT" >/dev/null 2>&1
sed -i '/^bootstrap_native\[/d' "$T/$SUB.info"
D cc all >/dev/null 2>&1
active="$(EV 'echo bootstrap_native_update_active();')"
[ "$active" = "" ]; ok $? "without them, back to the theme's own files after a cache clear"

echo "== The site's own theme"
out="$(D ev '$t = list_themes(TRUE); echo (int) !empty($t["bootstrap_native"]->info["hidden"]);')"
[ "$out" = 1 ]; ok $? "Bootstrap Native is hidden (a foundation, not a site theme)"
# As an admin: the prompt is only for people who may create sub-themes.
state() { D --user=1 ev "echo bootstrap_native_update_site_theme_state(), '|', strip_tags((string) bootstrap_native_update_help('admin/appearance', array()));" 2>&1; }
D vset theme_default bootstrap_native >/dev/null 2>&1
out="$(state)"; has "$out" "base|Move to your own theme"; ok $? "on Bootstrap Native itself: Appearance offers to move to the site's own theme"
D vset theme_default bartik >/dev/null 2>&1
out="$(state)"; has "$out" "none|Create your site's theme"; ok $? "no Bootstrap Native theme yet: Appearance offers to create one"
status="$(D core-requirements --severity=1 2>&1)"; has "$status" "Site theme"; ok $? "and the status report says so"
D vset theme_default "$DEFAULT" >/dev/null 2>&1
out="$(state)"; [ "$out" = "|" ]; ok $? "with a sub-theme as the default: no prompt"
# Moving off Bootstrap Native: the new theme gets its blocks and settings,
# the base theme is disabled.
D ev "theme_enable(array('bootstrap_native'));" >/dev/null 2>&1
D vset theme_default bootstrap_native >/dev/null 2>&1
blocks="$(D ev "echo db_query('SELECT COUNT(*) FROM {block} WHERE theme = :t AND status = 1', array(':t' => 'bootstrap_native'))->fetchField();")"
D bootstrap-native-subtheme bnu_move_sub --name="Moved Site" --default >/dev/null 2>&1
out="$(D ev "\$t = list_themes(TRUE); echo variable_get('theme_default'), '|', \$t['bootstrap_native']->status, '|', db_query('SELECT COUNT(*) FROM {block} WHERE theme = :t AND status = 1', array(':t' => 'bnu_move_sub'))->fetchField();")"
[ "$out" = "bnu_move_sub|0|$blocks" ]; ok $? "moved: the new theme is the default, has the $blocks blocks, Bootstrap Native is disabled ($out)"
D vset theme_default "$DEFAULT" >/dev/null 2>&1
D ev "theme_disable(array('bnu_move_sub')); variable_del('theme_bnu_move_sub_settings'); db_delete('block')->condition('theme', 'bnu_move_sub')->execute(); db_delete('system')->condition('name', 'bnu_move_sub')->execute();" >/dev/null 2>&1
/bin/rm -rf "${DRUPAL_ROOT:?}/sites/all/themes/bnu_move_sub"

echo "== Pages (browser)"
EV "bootstrap_native_update_activate(''); foreach (array_keys(bootstrap_native_update_builds()) as \$b) bootstrap_native_update_discard_build(\$b); foreach (array_keys(bootstrap_native_update_versions()) as \$v) bootstrap_native_update_discard_release(\$v); bootstrap_native_update_refresh_releases();" >/dev/null
SHOTS="${OUT:-$(mktemp -d)}"
LOGIN="$(D uli --name="${TEST_USER:-claude}" --no-browser 2>/dev/null)" PLAYWRIGHT_BROWSERS_PATH="${PLAYWRIGHT_BROWSERS_PATH:-/opt/playwright/browsers}" \
  "${PYTHON:-/opt/playwright/bin/python}" "$DIR/tests/update_ui.py" "$SHOTS" | sed '/^all passed$/d;/ failed$/d'
ok "${PIPESTATUS[0]}" "pages: all browser checks (screenshots: $SHOTS)"

echo "== Back to the theme's own files"
cleanup
trap - EXIT
out="$(EV "echo bootstrap_native_update_active_version(), '|', count(bootstrap_native_update_versions()), '|', count(bootstrap_native_update_builds());")"
[ "$out" = "$SHIPPED|0|0" ]; ok $? "shipped release active, no test releases or builds left ($out)"

[ $failures = 0 ] && echo "all passed" || echo "$failures failed"
exit $failures
