/**
 * IptvRowDnD — drag-and-drop строк/карточек за ручку .drag-handle
 *
 * IptvRowDnD.bind(rootEl|id, {
 *   itemSel: 'tr[data-idx]',
 *   indexAttr: 'data-idx',
 *   handleSel: '.drag-handle',
 *   canDrag: function () { return true; },
 *   onDrop: function (from, insertAt) {},
 *   onDragStart: function () {},
 *   onDragEnd: function () {}
 * })
 *
 * IptvRowDnD.moveItem(arr, from, insertAt)
 *
 * Важно: draggable ставится только на ручку (не на всю строку),
 * иначе после mousedown+mouseup вне таблицы строка остаётся draggable
 * и клики по кнопкам начинают drag вместо click.
 */
(function (global) {
  'use strict';

  function dropSide(el, clientY) {
    var rect = el.getBoundingClientRect();
    return (clientY - rect.top) < rect.height / 2 ? 'before' : 'after';
  }

  function moveItem(arr, from, insertAt) {
    if (!arr || from < 0 || from >= arr.length) return false;
    if (insertAt < 0 || insertAt > arr.length) return false;
    var adj = insertAt;
    if (from < adj) adj -= 1;
    if (adj === from) return false;
    var item = arr.splice(from, 1)[0];
    arr.splice(adj, 0, item);
    return true;
  }

  function bind(root, opts) {
    if (!opts || !opts.itemSel || typeof opts.onDrop !== 'function') return null;
    if (typeof root === 'string') root = document.getElementById(root);
    if (!root) return null;
    if (root.getAttribute('data-iptv-dnd') === '1') return root._iptvDnd || null;
    root.setAttribute('data-iptv-dnd', '1');

    var itemSel = opts.itemSel;
    var indexAttr = opts.indexAttr || 'data-idx';
    var handleSel = opts.handleSel || '.drag-handle';
    var dragSrc = -1;
    var active = false;
    var armedHandle = null;

    function canDrag() {
      return !opts.canDrag || !!opts.canDrag();
    }

    function clearMarks() {
      root.querySelectorAll('.drag-over-before, .drag-over-after, .is-dragging').forEach(function (node) {
        node.classList.remove('drag-over-before', 'drag-over-after', 'is-dragging');
      });
    }

    function clearArmed() {
      if (armedHandle) {
        armedHandle.removeAttribute('draggable');
        armedHandle = null;
      }
      root.querySelectorAll(handleSel + '[draggable="true"]').forEach(function (node) {
        node.removeAttribute('draggable');
      });
      root.querySelectorAll(itemSel + '[draggable="true"]').forEach(function (node) {
        node.removeAttribute('draggable');
      });
    }

    function itemIndex(item) {
      var n = parseInt(item.getAttribute(indexAttr), 10);
      return isNaN(n) ? -1 : n;
    }

    function eventTarget(e) {
      var t = e.target;
      if (t && t.nodeType === 3) t = t.parentElement;
      return t && t.closest ? t : null;
    }

    root.addEventListener('mousedown', function (e) {
      if (!canDrag()) return;
      var t = eventTarget(e);
      if (!t) return;
      /* клик по контролам строки — никогда не начинать drag */
      if (t.closest('input, button, a, textarea, select, label')) return;
      var handle = t.closest(handleSel);
      var item = t.closest(itemSel);
      if (!handle || !item || !root.contains(item)) return;
      clearArmed();
      handle.setAttribute('draggable', 'true');
      armedHandle = handle;
    });

    function onPointerUp() {
      if (dragSrc < 0) clearArmed();
    }
    document.addEventListener('mouseup', onPointerUp);
    document.addEventListener('pointerup', onPointerUp);

    root.addEventListener('dragstart', function (e) {
      var t = eventTarget(e);
      if (!t) {
        e.preventDefault();
        return;
      }
      var handle = t.closest(handleSel) || (armedHandle && armedHandle.contains(t) ? armedHandle : null);
      if (!handle || handle.getAttribute('draggable') !== 'true') {
        e.preventDefault();
        clearArmed();
        return;
      }
      var item = handle.closest(itemSel);
      if (!item || !root.contains(item)) {
        e.preventDefault();
        clearArmed();
        return;
      }
      dragSrc = itemIndex(item);
      if (dragSrc < 0) {
        e.preventDefault();
        clearArmed();
        return;
      }
      active = true;
      item.classList.add('is-dragging');
      e.dataTransfer.effectAllowed = 'move';
      try { e.dataTransfer.setData('text/plain', String(dragSrc)); } catch (err) {}
      if (opts.onDragStart) opts.onDragStart(dragSrc, item);
    });

    root.addEventListener('dragend', function () {
      clearMarks();
      clearArmed();
      if (active && opts.onDragEnd) opts.onDragEnd();
      active = false;
      dragSrc = -1;
    });

    root.addEventListener('dragover', function (e) {
      if (dragSrc < 0) return;
      var t = eventTarget(e);
      var item = t && t.closest(itemSel);
      if (!item || !root.contains(item)) return;
      e.preventDefault();
      e.dataTransfer.dropEffect = 'move';
      clearMarks();
      var dragging = root.querySelector(itemSel + '[' + indexAttr + '="' + dragSrc + '"]');
      if (dragging) dragging.classList.add('is-dragging');
      item.classList.add(dropSide(item, e.clientY) === 'before' ? 'drag-over-before' : 'drag-over-after');
    });

    root.addEventListener('drop', function (e) {
      if (dragSrc < 0) return;
      var t = eventTarget(e);
      var item = t && t.closest(itemSel);
      if (!item || !root.contains(item)) return;
      e.preventDefault();
      e.stopPropagation();
      var to = itemIndex(item);
      if (to < 0) return;
      var insertAt = dropSide(item, e.clientY) === 'before' ? to : to + 1;
      var from = dragSrc;
      clearMarks();
      clearArmed();
      dragSrc = -1;
      active = false;
      if (opts.onDragEnd) opts.onDragEnd();
      if (from !== insertAt && from !== insertAt - 1) {
        opts.onDrop(from, insertAt);
      }
    });

    var api = {
      destroy: function () {
        document.removeEventListener('mouseup', onPointerUp);
        document.removeEventListener('pointerup', onPointerUp);
        clearArmed();
        root.removeAttribute('data-iptv-dnd');
        root._iptvDnd = null;
      },
    };
    root._iptvDnd = api;
    return api;
  }

  global.IptvRowDnD = {
    bind: bind,
    moveItem: moveItem,
    dropSide: dropSide,
  };
})(window);
