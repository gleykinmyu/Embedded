/* IptvDropdown — стилизованные списки вместо native <select>.
 *
 * IptvDropdown.bind(rootEl)           — разметка .iptv-dd уже в HTML
 * IptvDropdown.enhance(selectEl)      — спрятать <select>, рядом .iptv-dd / listbox
 * IptvDropdown.enhanceAll(root?)      — все select без data-native / data-iptv-dd-skip
 * IptvDropdown.refresh(selectEl)      — после смены <option>
 *
 * Усиленный select: value/disabled/change работают как раньше.
 */
(function (global) {
  'use strict';

  var OPEN_Z = null;

  function closeOpen(except) {
    if (OPEN_Z && OPEN_Z !== except && typeof OPEN_Z.close === 'function') {
      OPEN_Z.close();
    }
  }

  function optionItems(select) {
    var items = [];
    Array.prototype.forEach.call(select.options || [], function (o) {
      items.push({
        value: o.value,
        label: o.textContent || o.value,
        offline: o.disabled,
        selected: o.selected,
      });
    });
    return items;
  }

  function makeDdShell(extraClass) {
    var root = document.createElement('div');
    root.className = 'iptv-dd' + (extraClass ? ' ' + extraClass : '');
    root.innerHTML =
      '<button type="button" class="iptv-dd-toggle" aria-haspopup="listbox" aria-expanded="false">' +
      '<span class="iptv-dd-label">—</span></button>' +
      '<ul class="iptv-dd-menu hidden" role="listbox"></ul>';
    return root;
  }

  function bindRoot(root, opts) {
    opts = opts || {};
    if (!root || root._iptvDdApi) return root && root._iptvDdApi;
    var toggle = root.querySelector('.iptv-dd-toggle');
    var labelEl = root.querySelector('.iptv-dd-label');
    var menu = root.querySelector('.iptv-dd-menu');
    if (!toggle || !labelEl || !menu) return null;

    var value = '';
    var disabled = false;
    var api;
    var placed = false;

    function clearMenuPlace() {
      menu.classList.remove('is-fixed');
      menu.style.position = '';
      menu.style.left = '';
      menu.style.top = '';
      menu.style.bottom = '';
      menu.style.width = '';
      menu.style.maxWidth = '';
      menu.style.maxHeight = '';
      menu.style.marginTop = '';
      menu.style.zIndex = '';
      menu.style.borderRadius = '';
      if (placed && menu.parentNode === document.body) {
        root.appendChild(menu);
      }
      placed = false;
    }

    function placeMenu() {
      var r = toggle.getBoundingClientRect();
      var maxH = Math.min(280, Math.floor(window.innerHeight * 0.5));
      var gap = 1;
      var spaceBelow = window.innerHeight - r.bottom - 8;
      var spaceAbove = r.top - 8;
      var openUp = spaceBelow < Math.min(140, maxH) && spaceAbove > spaceBelow;

      if (menu.parentNode !== document.body) {
        document.body.appendChild(menu);
      }
      placed = true;
      menu.classList.add('is-fixed');
      menu.style.position = 'fixed';
      /* округление вниз/вверх даёт субпиксельный «уезд» относительно toggle */
      menu.style.left = r.left + 'px';
      menu.style.width = r.width + 'px';
      menu.style.maxWidth = r.width + 'px';
      menu.style.zIndex = '400';
      menu.style.marginTop = '0';
      menu.style.right = 'auto';

      if (openUp) {
        menu.style.top = 'auto';
        menu.style.bottom = (window.innerHeight - r.top + gap) + 'px';
        menu.style.maxHeight = Math.max(80, Math.min(maxH, spaceAbove)) + 'px';
        menu.style.borderRadius = '8px 8px 0 0';
      } else {
        menu.style.bottom = 'auto';
        menu.style.top = (r.bottom - gap) + 'px';
        menu.style.maxHeight = Math.max(80, Math.min(maxH, spaceBelow)) + 'px';
        menu.style.borderRadius = '0 0 8px 8px';
      }
    }

    function close() {
      menu.classList.add('hidden');
      toggle.setAttribute('aria-expanded', 'false');
      root.classList.remove('is-open');
      clearMenuPlace();
      if (OPEN_Z === api) OPEN_Z = null;
    }

    function open() {
      if (disabled) return;
      closeOpen(api);
      menu.classList.remove('hidden');
      toggle.setAttribute('aria-expanded', 'true');
      root.classList.add('is-open');
      placeMenu();
      /* после появления меню может вылезти скроллбар и сдвинуть layout — пересчёт */
      requestAnimationFrame(function () {
        if (OPEN_Z === api) placeMenu();
        requestAnimationFrame(function () {
          if (OPEN_Z === api) placeMenu();
        });
      });
      OPEN_Z = api;
    }

    function syncLabel() {
      var text = opts.placeholder || '—';
      var found = false;
      Array.prototype.forEach.call(menu.querySelectorAll('[role="option"]'), function (li) {
        var on = li.getAttribute('data-value') === value;
        li.setAttribute('aria-selected', on ? 'true' : 'false');
        li.classList.toggle('is-active', on);
        if (on) {
          text = li.textContent;
          found = true;
        }
      });
      if (!found) {
        var first = menu.querySelector('[role="option"]');
        if (first) text = first.textContent;
      }
      labelEl.textContent = text;
    }

    function setValue(v, silent) {
      value = v == null ? '' : String(v);
      syncLabel();
      if (!silent) {
        if (typeof opts.onChange === 'function') opts.onChange(value);
        if (typeof root._onChange === 'function') root._onChange(value);
      }
    }

    function setOptions(items, emptyText) {
      menu.innerHTML = '';
      if (!items || !items.length) {
        labelEl.textContent = emptyText || opts.placeholder || '—';
        value = '';
        setDisabled(true);
        close();
        return;
      }
      setDisabled(false);
      items.forEach(function (item) {
        var li = document.createElement('li');
        li.setAttribute('role', 'option');
        li.setAttribute('data-value', item.value);
        li.textContent = item.label;
        if (item.offline) li.classList.add('is-offline');
        menu.appendChild(li);
      });
      syncLabel();
    }

    function setDisabled(on) {
      disabled = !!on;
      toggle.disabled = disabled;
      root.classList.toggle('is-disabled', disabled);
      if (disabled) close();
    }

    toggle.addEventListener('click', function (e) {
      e.preventDefault();
      e.stopPropagation();
      if (disabled) return;
      if (root.classList.contains('is-open')) close();
      else open();
    });

    menu.addEventListener('click', function (e) {
      var li = e.target.closest && e.target.closest('[role="option"]');
      if (!li || !menu.contains(li)) return;
      e.preventDefault();
      var v = li.getAttribute('data-value') || '';
      close();
      if (v === value) return;
      setValue(v);
    });

    api = {
      root: root,
      menu: menu,
      getValue: function () { return value; },
      setValue: function (v) { setValue(v, true); },
      setOptions: setOptions,
      setDisabled: setDisabled,
      close: close,
      reposition: function () {
        if (root.classList.contains('is-open')) placeMenu();
      },
      onChange: function (fn) { root._onChange = fn; },
    };
    root._iptvDdApi = api;
    return api;
  }

  function enhanceListbox(select) {
    if (select._iptvDd) return select._iptvDd;
    select.classList.add('iptv-native-hidden');
    select.setAttribute('tabindex', '-1');
    select.setAttribute('aria-hidden', 'true');

    var box = document.createElement('div');
    box.className = 'iptv-listbox';
    box.setAttribute('role', 'listbox');
    box.setAttribute('aria-label', select.getAttribute('aria-label') || 'Список');
    select.parentNode.insertBefore(box, select.nextSibling);

    function paint() {
      var cur = select.value;
      var items = optionItems(select);
      box.innerHTML = '';
      if (!items.length) {
        box.innerHTML = '<div class="iptv-listbox-empty">— нет совпадений —</div>';
        return;
      }
      items.forEach(function (item) {
        var row = document.createElement('div');
        row.className = 'iptv-listbox-item' + (item.value === cur ? ' is-active' : '');
        row.setAttribute('role', 'option');
        row.setAttribute('data-value', item.value);
        row.setAttribute('aria-selected', item.value === cur ? 'true' : 'false');
        row.textContent = item.label;
        box.appendChild(row);
      });
      var active = box.querySelector('.is-active');
      if (active && typeof active.scrollIntoView === 'function') {
        try { active.scrollIntoView({ block: 'nearest' }); } catch (e) {}
      }
    }

    box.addEventListener('click', function (e) {
      var row = e.target.closest && e.target.closest('.iptv-listbox-item');
      if (!row) return;
      var v = row.getAttribute('data-value') || '';
      if (select.value === v) return;
      select.value = v;
      paint();
      select.dispatchEvent(new Event('change', { bubbles: true }));
    });

    var api = {
      refresh: paint,
      getValue: function () { return select.value; },
      setValue: function (v) {
        select.value = v;
        paint();
      },
    };
    select._iptvDd = api;
    var mo = new MutationObserver(function () { paint(); });
    mo.observe(select, { childList: true, subtree: true, attributes: true, attributeFilter: ['disabled'] });
    paint();
    return api;
  }

  function enhanceSelect(select) {
    if (!select || select.nodeName !== 'SELECT') return null;
    if (select.getAttribute('data-native') === '1') return null;
    if (select.getAttribute('data-iptv-dd-skip') === '1') return null;
    if (select._iptvDd) return select._iptvDd;
    if (select.multiple) return null;

    var size = parseInt(select.getAttribute('size') || '1', 10);
    if (size > 1) return enhanceListbox(select);

    select.classList.add('iptv-native-hidden');
    select.setAttribute('tabindex', '-1');
    select.setAttribute('aria-hidden', 'true');

    var dd = makeDdShell(select.className.replace(/\btype-table\b/, '').trim());
    if (select.id) dd.setAttribute('data-for', select.id);
    select.parentNode.insertBefore(dd, select.nextSibling);

    var api = bindRoot(dd, {
      placeholder: select.getAttribute('data-placeholder') || '—',
      onChange: function (v) {
        if (select.value !== v) {
          select.value = v;
          select.dispatchEvent(new Event('change', { bubbles: true }));
        }
      },
    });
    if (!api) return null;

    function syncFromSelect() {
      api.setOptions(optionItems(select), select.getAttribute('data-empty') || '—');
      api.setValue(select.value);
      api.setDisabled(!!select.disabled);
    }

    syncFromSelect();

    var mo = new MutationObserver(function () { syncFromSelect(); });
    mo.observe(select, { childList: true, subtree: true, attributes: true, attributeFilter: ['disabled'] });

    var desc = Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype, 'value');
    if (desc && desc.set) {
      Object.defineProperty(select, 'value', {
        configurable: true,
        get: function () { return desc.get.call(select); },
        set: function (v) {
          desc.set.call(select, v);
          api.setValue(select.value);
        },
      });
    }

    var wrapped = {
      refresh: syncFromSelect,
      getValue: function () { return select.value; },
      setValue: function (v) { select.value = v; api.setValue(select.value); },
      api: api,
    };
    select._iptvDd = wrapped;
    return wrapped;
  }

  function enhanceAll(root) {
    root = root || document;
    var list = root.querySelectorAll ? root.querySelectorAll('select') : [];
    Array.prototype.forEach.call(list, function (sel) {
      try { enhanceSelect(sel); } catch (e) { console.warn(e); }
    });
  }

  function refresh(select) {
    if (!select) return;
    if (select._iptvDd && select._iptvDd.refresh) select._iptvDd.refresh();
    else enhanceSelect(select);
  }

  document.addEventListener('click', function (e) {
    if (!OPEN_Z) return;
    var t = e.target;
    if (OPEN_Z.root && OPEN_Z.root.contains(t)) return;
    if (OPEN_Z.menu && OPEN_Z.menu.contains(t)) return;
    OPEN_Z.close();
  });

  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape' && OPEN_Z) OPEN_Z.close();
  });

  window.addEventListener('resize', function () {
    if (OPEN_Z && typeof OPEN_Z.reposition === 'function') OPEN_Z.reposition();
  });
  window.addEventListener('scroll', function () {
    if (OPEN_Z && typeof OPEN_Z.reposition === 'function') OPEN_Z.reposition();
  }, true);

  function boot() {
    enhanceAll(document);
    /* новые <select> (динамические таблицы и т.п.) — без ручного enhanceAll */
    if (typeof MutationObserver === 'function') {
      var mo = new MutationObserver(function (mutations) {
        for (var i = 0; i < mutations.length; i++) {
          var nodes = mutations[i].addedNodes;
          if (!nodes) continue;
          for (var j = 0; j < nodes.length; j++) {
            var node = nodes[j];
            if (!node || node.nodeType !== 1) continue;
            try {
              if (node.nodeName === 'SELECT') enhanceSelect(node);
              else if (node.querySelectorAll) {
                var list = node.querySelectorAll('select');
                for (var k = 0; k < list.length; k++) enhanceSelect(list[k]);
              }
            } catch (err) { console.warn(err); }
          }
        }
      });
      mo.observe(document.documentElement, { childList: true, subtree: true });
    }
  }
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot);
  } else {
    boot();
  }

  global.IptvDropdown = {
    bind: bindRoot,
    enhance: enhanceSelect,
    enhanceAll: enhanceAll,
    refresh: refresh,
  };
})(window);
