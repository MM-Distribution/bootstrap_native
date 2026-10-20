/**
 * @file
 * Loads Bootstrap 6's JavaScript (an ES module, no jQuery).
 *
 * Bootstrap listens for its data attributes (data-bs-toggle and the like)
 * on the document, so they also work in content Drupal adds later by AJAX.
 * Other scripts get it as window.bootstrap after the "bs-native:ready"
 * event on document.
 *
 * Loaded as <script type="module"> (bs_native_preprocess_html()); the
 * cache-busting query of this file is passed on to Bootstrap's.
 */
const query = new URL(import.meta.url).search;
const bootstrap = await import(`./bootstrap.bundle.min.js${query}`);
window.bootstrap = bootstrap;
document.dispatchEvent(new CustomEvent('bs-native:ready', { detail: bootstrap }));
