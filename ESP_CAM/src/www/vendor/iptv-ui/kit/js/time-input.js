/* IptvTimeInput — поле времени ЧЧ:ММ с автоподстановкой «:».
 *
 * IptvTimeInput.bind(root, { selector?, onChange?(input), invalidClass? })
 *   делегирование input/keydown/blur на root (динамические строки OK)
 * IptvTimeInput.enhance(input)     — один input
 * IptvTimeInput.enhanceAll(root?)  — все input.time / [data-iptv-time]
 * IptvTimeInput.mask(value) → 'HH:MM'|частично
 * IptvTimeInput.isComplete(value)
 * IptvTimeInput.format(value)
 *
 * Разметка: <input class="time" data-iptv-time inputmode="numeric" maxlength="5">
 */
(function (global) {
  'use strict';

  var DEFAULT_SEL = 'input.time, input[data-iptv-time]';
  var INVALID = 'is-invalid';

  function digitsOnly(s) {
    return String(s || '').replace(/\D/g, '').slice(0, 4);
  }

  /** Только цифры → HH:MM, часы 00–23, минуты 00–59. После 2 цифр — «:». */
  function mask(raw) {
    var d = digitsOnly(raw);
    if (!d.length) return '';
    var out = '';
    var h1 = d.charAt(0);
    if (h1 > '2') h1 = '2';
    out += h1;
    if (d.length < 2) return out;
    var h2 = d.charAt(1);
    if (h1 === '2' && h2 > '3') h2 = '3';
    out += h2 + ':';
    if (d.length < 3) return out;
    var m1 = d.charAt(2);
    if (m1 > '5') m1 = '5';
    out += m1;
    if (d.length < 4) return out;
    out += d.charAt(3);
    return out;
  }

  function isComplete(v) {
    return /^\d{2}:\d{2}$/.test(String(v || ''));
  }

  function format(v) {
    var s = String(v || '').trim();
    if (/^\d{1,2}:\d{2}$/.test(s)) {
      var parts = s.split(':');
      var d = (('0' + parts[0]).slice(-2) + ('0' + parts[1]).slice(-2)).slice(0, 4);
      return mask(d);
    }
    return mask(s);
  }

  function setValidity(input, invalidClass) {
    var cls = invalidClass || INVALID;
    var v = input.value;
    input.classList.toggle(cls, v.length > 0 && !isComplete(v));
  }

  function applyMask(input, invalidClass) {
    var prev = input.value;
    var sel = input.selectionStart;
    if (sel == null) sel = prev.length;
    var digitsBefore = digitsOnly(prev.slice(0, sel)).length;
    var next = mask(prev);
    input.value = next;
    var pos = 0;
    var seen = 0;
    while (pos < next.length && seen < digitsBefore) {
      if (/\d/.test(next.charAt(pos))) seen++;
      pos++;
    }
    if (next.charAt(pos) === ':') pos++;
    try { input.setSelectionRange(pos, pos); } catch (e) {}
    setValidity(input, invalidClass);
  }

  function onBackspaceColon(e, input, invalidClass) {
    if (e.key !== 'Backspace') return false;
    var start = input.selectionStart;
    var end = input.selectionEnd;
    if (start !== end || start <= 0) return false;
    if (input.value.charAt(start - 1) !== ':') return false;
    e.preventDefault();
    var dig = digitsOnly(input.value);
    dig = dig.slice(0, Math.max(0, dig.length - 1));
    input.value = mask(dig);
    var pos = input.value.indexOf(':');
    if (pos < 0) pos = input.value.length;
    try { input.setSelectionRange(pos, pos); } catch (err) {}
    setValidity(input, invalidClass);
    return true;
  }

  function prep(input) {
    if (!input || input.nodeType !== 1) return;
    if (input.getAttribute('data-iptv-time-ready') === '1') return;
    input.setAttribute('data-iptv-time', '1');
    input.setAttribute('data-iptv-time-ready', '1');
    if (!input.getAttribute('inputmode')) input.setAttribute('inputmode', 'numeric');
    if (!input.getAttribute('maxlength')) input.setAttribute('maxlength', '5');
    if (!input.getAttribute('autocomplete')) input.setAttribute('autocomplete', 'off');
    if (!input.getAttribute('spellcheck')) input.setAttribute('spellcheck', 'false');
    if (!input.classList.contains('time')) input.classList.add('time');
  }

  function enhance(input) {
    if (!input) return null;
    prep(input);
    if (input.value) {
      input.value = format(input.value);
      setValidity(input);
    }
    if (input.getAttribute('data-iptv-time-bound') === '1') return input;
    input.setAttribute('data-iptv-time-bound', '1');
    input.addEventListener('input', function () { applyMask(input); });
    input.addEventListener('keydown', function (e) { onBackspaceColon(e, input); });
    input.addEventListener('blur', function () { setValidity(input); });
    return input;
  }

  function enhanceAll(root) {
    root = root && root.querySelectorAll ? root : document;
    Array.prototype.forEach.call(root.querySelectorAll(DEFAULT_SEL), enhance);
  }

  /**
   * Делегирование на контейнер (таблица слотов и т.п.).
   * @param {string|Element} rootOrId
   * @param {{selector?: string, onChange?: Function, invalidClass?: string}} [opts]
   */
  function bind(rootOrId, opts) {
    opts = opts || {};
    var root = typeof rootOrId === 'string'
      ? document.getElementById(rootOrId)
      : rootOrId;
    if (!root) return null;
    if (root.getAttribute('data-iptv-time-root') === '1') return root;
    root.setAttribute('data-iptv-time-root', '1');
    var sel = opts.selector || DEFAULT_SEL;
    var invalidClass = opts.invalidClass || INVALID;

    function match(t) {
      if (!t || t.tagName !== 'INPUT') return null;
      if (!(t.classList.contains('time') || t.hasAttribute('data-iptv-time'))) return null;
      if (!root.contains(t)) return null;
      return t;
    }

    root.addEventListener('focusin', function (e) {
      var inp = match(e.target);
      if (inp) prep(inp);
    });
    root.addEventListener('input', function (e) {
      var inp = match(e.target);
      if (!inp) return;
      applyMask(inp, invalidClass);
      if (typeof opts.onChange === 'function') opts.onChange(inp, e);
    });
    root.addEventListener('keydown', function (e) {
      var inp = match(e.target);
      if (!inp) return;
      if (onBackspaceColon(e, inp, invalidClass)) {
        if (typeof opts.onChange === 'function') opts.onChange(inp, e);
      }
    });
    root.addEventListener('blur', function (e) {
      var inp = match(e.target);
      if (inp) setValidity(inp, invalidClass);
    }, true);

    enhanceAll(root);
    return root;
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function () {
      enhanceAll(document);
    });
  } else {
    enhanceAll(document);
  }

  global.IptvTimeInput = {
    bind: bind,
    enhance: enhance,
    enhanceAll: enhanceAll,
    mask: mask,
    format: format,
    isComplete: isComplete,
  };
})(window);
