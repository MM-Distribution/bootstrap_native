/**
 * @file
 * Theme settings: a color picker beside each hex field.
 *
 * The hex field is what gets saved (empty = Bootstrap's own color); the
 * picker only fills it in. A change updates the preview (the field's AJAX,
 * on "change"). "Clear" empties the field.
 *
 * Written for core's jQuery 1.4.4 too (bind, attr; no on, prop).
 */
(function ($) {
  'use strict';

  Drupal.behaviors.bootstrapNativeColors = {
    attach: function (context) {
      $('input.bootstrap-native-color', context).once('bootstrap-native-color', function () {
        var $text = $(this);
        var $picker = $('<input type="color" class="bootstrap-native-picker" />');
        var $clear = $('<button type="button" class="bootstrap-native-clear"></button>').text(Drupal.t('Clear'));
        $picker.attr('aria-label', Drupal.t('Pick @name', { '@name': $.trim($('label[for="' + this.id + '"]').text()) }));

        function sync() {
          var value = $.trim($text.val());
          var hex = /^#?[0-9a-f]{6}$/i.test(value) ? value.replace(/^#?/, '#') : '';
          $picker.val(hex || $text.attr('data-default'));
          $picker.toggleClass('is-default', !hex);
          if (value) {
            $clear.removeAttr('hidden');
          }
          else {
            $clear.attr('hidden', 'hidden');
          }
        }

        $picker.bind('input', function () {
          $text.val(this.value);
          $picker.removeClass('is-default');
          $clear.removeAttr('hidden');
        }).bind('change', function () {
          $text.trigger('change');
        });
        $clear.bind('click', function () {
          $text.val('').trigger('change');
          sync();
          $text.focus();
        });
        $text.bind('input change', sync);

        $text.before($picker).after($clear);
        sync();
      });
    }
  };
})(jQuery);
