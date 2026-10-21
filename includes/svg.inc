<?php

/**
 * @file
 * Cleans SVG files before Drupal keeps them (the theme's logo and browser
 * icon uploads).
 *
 * An SVG is a document, not only a picture: opened by its own address it
 * can run scripts on the site. Only drawing remains: SVG elements on a
 * list, no event attributes, links only within the file (or embedded
 * PNG/JPG/GIF/WebP pictures), no styles that load anything. Files with
 * entity declarations are refused outright.
 *
 * Plain PHP (DOM extension), no Drupal: tests/svg_test.php runs it alone.
 */

define('BOOTSTRAP_NATIVE_UPDATE_SVG_NS', 'http://www.w3.org/2000/svg');
define('BOOTSTRAP_NATIVE_UPDATE_SVG_MAX_BYTES', 2 * 1024 * 1024);

/**
 * The SVG elements a cleaned file keeps (lowercase); others go, with
 * everything inside them.
 */
function bootstrap_native_update_svg_elements() {
  $names = 'svg g defs symbol use title desc switch path rect circle ellipse line polyline polygon '
    . 'text tspan textpath lineargradient radialgradient stop clippath mask pattern marker image style '
    . 'filter feblend fecolormatrix fecomponenttransfer fecomposite feconvolvematrix fediffuselighting '
    . 'fedisplacementmap fedistantlight fedropshadow feflood fefunca fefuncb fefuncg fefuncr '
    . 'fegaussianblur femerge femergenode femorphology feoffset fepointlight fespecularlighting '
    . 'fespotlight fetile feturbulence';
  return array_flip(explode(' ', $names));
}

/**
 * Cleans an SVG file's text.
 *
 * @param string $data
 *   The file as uploaded.
 *
 * @return string|false
 *   The cleaned file, or FALSE if it isn't an SVG that can be read safely.
 */
function bootstrap_native_update_svg_clean($data) {
  if (!is_string($data) || $data === '' || strlen($data) > BOOTSTRAP_NATIVE_UPDATE_SVG_MAX_BYTES || !class_exists('DOMDocument')) {
    return FALSE;
  }
  // Entities can pull in other files or blow up in size.
  if (stripos($data, '<!ENTITY') !== FALSE) {
    return FALSE;
  }
  $doc = new DOMDocument();
  $errors = libxml_use_internal_errors(TRUE);
  $loaded = $doc->loadXML($data, LIBXML_NONET | LIBXML_COMPACT);
  libxml_clear_errors();
  libxml_use_internal_errors($errors);
  $root = $loaded ? $doc->documentElement : NULL;
  if (!$root || strtolower($root->localName) != 'svg' || $root->namespaceURI != BOOTSTRAP_NATIVE_UPDATE_SVG_NS) {
    return FALSE;
  }
  bootstrap_native_update_svg_clean_node($root, bootstrap_native_update_svg_elements());
  return '<?xml version="1.0" encoding="UTF-8"?>' . "\n" . $doc->saveXML($root) . "\n";
}

/**
 * Cleans an element and what is inside it.
 */
function bootstrap_native_update_svg_clean_node(DOMElement $element, array $allowed) {
  foreach (iterator_to_array($element->attributes) as $attribute) {
    if (!bootstrap_native_update_svg_attribute_ok($element, $attribute)) {
      $element->removeAttributeNode($attribute);
    }
    elseif ($attribute->localName == 'style') {
      $attribute->value = bootstrap_native_update_svg_clean_css($attribute->value);
    }
  }
  foreach (iterator_to_array($element->childNodes) as $child) {
    if ($child instanceof DOMElement) {
      if ($child->namespaceURI == BOOTSTRAP_NATIVE_UPDATE_SVG_NS && isset($allowed[strtolower($child->localName)])) {
        bootstrap_native_update_svg_clean_node($child, $allowed);
      }
      else {
        $element->removeChild($child);
      }
    }
    elseif ($child instanceof DOMText) {
      // Text, and CDATA (<style> rules).
      if (strtolower($element->localName) == 'style') {
        $child->data = bootstrap_native_update_svg_clean_css($child->data);
      }
    }
    else {
      // Comments, processing instructions.
      $element->removeChild($child);
    }
  }
}

/**
 * Whether an attribute may stay.
 */
function bootstrap_native_update_svg_attribute_ok(DOMElement $element, DOMAttr $attribute) {
  $name = strtolower($attribute->localName);
  $ns = $attribute->namespaceURI;
  if (strpos($name, 'on') === 0) {
    return FALSE;
  }
  if ($name == 'href') {
    if ($ns && $ns != 'http://www.w3.org/1999/xlink') {
      return FALSE;
    }
    $value = trim($attribute->value);
    if ($value !== '' && $value[0] == '#') {
      return TRUE;
    }
    return strtolower($element->localName) == 'image' && preg_match('#^data:image/(png|jpe?g|gif|webp);base64,[a-z0-9+/=\s]+$#i', $value);
  }
  // Inkscape, Illustrator and other editors' own attributes.
  if ($ns && $ns != 'http://www.w3.org/XML/1998/namespace') {
    return FALSE;
  }
  // Anything that reads as a script address.
  return !preg_match('/^\s*(javascript|vbscript|data)\s*:/i', $attribute->value);
}

/**
 * Styles without anything that loads or runs: no @import, url() only to
 * the file's own parts (#id), no old script hooks.
 */
function bootstrap_native_update_svg_clean_css($css) {
  // CSS escapes (u\72l) could spell what the rules below remove: read
  // them as the characters they stand for first.
  $css = preg_replace_callback('/\\\\([0-9a-f]{1,6})\s?/i', function ($m) {
    return html_entity_decode('&#x' . $m[1] . ';', ENT_QUOTES, 'UTF-8');
  }, $css);
  $css = preg_replace('/\\\\(.)/s', '$1', $css);
  $css = preg_replace('/@import[^;]*;?/i', '', $css);
  $css = preg_replace('/url\(\s*(?![\'"]?#)[^)]*\)/i', 'none', $css);
  $css = preg_replace('/expression\s*\(|-moz-binding|behavior\s*:|javascript\s*:/i', '', $css);
  return $css;
}
