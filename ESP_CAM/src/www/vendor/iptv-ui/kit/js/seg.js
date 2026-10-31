/* IptvSeg — сегментированный переключатель (стили в page.css → .iptv-seg).
 *
 * IptvSeg.bind(root, { onChange(value, btn) })
 * IptvSeg.set(root, value)   // value = data-value кнопки; '' снимает выбор
 * IptvSeg.get(root) → string|null
 */
(function (global) {
  'use strict';

  function resolve(elOrId) {
    if (!elOrId) return null;
    if (typeof elOrId === 'string') return document.getElementById(elOrId);
    return elOrId.nodeType ? elOrId : null;
  }

  function buttons(root) {
    return Array.prototype.slice.call(root.querySelectorAll('.iptv-seg__btn'));
  }

  function set(rootOrId, value) {
    var root = resolve(rootOrId);
    if (!root) return;
    var v = value == null ? '' : String(value);
    buttons(root).forEach(function (btn) {
      var on = v !== '' && btn.getAttribute('data-value') === v;
      btn.classList.toggle('is-active', on);
      btn.setAttribute('aria-pressed', on ? 'true' : 'false');
    });
  }

  function get(rootOrId) {
    var root = resolve(rootOrId);
    if (!root) return null;
    var active = root.querySelector('.iptv-seg__btn.is-active');
    return active ? active.getAttribute('data-value') : null;
  }

  function bind(rootOrId, opts) {
    opts = opts || {};
    var root = resolve(rootOrId);
    if (!root) return null;
    if (root.getAttribute('data-iptv-seg') === '1') return root;
    root.setAttribute('data-iptv-seg', '1');
    if (!root.getAttribute('role')) root.setAttribute('role', 'group');

    root.addEventListener('click', function (e) {
      var btn = e.target && e.target.closest ? e.target.closest('.iptv-seg__btn') : null;
      if (!btn || !root.contains(btn) || btn.disabled) return;
      e.preventDefault();
      var value = btn.getAttribute('data-value');
      set(root, value);
      if (typeof opts.onChange === 'function') opts.onChange(value, btn);
    });

    buttons(root).forEach(function (btn) {
      if (!btn.hasAttribute('aria-pressed')) {
        btn.setAttribute('aria-pressed', btn.classList.contains('is-active') ? 'true' : 'false');
      }
    });
    return root;
  }

  global.IptvSeg = { bind: bind, set: set, get: get };
})(window);
