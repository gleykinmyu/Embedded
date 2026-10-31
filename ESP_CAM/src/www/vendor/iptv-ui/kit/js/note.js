/* IptvNote — единые статусные сообщения панели.
 *
 * Разметка: <div id="note" class="note hidden" role="status"></div>
 * Files: #status.status-box.note + #status-text + .status-progress
 * Player: #player-note.note.player-note
 *
 * IptvNote.show(text, kind?, opts?)
 *   kind: '' | 'info' | 'ok' | 'err' | 'warn'
 *   opts: {
 *     el: 'note' | Element,   // цель (по умолчанию #note)
 *     ttl: ms | 0 | false,    // автоскрытие; 0/false = не скрывать
 *     sticky: true,           // то же, что ttl: 0
 *     progress: 0..100|null   // полоса прогресса (files)
 *   }
 * IptvNote.clear(el?)
 * IptvNote.configure({ defaultTtl, defaultEl, ttlByKind })
 *
 * Страница: body[data-note-ttl="5000"] или IptvNote.configure({ defaultTtl: 5000 })
 *
 * По умолчанию: ok / warn / err / info — автоскрытие;
 * пустой kind (загрузка…) — sticky, пока не сменят сообщение или clear().
 */
(function (global) {
  'use strict';

  var DEFAULT_TTL = 4500;
  var TTL_BY_KIND = { ok: 4000, warn: 5500, err: 7000, info: 4500 };
  var DEFAULT_EL = 'note';
  var timers = new WeakMap();
  var KIND_RE = /\b(ok|err|warn|info)\b/g;

  function cfgFromBody() {
    var b = document.body;
    if (!b) return;
    var ttl = parseInt(b.getAttribute('data-note-ttl') || '', 10);
    if (ttl > 0) DEFAULT_TTL = ttl;
    var el = b.getAttribute('data-note-el');
    if (el) DEFAULT_EL = el;
  }

  function resolveEl(elOrId) {
    if (elOrId == null || elOrId === '') elOrId = DEFAULT_EL;
    if (typeof elOrId === 'string') return document.getElementById(elOrId);
    return elOrId && elOrId.nodeType ? elOrId : null;
  }

  function textTarget(box) {
    if (!box) return null;
    var inner = box.querySelector('.note-text, .status-text');
    if (inner) return inner;
    if (box.id === 'status') {
      var st = document.getElementById('status-text');
      if (st) return st;
    }
    return box;
  }

  function clearTimer(box) {
    var t = timers.get(box);
    if (t) {
      clearTimeout(t);
      timers.delete(box);
    }
  }

  function normalizeKind(kind) {
    if (!kind) return '';
    kind = String(kind);
    if (kind === 'error') return 'err';
    if (kind === 'success') return 'ok';
    if (kind === 'warning') return 'warn';
    return kind;
  }

  function resolveTtl(kind, opts) {
    opts = opts || {};
    if (opts.sticky) return 0;
    if (opts.ttl === 0 || opts.ttl === false) return 0;
    if (typeof opts.ttl === 'number' && opts.ttl > 0) return opts.ttl;
    if (!kind) return 0;
    if (TTL_BY_KIND[kind] != null) return TTL_BY_KIND[kind];
    return DEFAULT_TTL;
  }

  function setProgress(box, pct) {
    var prog =
      box.querySelector('.status-progress') ||
      (box.id === 'status' ? document.getElementById('status-progress') : null);
    var bar =
      box.querySelector('.status-progress-bar') ||
      document.getElementById('status-progress-bar');
    if (!prog || !bar) return;
    if (pct == null || pct === false) {
      prog.classList.add('hidden');
      bar.style.width = '0%';
      return;
    }
    prog.classList.remove('hidden');
    bar.style.width = Math.max(0, Math.min(100, Number(pct) || 0)) + '%';
  }

  function stripKinds(box) {
    box.className = String(box.className || '').replace(KIND_RE, ' ').replace(/\s+/g, ' ').trim();
  }

  function clear(elOrId) {
    var box = resolveEl(elOrId);
    if (!box) return;
    clearTimer(box);
    stripKinds(box);
    box.classList.add('hidden');
    var target = textTarget(box);
    if (target) target.textContent = '';
    setProgress(box, null);
  }

  function show(text, kind, opts) {
    opts = opts || {};
    if (typeof kind === 'object' && kind !== null) {
      opts = kind;
      kind = opts.kind || '';
    }
    kind = normalizeKind(kind);

    var box = resolveEl(opts.el);
    if (!box) return null;

    if (text == null || text === '') {
      clear(box);
      return box;
    }

    clearTimer(box);
    stripKinds(box);
    if (!/\bnote\b/.test(box.className)) {
      box.className = (box.className ? box.className + ' ' : '') + 'note';
    }
    if (kind) box.classList.add(kind);
    box.classList.remove('hidden');

    var target = textTarget(box);
    if (target) target.textContent = String(text);

    if ('progress' in opts) setProgress(box, opts.progress);
    else setProgress(box, null);

    var ttl = resolveTtl(kind, opts);
    if (ttl > 0) {
      var handle = setTimeout(function () {
        timers.delete(box);
        clear(box);
      }, ttl);
      timers.set(box, handle);
    }
    return box;
  }

  function configure(opts) {
    opts = opts || {};
    if (typeof opts.defaultTtl === 'number' && opts.defaultTtl >= 0) {
      DEFAULT_TTL = opts.defaultTtl;
    }
    if (opts.defaultEl) DEFAULT_EL = opts.defaultEl;
    if (opts.ttlByKind && typeof opts.ttlByKind === 'object') {
      Object.keys(opts.ttlByKind).forEach(function (k) {
        TTL_BY_KIND[k] = opts.ttlByKind[k];
      });
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', cfgFromBody);
  } else {
    cfgFromBody();
  }

  global.IptvNote = {
    show: show,
    clear: clear,
    configure: configure,
  };
})(window);
