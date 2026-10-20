<?php

/**
 * @file
 * The page: navbar header, content with optional sidebars, footer.
 *
 * Variables as in core's page.tpl.php, plus (bs_native_preprocess_page()):
 * - $main_menu_nav, $secondary_menu_nav: the menus as navbar links.
 * - $has_navbar_drawer: whether there is anything for the navbar's menu
 *   (it becomes a drawer on narrow screens, inline from "lg" up).
 *
 * The columns follow the sidebars that have blocks (scss/_layout.scss).
 */
?>
<header class="site-header" role="banner">
  <nav class="navbar lg:navbar-expand" aria-label="<?php print t('Site'); ?>">
    <div class="container">
      <?php if ($logo || $site_name): ?>
        <a class="navbar-brand" href="<?php print $front_page; ?>" rel="home">
          <?php if ($logo): ?>
            <img src="<?php print $logo; ?>" alt="<?php print $site_name ? '' : t('Home'); ?>" />
          <?php endif; ?>
          <?php if ($site_name): ?>
            <span class="site-name"><?php print $site_name; ?></span>
          <?php endif; ?>
        </a>
      <?php endif; ?>
      <?php if ($site_slogan): ?>
        <span class="navbar-text site-slogan"><?php print $site_slogan; ?></span>
      <?php endif; ?>

      <?php if ($has_navbar_drawer): ?>
        <button class="btn-icon navbar-toggler" type="button" data-bs-toggle="drawer" data-bs-target="#site-navigation" aria-controls="site-navigation" aria-expanded="false" aria-label="<?php print t('Menu'); ?>">
          <span class="navbar-toggler-icon" aria-hidden="true"></span>
        </button>
        <dialog class="drawer drawer-end" tabindex="-1" id="site-navigation" aria-labelledby="site-navigation-title">
          <div class="drawer-header">
            <h2 class="drawer-title h5" id="site-navigation-title"><?php print t('Menu'); ?></h2>
            <button type="button" class="btn-close" data-bs-dismiss="drawer" aria-label="<?php print t('Close'); ?>"></button>
          </div>
          <div class="drawer-body">
            <?php print $main_menu_nav; ?>
            <?php print $secondary_menu_nav; ?>
            <?php print render($page['navigation']); ?>
          </div>
        </dialog>
      <?php endif; ?>
    </div>
  </nav>
  <?php if ($page['header']): ?>
    <div class="container site-header-region"><?php print render($page['header']); ?></div>
  <?php endif; ?>
</header>

<div class="site-main">
  <div class="container">
    <?php print render($page['highlighted']); ?>
    <?php print $breadcrumb; ?>

    <div class="page-columns">
      <main class="page-content" id="main-content" tabindex="-1">
        <?php print render($title_prefix); ?>
        <?php if ($title): ?>
          <h1 class="page-title" id="page-title"><?php print $title; ?></h1>
        <?php endif; ?>
        <?php print render($title_suffix); ?>
        <?php print $messages; ?>
        <?php if ($tabs): ?>
          <?php print render($tabs); ?>
        <?php endif; ?>
        <?php print render($page['help']); ?>
        <?php if ($action_links): ?>
          <ul class="action-links"><?php print render($action_links); ?></ul>
        <?php endif; ?>
        <?php print render($page['content']); ?>
        <?php print $feed_icons; ?>
      </main>

      <?php if ($page['sidebar_first']): ?>
        <aside class="page-sidebar-first" role="complementary">
          <?php print render($page['sidebar_first']); ?>
        </aside>
      <?php endif; ?>

      <?php if ($page['sidebar_second']): ?>
        <aside class="page-sidebar-second" role="complementary">
          <?php print render($page['sidebar_second']); ?>
        </aside>
      <?php endif; ?>
    </div>
  </div>
</div>

<?php if ($page['footer']): ?>
  <footer class="site-footer" role="contentinfo">
    <div class="container"><?php print render($page['footer']); ?></div>
  </footer>
<?php endif; ?>
