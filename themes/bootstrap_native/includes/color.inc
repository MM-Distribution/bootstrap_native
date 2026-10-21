<?php

/**
 * @file
 * Color settings to Bootstrap tokens.
 *
 * A few colors from the theme settings (primary, secondary, base, text,
 * links) become Bootstrap's CSS variables, so the whole framework follows
 * them: buttons, checkboxes, focus rings, tinted backgrounds, borders.
 *
 * Rules (the theme is "smart" instead of asking for every shade):
 * - Brand colors used as backgrounds (buttons, badges) stay exactly as set;
 *   the text on them is black or white, whichever reads better.
 * - Any color used as text (links, body text, colored text on the page) is
 *   darkened or lightened just enough to stay readable on the base color
 *   (WCAG AA, 4.5:1). Each change is reported on the settings page.
 * - A dark base color switches the theme to its dark color scheme.
 *
 * Math in OKLCH (lightness changes keep the hue); output is plain hex.
 */

/**
 * Parses a color: hex ('#abc', '#aabbcc', with or without '#') or one of
 * Bootstrap's named colors ('orange', 'indigo-600'; see
 * bootstrap_native_color_named()).
 *
 * @return array|null
 *   array(r, g, b), each 0..1, or NULL when $value isn't a color.
 */
function bootstrap_native_color_parse($value) {
  $value = trim((string) $value);
  // A name first; "abcdef" without its "#" is still hex.
  if (preg_match('/^[a-z]+(-\d{3})?$/i', $value) && ($rgb = bootstrap_native_color_named(strtolower($value)))) {
    return $rgb;
  }
  $value = ltrim($value, '#');
  if (preg_match('/^[0-9a-f]{3}$/i', $value)) {
    $value = $value[0] . $value[0] . $value[1] . $value[1] . $value[2] . $value[2];
  }
  if (!preg_match('/^[0-9a-f]{6}$/i', $value)) {
    return NULL;
  }
  return array(
    hexdec(substr($value, 0, 2)) / 255,
    hexdec(substr($value, 2, 2)) / 255,
    hexdec(substr($value, 4, 2)) / 255,
  );
}

/**
 * Bootstrap's named colors, from a release's scss/_colors.scss.
 *
 * Its colors list ($blue: oklch(60% 0.24 240) ...) and the tint and shade
 * stops ("100": 80% toward white, "600": 16% toward black; 500 is the
 * color itself).
 *
 * @return array
 *   Keys: colors (name => array(L 0..1, C, H)), tints, shades (stop =>
 *   fraction).
 */
function bootstrap_native_color_palette_parse($scss) {
  $palette = array('colors' => array(), 'tints' => array(), 'shades' => array());
  if (preg_match('/scss-docs-start colors-list(.*?)scss-docs-end colors-list/s', $scss, $m)) {
    preg_match_all('/\$([a-z]+):\s*oklch\(\s*([\d.]+)%\s+([\d.]+)\s+([\d.]+)\s*\)/', $m[1], $colors, PREG_SET_ORDER);
    foreach ($colors as $color) {
      $palette['colors'][$color[1]] = array($color[2] / 100, (float) $color[3], (float) $color[4]);
    }
  }
  foreach (array('tints', 'shades') as $kind) {
    if (preg_match('/\$color-' . $kind . ':\s*defaults\(\s*\((.*?)\)/s', $scss, $m)) {
      preg_match_all('/"(\d{3})":\s*([\d.]+)%/', $m[1], $stops, PREG_SET_ORDER);
      foreach ($stops as $stop) {
        $palette[$kind][$stop[1]] = $stop[2] / 100;
      }
    }
  }
  return $palette;
}

/**
 * The named colors in use (set once per request from the active release).
 *
 * @param array|null $palette
 *   From bootstrap_native_color_palette_parse(), to set it.
 *
 * @return array
 *   The palette (empty lists until one is set).
 */
function bootstrap_native_color_palette($palette = NULL) {
  static $current = array('colors' => array(), 'tints' => array(), 'shades' => array());
  if ($palette !== NULL) {
    $current = $palette;
  }
  return $current;
}

/**
 * One of Bootstrap's named colors: 'orange' (= orange-500), 'indigo-600'.
 *
 * Worked out as Bootstrap does: color-mix() in OKLCH with white (tints)
 * or black (shades), which keeps the hue.
 *
 * @return array|null
 *   array(r, g, b), or NULL for a name the palette doesn't have.
 */
function bootstrap_native_color_named($name) {
  $palette = bootstrap_native_color_palette();
  if (!preg_match('/^([a-z]+)(?:-(\d{3}))?$/', $name, $m) || !isset($palette['colors'][$m[1]])) {
    return NULL;
  }
  list($l, $c, $h) = $palette['colors'][$m[1]];
  $stop = isset($m[2]) ? $m[2] : '500';
  if (isset($palette['tints'][$stop])) {
    $f = $palette['tints'][$stop];
    $l += (1 - $l) * $f;
    $c *= 1 - $f;
  }
  elseif (isset($palette['shades'][$stop])) {
    $f = $palette['shades'][$stop];
    $l *= 1 - $f;
    $c *= 1 - $f;
  }
  elseif ($stop !== '500') {
    return NULL;
  }
  // Out-of-gamut channels are clipped, as browsers draw Bootstrap's own
  // oklch() values (not the chroma reduction used for worked-out colors).
  $rad = deg2rad($h);
  return array_map(function ($channel) {
    return max(0, min(1, $channel));
  }, _bootstrap_native_color_from_oklab(array($l, $c * cos($rad), $c * sin($rad))));
}

/**
 * Whether a setting value is one of Bootstrap's color names (not hex).
 */
function bootstrap_native_color_is_name($value) {
  return (bool) preg_match('/^[a-z]+(-\d{3})?$/', (string) $value);
}

/**
 * Formats array(r, g, b) (0..1) as '#rrggbb'.
 */
function bootstrap_native_color_hex(array $rgb) {
  $hex = '#';
  foreach ($rgb as $channel) {
    $hex .= sprintf('%02x', (int) round(max(0, min(1, $channel)) * 255));
  }
  return $hex;
}

/**
 * sRGB channel (0..1) to linear light.
 */
function _bootstrap_native_color_linear($c) {
  return $c <= 0.04045 ? $c / 12.92 : pow(($c + 0.055) / 1.055, 2.4);
}

/**
 * Linear light to sRGB channel (0..1).
 */
function _bootstrap_native_color_gamma($c) {
  return $c <= 0.0031308 ? 12.92 * $c : 1.055 * pow(max(0, $c), 1 / 2.4) - 0.055;
}

/**
 * WCAG relative luminance of array(r, g, b).
 */
function bootstrap_native_color_luminance(array $rgb) {
  list($r, $g, $b) = array_map('_bootstrap_native_color_linear', $rgb);
  return 0.2126 * $r + 0.7152 * $g + 0.0722 * $b;
}

/**
 * WCAG contrast ratio of two colors (1..21).
 */
function bootstrap_native_color_contrast(array $a, array $b) {
  $la = bootstrap_native_color_luminance($a);
  $lb = bootstrap_native_color_luminance($b);
  return (max($la, $lb) + 0.05) / (min($la, $lb) + 0.05);
}

/**
 * array(r, g, b) to OKLab array(L, a, b).
 */
function bootstrap_native_color_oklab(array $rgb) {
  list($r, $g, $b) = array_map('_bootstrap_native_color_linear', $rgb);
  $l = pow(0.4122214708 * $r + 0.5363325363 * $g + 0.0514459929 * $b, 1 / 3);
  $m = pow(0.2119034982 * $r + 0.6806995451 * $g + 0.1073969566 * $b, 1 / 3);
  $s = pow(0.0883024619 * $r + 0.2817188376 * $g + 0.6299787005 * $b, 1 / 3);
  return array(
    0.2104542553 * $l + 0.7936177850 * $m - 0.0040720468 * $s,
    1.9779984951 * $l - 2.4285922050 * $m + 0.4505937099 * $s,
    0.0259040371 * $l + 0.7827717662 * $m - 0.8086757660 * $s,
  );
}

/**
 * OKLab array(L, a, b) to array(r, g, b), unclamped (may be out of gamut).
 */
function _bootstrap_native_color_from_oklab(array $lab) {
  list($L, $a, $b) = $lab;
  $l = pow($L + 0.3963377774 * $a + 0.2158037573 * $b, 3);
  $m = pow($L - 0.1055613458 * $a - 0.0638541728 * $b, 3);
  $s = pow($L - 0.0894841775 * $a - 1.2914855480 * $b, 3);
  return array_map('_bootstrap_native_color_gamma', array(
    4.0767416621 * $l - 3.3077115913 * $m + 0.2309699292 * $s,
    -1.2684380046 * $l + 2.6097574011 * $m - 0.3413193965 * $s,
    -0.0041960863 * $l - 0.7034186147 * $m + 1.7076147010 * $s,
  ));
}

/**
 * array(r, g, b) to OKLCH array(L, C, h) (h in degrees).
 */
function bootstrap_native_color_oklch(array $rgb) {
  list($L, $a, $b) = bootstrap_native_color_oklab($rgb);
  $h = rad2deg(atan2($b, $a));
  return array($L, sqrt($a * $a + $b * $b), $h < 0 ? $h + 360 : $h);
}

/**
 * OKLCH array(L, C, h) to array(r, g, b), lowering chroma to fit sRGB.
 */
function bootstrap_native_color_from_oklch(array $lch) {
  list($L, $C, $h) = $lch;
  $L = max(0, min(1, $L));
  $rad = deg2rad($h);
  $in = function ($c) use ($L, $rad) {
    $rgb = _bootstrap_native_color_from_oklab(array($L, $c * cos($rad), $c * sin($rad)));
    foreach ($rgb as $channel) {
      if ($channel < -0.0005 || $channel > 1.0005) {
        return FALSE;
      }
    }
    return $rgb;
  };
  if ($rgb = $in($C)) {
    return $rgb;
  }
  // Out of gamut: the most chroma that fits, same lightness and hue.
  $low = 0;
  $high = $C;
  $rgb = $in(0);
  for ($i = 0; $i < 24; $i++) {
    $mid = ($low + $high) / 2;
    if ($try = $in($mid)) {
      $low = $mid;
      $rgb = $try;
    }
    else {
      $high = $mid;
    }
  }
  return $rgb;
}

/**
 * Mixes two colors in OKLab; $amount is how much of $b (0..1).
 */
function bootstrap_native_color_mix(array $a, array $b, $amount) {
  $la = bootstrap_native_color_oklab($a);
  $lb = bootstrap_native_color_oklab($b);
  $mix = array();
  foreach ($la as $i => $value) {
    $mix[$i] = $value + ($lb[$i] - $value) * $amount;
  }
  return array_map(function ($c) {
    return max(0, min(1, $c));
  }, _bootstrap_native_color_from_oklab($mix));
}

/**
 * Text color on a background: white when it reads well, else black.
 *
 * White wins near the line (Bootstrap's own blue, #0d6efd, is 4.5:1 with
 * white and slightly more with black; brand buttons usually carry white).
 */
function bootstrap_native_color_on(array $bg) {
  $white = array(1, 1, 1);
  $black = array(0, 0, 0);
  $on_white = bootstrap_native_color_contrast($white, $bg);
  return $on_white >= 4.4 || $on_white >= bootstrap_native_color_contrast($black, $bg) ? $white : $black;
}

/**
 * $fg, darkened or lightened just enough to reach $ratio on $bg.
 *
 * Only the lightness changes (hue kept). When even black or white can't
 * reach $ratio, the best of the two is returned.
 */
function bootstrap_native_color_readable(array $fg, array $bg, $ratio = 4.5) {
  if (bootstrap_native_color_contrast($fg, $bg) >= $ratio) {
    return $fg;
  }
  // A little margin: the result is rounded to a hex color afterwards.
  $ratio += 0.03;
  list($L, $C, $h) = bootstrap_native_color_oklch($fg);
  // Away from the background: darker on light backgrounds, lighter on dark.
  $target = bootstrap_native_color_luminance($bg) > 0.18 ? 0 : 1;
  if (bootstrap_native_color_contrast(bootstrap_native_color_from_oklch(array($target, $C, $h)), $bg) < $ratio) {
    return bootstrap_native_color_on($bg);
  }
  // The closest lightness to the original that passes.
  $near = $L;
  $far = $target;
  for ($i = 0; $i < 24; $i++) {
    $mid = ($near + $far) / 2;
    if (bootstrap_native_color_contrast(bootstrap_native_color_from_oklch(array($mid, $C, $h)), $bg) >= $ratio) {
      $far = $mid;
    }
    else {
      $near = $mid;
    }
  }
  return bootstrap_native_color_from_oklch(array($far, $C, $h));
}

/**
 * $color with its OKLCH lightness moved by $amount (negative = darker).
 */
function bootstrap_native_color_shift(array $color, $amount) {
  list($L, $C, $h) = bootstrap_native_color_oklch($color);
  return bootstrap_native_color_from_oklch(array($L + $amount, $C, $h));
}

/**
 * The automatic link hover color: a clear step from the link.
 *
 * Further from the background (darker on a light one, lighter on a dark
 * one) by 0.18 in OKLCH lightness, enough to see; a link already close to
 * black (or white) steps the other way instead, kept readable.
 */
function bootstrap_native_color_hover(array $link, array $base, $dark) {
  list($l) = bootstrap_native_color_oklch($link);
  $step = $dark ? 0.18 : -0.18;
  if ($l + $step < 0.12 || $l + $step > 0.97) {
    $step = -$step;
  }
  return bootstrap_native_color_readable(bootstrap_native_color_shift($link, $step), $base, 4.5);
}

/**
 * Whether a background color calls for the dark color scheme.
 */
function bootstrap_native_color_is_dark(array $bg) {
  // Where black and white text read equally well (about mid gray).
  return bootstrap_native_color_luminance($bg) < 0.18;
}

/**
 * The theme's color settings as Bootstrap CSS variables.
 *
 * @param array $settings
 *   Keys primary, secondary, base, text, link, link_hover: hex colors, or
 *   empty for Bootstrap's own.
 *
 * @return array
 *   - vars: CSS variable => hex value, only for what the settings change.
 *   - scheme: 'light' or 'dark'.
 *   - notes: array of setting => array('from' => hex, 'to' => hex,
 *     'reason' => text) for colors changed to stay readable.
 */
function bootstrap_native_color_tokens(array $settings) {
  $color = array();
  foreach (array('primary', 'secondary', 'base', 'text', 'link', 'link_hover') as $key) {
    $color[$key] = isset($settings[$key]) ? bootstrap_native_color_parse($settings[$key]) : NULL;
  }
  $vars = array();
  $notes = array();
  $hex = 'bootstrap_native_color_hex';
  // Records a color changed for readability.
  $readable = function ($key, array $fg, array $bg, $ratio) use (&$notes, $hex) {
    $out = bootstrap_native_color_readable($fg, $bg, $ratio);
    if ($key && $hex($out) != $hex($fg)) {
      $notes[$key] = array(
        'from' => $hex($fg),
        'to' => $hex($out),
        'ratio' => $ratio,
      );
    }
    return $out;
  };

  // Base and text. Without a base color, white (Bootstrap's light body).
  $base = $color['base'] ? $color['base'] : array(1, 1, 1);
  $dark = bootstrap_native_color_is_dark($base);
  if ($color['text']) {
    $text = $readable('text', $color['text'], $base, 4.5);
  }
  else {
    // Near black or near white with a hint of the base color's hue.
    list(, $c, $h) = bootstrap_native_color_oklch($base);
    $text = bootstrap_native_color_readable(bootstrap_native_color_from_oklch(array($dark ? 0.96 : 0.22, min($c, 0.02), $h)), $base, 7);
  }
  if ($color['base'] || $color['text']) {
    $vars['--bs-bg-body'] = $hex($base);
    $vars['--bs-fg-body'] = $hex($text);
    // Steps between the base and the text color: panels, wells, borders.
    foreach (array(1 => 0.025, 2 => 0.05, 3 => 0.085, 4 => 0.13) as $step => $amount) {
      $vars['--bs-bg-' . $step] = $hex(bootstrap_native_color_mix($base, $text, $amount));
    }
    foreach (array(1 => array(0.12, 7), 2 => array(0.24, 4.5), 3 => array(0.36, 4.5), 4 => array(0.5, 3)) as $step => $info) {
      $vars['--bs-fg-' . $step] = $hex(bootstrap_native_color_readable(bootstrap_native_color_mix($text, $base, $info[0]), $base, $info[1]));
    }
    $vars['--bs-border-color'] = $hex(bootstrap_native_color_mix($base, $text, 0.16));
  }

  // Brand colors: Bootstrap's "primary" and "accent" token families.
  foreach (array('primary' => 'primary', 'secondary' => 'accent') as $key => $family) {
    if (!$color[$key]) {
      continue;
    }
    $brand = $color[$key];
    $p = '--bs-' . $family . '-';
    $vars[$p . 'base'] = $hex($brand);
    $vars[$p . 'bg'] = $hex($brand);
    $vars[$p . 'contrast'] = $hex(bootstrap_native_color_on($brand));
    $vars[$p . 'fg'] = $hex($readable($key, $brand, $base, 4.5));
    $vars[$p . 'fg-emphasis'] = $hex(bootstrap_native_color_readable(bootstrap_native_color_mix($brand, $text, 0.35), $base, 7));
    $vars[$p . 'bg-subtle'] = $hex(bootstrap_native_color_mix($base, $brand, 0.12));
    $vars[$p . 'bg-muted'] = $hex(bootstrap_native_color_mix($base, $brand, 0.22));
    $vars[$p . 'border'] = $hex(bootstrap_native_color_mix($base, $brand, 0.45));
    $vars[$p . 'focus-ring'] = $hex(bootstrap_native_color_mix($base, $brand, 0.5));
  }

  // Links: their own color, else the primary text color, else Bootstrap's.
  $link = NULL;
  if ($color['link']) {
    $link = $readable('link', $color['link'], $base, 4.5);
  }
  elseif ($color['primary']) {
    $link = bootstrap_native_color_readable($color['primary'], $base, 4.5);
  }
  if ($link) {
    $vars['--bs-link-color'] = $hex($link);
  }
  if ($color['link_hover']) {
    $vars['--bs-link-hover-color'] = $hex($readable('link_hover', $color['link_hover'], $base, 4.5));
  }
  elseif ($link) {
    $vars['--bs-link-hover-color'] = $hex(bootstrap_native_color_hover($link, $base, $dark));
  }

  return array(
    'vars' => $vars,
    'scheme' => $dark ? 'dark' : 'light',
    'notes' => $notes,
  );
}

/**
 * The CSS for bootstrap_native_color_tokens() output: one :root rule.
 */
function bootstrap_native_color_css(array $tokens) {
  $css = 'color-scheme:' . $tokens['scheme'] . ';';
  foreach ($tokens['vars'] as $name => $value) {
    $css .= $name . ':' . $value . ';';
  }
  return ':root{' . $css . '}';
}

/**
 * Sets the palette from the release the site uses (Drupal only).
 *
 * The release's scss/_colors.scss: the theme's own copy, or the release a
 * build on the Bootstrap updates page was made from. Cached per file.
 */
function bootstrap_native_color_load_palette() {
  static $loaded = FALSE;
  if ($loaded || !function_exists('cache_get')) {
    return;
  }
  $loaded = TRUE;
  $file = function_exists('bootstrap_native_update_palette_file') ? bootstrap_native_update_palette_file() : '';
  if (!$file || !is_file($file)) {
    $file = dirname(dirname(__FILE__)) . '/vendor/bootstrap/scss/_colors.scss';
  }
  $cid = 'bootstrap_native:palette:' . md5($file . ':' . @filemtime($file));
  if ($cache = cache_get($cid)) {
    bootstrap_native_color_palette($cache->data);
    return;
  }
  $palette = bootstrap_native_color_palette_parse((string) @file_get_contents($file));
  cache_set($cid, $palette);
  bootstrap_native_color_palette($palette);
}
