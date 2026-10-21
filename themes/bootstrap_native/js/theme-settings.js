/**
 * @file
 * Theme settings: color fields (picker, Set / Automatic), the preview's
 * sample switch and phone view, the save bar (unsaved changes, Discard
 * changes, Reset with Undo; Reset and Discard act in the page only).
 *
 * The text field is what gets saved (a hex value, a Bootstrap color name,
 * or empty = automatic); the picker only fills it in. A change updates the
 * preview, the notes and the step summaries (the field's AJAX, on
 * "change"). Each note carries the hex the theme uses (data-hex).
 *
 * Written for core's jQuery 1.4.4 too (bind, attr; no on, prop).
 */
(function ($) {
  'use strict';

  var view = 'page';
  var dirty = false;
  var submitting = false;
  // The fields as saved (when the page loaded), and before the last Reset.
  var saved = null;
  var beforeReset = null;
  // One column (the preview behind a button) when the form is narrow: the
  // stylesheet's container query; here, the same width of the form.
  function isNarrow(form) {
    return form.offsetWidth > 0 && form.offsetWidth < 46 * parseFloat(getComputedStyle(document.documentElement).fontSize);
  }

  // The preview frame: the chosen sample, its height, and clicks that stay
  // inside (its links are only for show; "#" would load this page in it).
  function setupFrame(frame) {
    var doc = frame.contentDocument;
    if (!doc || !doc.documentElement) {
      return;
    }
    doc.documentElement.setAttribute('data-preview', view);
    frame.style.height = doc.documentElement.scrollHeight + 'px';
    if (!doc.bnBound) {
      doc.bnBound = true;
      doc.addEventListener('click', function (e) {
        if (e.target.closest && e.target.closest('a, button, input[type=submit], label')) {
          e.preventDefault();
        }
      });
      doc.addEventListener('submit', function (e) {
        e.preventDefault();
      });
    }
  }

  // The step fields that are saved (not the file uploads, nor the color
  // pickers: they only follow the text fields).
  function fields() {
    return $('form.bn-settings-form .bn-steps').find('input, select, textarea').not('[type=file], [type=hidden], [type=button], [type=submit], .bn-picker');
  }
  function checkable(el) {
    return el.type === 'checkbox' || el.type === 'radio';
  }
  function snapshot($els) {
    var list = [];
    $els.each(function () {
      list.push([this, checkable(this) ? this.checked : $(this).val()]);
    });
    return list;
  }
  function differs(list) {
    for (var i = 0; i < list.length; i++) {
      if ((checkable(list[i][0]) ? list[i][0].checked : $(list[i][0]).val()) !== list[i][1]) {
        return true;
      }
    }
    return false;
  }
  // Puts fields back, then updates the color rows and the preview (one
  // color field's AJAX sends the whole form).
  function restore(list) {
    var $colors = $([]);
    $.each(list, function (i, item) {
      if (checkable(item[0])) {
        item[0].checked = item[1];
      }
      else {
        $(item[0]).val(item[1]);
      }
      if ($(item[0]).hasClass('bootstrap-native-color')) {
        $colors = $colors.add(item[0]);
      }
    });
    refreshColors($colors);
  }
  function refreshColors($colors) {
    $colors.each(function () {
      $(this).closest('.bn-color').removeClass('is-editing');
      var sync = $(this).data('bnSync');
      if (sync) {
        sync();
      }
    });
    $colors.eq(0).trigger('change');
  }

  // Unsaved changes: anything different from what's saved (a change undone
  // by hand counts as saved again).
  function updateState() {
    if (!saved) {
      return;
    }
    dirty = differs(saved) || $('form.bn-settings-form .bn-steps input[type=file]').filter(function () {
      return this.value !== '';
    }).length > 0;
    $('.bn-unsaved').each(function () {
      $(this).text($(this).attr(dirty ? 'data-unsaved' : 'data-saved')).toggleClass('is-unsaved', dirty);
    });
    // Right after a Reset its own line and Undo say it all.
    if (beforeReset) {
      $('.bn-unsaved').attr('hidden', 'hidden');
    }
    else {
      $('.bn-unsaved').removeAttr('hidden');
    }
    if (dirty && !beforeReset) {
      $('.bn-discard').removeAttr('hidden');
    }
    else {
      $('.bn-discard').attr('hidden', 'hidden');
    }
  }
  function markDirty() {
    // After the value that changed is in place.
    setTimeout(updateState, 0);
  }

  function showReset(done) {
    if (done) {
      $('.bn-reset-status').text($('.bn-reset-status').attr('data-text'));
      $('.bn-undo').removeAttr('hidden');
      $('input.bn-reset').attr('hidden', 'hidden');
    }
    else {
      $('.bn-reset-status').text('');
      $('.bn-undo').attr('hidden', 'hidden');
      $('input.bn-reset').removeAttr('hidden');
    }
  }

  // A color field: the row with picker, field and the Set / Automatic button.
  function setupColor(input) {
    var $input = $(input);
    var $wrap = $input.closest('.bn-color');
    var key = $input.attr('data-key');
    var $row = $('<div class="bn-color-row"></div>');
    var $picker = $('<input type="color" class="bn-picker" />');
    var $swatch = $('<span class="bn-auto-swatch" aria-hidden="true"></span>');
    var $auto = $('<span class="bn-auto-label"></span>');
    var $mode = $('<button type="button" class="bn-mode"></button>');
    var label = $.trim($('label[for="' + input.id + '"]').text());
    $picker.attr('aria-label', Drupal.t('Pick @name', { '@name': label }));
    $input.before($row);
    $row.append($swatch).append($picker).append($input).append($auto).append($mode);

    function note() {
      return $('#bn-note-' + key);
    }
    function sync() {
      var $note = note();
      var hex = $note.attr('data-hex') || '';
      var empty = $.trim($input.val()) === '';
      $wrap.attr('data-mode', empty && !$wrap.hasClass('is-editing') ? 'auto' : 'set');
      $wrap.attr('data-state', $note.attr('data-state') || 'auto');
      if (/^#[0-9a-f]{6}$/i.test(hex)) {
        $picker.val(hex);
        $swatch.css('background', hex);
      }
      $auto.html($note.attr('data-state') === 'auto' ? $note.html() : '');
      $mode.text(empty && !$wrap.hasClass('is-editing') ? Drupal.t('Set') : Drupal.t('Automatic'));
      $mode.attr('aria-label', empty && !$wrap.hasClass('is-editing') ? Drupal.t('Set @name', { '@name': label }) : Drupal.t('@name back to automatic', { '@name': label }));
    }

    $picker.bind('input', function () {
      $input.val(this.value);
      markDirty();
    }).bind('change', function () {
      $input.trigger('change');
    });
    $input.bind('input', markDirty).bind('change', function () {
      $wrap.removeClass('is-editing');
    });
    $mode.bind('click', function () {
      if ($wrap.attr('data-mode') === 'auto') {
        // Start from what the theme uses now.
        $wrap.addClass('is-editing');
        $input.val(note().attr('data-hex') || '');
        sync();
        $input.focus().select();
        markDirty();
      }
      else {
        $wrap.removeClass('is-editing');
        $input.val('').trigger('change');
        markDirty();
        sync();
        $mode.focus();
      }
    });
    $input.data('bnSync', sync);
    sync();
  }

  Drupal.behaviors.bootstrapNativeSettings = {
    attach: function (context) {
      $('input.bootstrap-native-color', context).once('bn-color', function () {
        setupColor(this);
      });
      // A note replaced by the preview's AJAX: its field takes the new hex.
      $('.bn-color-note', context).each(function () {
        var sync = $('input.bootstrap-native-color[data-key="' + this.id.replace('bn-note-', '') + '"]').data('bnSync');
        if (sync) {
          sync();
        }
      });

      $('.bootstrap-native-preview iframe', context).once('bn-frame', function () {
        var frame = this;
        if (frame.contentDocument && frame.contentDocument.readyState === 'complete' && frame.contentDocument.body && frame.contentDocument.body.firstChild) {
          setupFrame(frame);
        }
        $(frame).bind('load', function () {
          setupFrame(frame);
        });
      });

      $('.bn-views', context).once('bn-views', function () {
        view = $(this).find('.bn-view[aria-pressed="true"]').attr('data-view') || 'page';
        $(this).find('.bn-view').bind('click', function () {
          view = $(this).attr('data-view');
          // Sent with each preview update, so the new frame starts on it.
          $('input.bn-preview-view').val(view);
          $('.bn-view').attr('aria-pressed', 'false');
          $(this).attr('aria-pressed', 'true');
          $('.bootstrap-native-preview iframe').each(function () {
            setupFrame(this);
          });
        });
      });

      // Phones: the preview opens over the page.
      $('.bn-preview-open', context).once('bn-open', function () {
        $(this).bind('click', function () {
          var $pane = $('.bn-preview-pane').addClass('is-open');
          $pane.find('iframe').each(function () {
            setupFrame(this);
          });
          $pane.find('.bn-preview-close').focus();
        });
      });
      $('.bn-preview-close', context).once('bn-close', function () {
        $(this).bind('click', function () {
          $('.bn-preview-pane').removeClass('is-open');
          $('.bn-preview-open').focus();
        });
      });

      $('form.bn-settings-form', context).once('bn-form', function () {
        var $form = $(this);
        // On a phone the steps start closed, but the first.
        if (isNarrow(this)) {
          $form.find('details.bn-details').each(function () {
            if (!this.hasAttribute('data-first')) {
              this.removeAttribute('open');
            }
          });
        }
        $form.find('.bn-steps').find('input, select, textarea').not('.bootstrap-native-color').bind('change', markDirty);
        saved = snapshot(fields());
        updateState();
        $form.bind('submit', function () {
          submitting = true;
          // Drupal's AJAX disables a field while its preview update runs,
          // and a disabled field isn't sent: typing a color and pressing
          // Save at once would save it empty.
          $(this).find('input.bootstrap-native-color').removeAttr('disabled').removeClass('progress-disabled');
        });
        // Reset: empties the six colors in the page; Save keeps it, Undo
        // (or Discard changes, or leaving) puts them back.
        $form.find('input.bn-reset').bind('click', function (e) {
          e.preventDefault();
          var $colors = $form.find('input.bootstrap-native-color');
          beforeReset = snapshot($colors);
          $colors.val('');
          refreshColors($colors);
          showReset(true);
          updateState();
          $form.find('.bn-undo').focus();
          return false;
        });
        $form.find('.bn-undo').bind('click', function () {
          if (beforeReset) {
            restore(beforeReset);
            beforeReset = null;
          }
          showReset(false);
          updateState();
          $form.find('input.bn-reset').focus();
        });
        $form.find('.bn-discard').bind('click', function () {
          $form.find('.bn-steps input[type=file]').val('');
          restore(saved);
          beforeReset = null;
          showReset(false);
          updateState();
          $form.find('.bn-savebar .form-submit').eq(0).focus();
        });
        $(document).bind('keydown', function (e) {
          if (e.keyCode === 27 && $('.bn-preview-pane').hasClass('is-open')) {
            $('.bn-preview-close').click();
          }
        });
        window.addEventListener('beforeunload', function (e) {
          if (dirty && !submitting) {
            e.preventDefault();
            e.returnValue = '';
          }
        });
      });
    }
  };
})(jQuery);
