<?php

/**
 * @file
 * An admin page: the band (View site, breadcrumb, title, local actions,
 * primary tabs), then secondary tabs, messages, help and the content.
 *
 * Variables as in core's page.tpl.php, plus (template.php):
 * - $tabs_primary, $tabs_secondary: the two rows of tabs, apart.
 *
 * Drupal's toolbar or admin_menu stay as they are, above the band.
 */
?>
<header class="bna-band" role="banner">
  <div class="bna-band-inner">
    <div class="bna-top">
      <a class="bna-view-site" href="<?php print $front_page; ?>"><span aria-hidden="true">←</span> <?php print t('View site'); ?></a>
      <?php if ($breadcrumb): ?>
        <span class="bna-sep" aria-hidden="true">|</span>
        <?php print $breadcrumb; ?>
      <?php endif; ?>
    </div>
    <div class="bna-title-row">
      <?php print render($title_prefix); ?>
      <?php if ($title): ?>
        <h1 class="bna-title" id="page-title"><?php print $title; ?></h1>
      <?php endif; ?>
      <?php print render($title_suffix); ?>
      <?php if ($action_links): ?>
        <ul class="action-links bna-actions"><?php print render($action_links); ?></ul>
      <?php endif; ?>
    </div>
    <?php print render($tabs_primary); ?>
  </div>
</header>

<main class="bna-main" id="main-content" tabindex="-1">
  <?php print render($tabs_secondary); ?>
  <?php print $messages; ?>
  <?php print render($page['help']); ?>
  <?php print render($page['content']); ?>
</main>
