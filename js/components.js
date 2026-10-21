/**
 * @file
 * Bootstrap components: the stylesheet size with the boxes as ticked now
 * (an estimate: parts share some styles).
 */
(function () {
  'use strict';
  function init() {
    var out = document.querySelector('.bnu-size-new');
    var boxes = document.querySelectorAll('.bnu-parts input[type="checkbox"][data-size]');
    if (!out || !boxes.length) {
      return;
    }
    var now = parseInt(out.getAttribute('data-now'), 10);
    var start = [];
    for (var i = 0; i < boxes.length; i++) {
      start.push(boxes[i].checked);
    }
    function update() {
      var size = now;
      for (var i = 0; i < boxes.length; i++) {
        var bytes = parseInt(boxes[i].getAttribute('data-size'), 10) || 0;
        if (boxes[i].checked && !start[i]) {
          size += bytes;
        }
        else if (!boxes[i].checked && start[i]) {
          size -= bytes;
        }
      }
      out.textContent = Math.max(0, Math.round(size / 1024));
    }
    for (var j = 0; j < boxes.length; j++) {
      boxes[j].addEventListener('change', update);
    }
  }
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  }
  else {
    init();
  }
})();
