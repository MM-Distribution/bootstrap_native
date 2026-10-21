/**
 * @file
 * Bootstrap Native Admin: on a phone, filters (Views' exposed filters,
 * core's "Show only items where") fold up behind "Filters", which says
 * what they are set to.
 */
(function ($) {
  'use strict';

  // What a filter is set to: chosen options and filled-in text, else "Any".
  function state(box) {
    var parts = [];
    $(box).find('select').each(function () {
      if (this.selectedIndex > 0 && this.options[this.selectedIndex]) {
        parts.push($.trim(this.options[this.selectedIndex].text));
      }
    });
    $(box).find('input[type="text"], input[type="search"]').each(function () {
      if ($.trim(this.value) !== '') {
        parts.push('"' + $.trim(this.value) + '"');
      }
    });
    return parts.length ? parts.join(' · ') : Drupal.t('Any');
  }

  Drupal.behaviors.bootstrapNativeAdminFilters = {
    attach: function (context) {
      if (!window.matchMedia || !window.matchMedia('(max-width: 47.99rem)').matches) {
        return;
      }
      $('.bna-main .views-exposed-form, .bna-main fieldset#edit-filters', context).once('bna-filters', function () {
        var details = document.createElement('details');
        var summary = document.createElement('summary');
        details.className = 'bna-filters';
        summary.innerHTML = '<span>' + Drupal.t('Filters') + '</span><span class="bna-filters-state"></span>';
        $(summary).find('.bna-filters-state').text(state(this));
        details.appendChild(summary);
        this.parentNode.insertBefore(details, this);
        details.appendChild(this);
      });
    }
  };
})(jQuery);
