<?php

/**
 * @file
 * The HTML document (core's html.tpl.php as HTML5).
 *
 * Differences from core:
 * - $html_attributes: lang, dir and data-bs-theme (Bootstrap's light or
 *   dark scheme, from the base color setting; see bootstrap_native_preprocess_html()).
 * - HTML5 doctype; RDF namespaces kept for modules that read them.
 */
?><!DOCTYPE html>
<html<?php print $html_attributes; ?><?php print $rdf_namespaces; ?>>
<head>
  <?php print $head; ?>
  <title><?php print $head_title; ?></title>
  <?php print $styles; ?>
  <?php print $scripts; ?>
</head>
<body class="<?php print $classes; ?>" <?php print $attributes; ?>>
  <div id="skip-link">
    <a href="#main-content" class="element-invisible element-focusable"><?php print t('Skip to main content'); ?></a>
  </div>
  <?php print $page_top; ?>
  <?php print $page; ?>
  <?php print $page_bottom; ?>
</body>
</html>
