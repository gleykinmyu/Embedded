/* Общий контроллер модалок (.modal) и оверлеев (.file-picker-overlay).
 *
 * IptvModal.open(id|el)
 * IptvModal.close(id|el)
 * IptvModal.isOpen(id|el)
 * IptvModal.anyOpen(except?)
 * IptvModal.syncBody()
 * IptvModal.bind(id|el, { onClose, backdrop=true, escape=true })
 *
 * Кнопки закрытия: атрибут data-modal-close (крестик / Отмена).
 * Клик по фону закрывает, если backdrop не выключен.
 * Escape закрывает верхнее окно (по data-modal-opened).
 */
(function (global) {
  'use strict';

  var DIALOG_SEL = '.modal, .file-picker-overlay';

  function resolve(elOrId) {
    if (!elOrId) return null;
    if (typeof elOrId === 'string') return document.getElementById(elOrId);
    return elOrId.nodeType ? elOrId : null;
  }

  function eachDialog(fn) {
    Array.prototype.forEach.call(document.querySelectorAll(DIALOG_SEL), fn);
  }

  function anyOpen(except) {
    var exceptEl = resolve(except);
    var found = false;
    eachDialog(function (m) {
      if (found) return;
      if (m.classList.contains('hidden')) return;
      if (exceptEl && m === exceptEl) return;
      found = true;
    });
    return found;
  }

  function syncBody() {
    if (anyOpen()) document.body.classList.add('modal-open');
    else document.body.classList.remove('modal-open');
  }

  function topOpen() {
    var best = null;
    var bestTs = -1;
    eachDialog(function (m) {
      if (m.classList.contains('hidden')) return;
      var ts = parseInt(m.getAttribute('data-modal-opened') || '0', 10);
      if (ts >= bestTs) {
        bestTs = ts;
        best = m;
      }
    });
    return best;
  }

  function open(elOrId) {
    var modal = resolve(elOrId);
    if (!modal) return null;
    modal.classList.remove('hidden');
    modal.style.removeProperty('display');
    modal.setAttribute('data-modal-opened', String(Date.now()));
    document.body.classList.add('modal-open');
    return modal;
  }

  function close(elOrId) {
    var modal = resolve(elOrId);
    if (!modal) return;
    modal.classList.add('hidden');
    modal.style.removeProperty('display');
    modal.removeAttribute('data-modal-opened');
    syncBody();
  }

  /** Закрыть все .modal и .file-picker-overlay (аварийный сброс «залипшего» оверлея). */
  function closeAll() {
    eachDialog(function (m) {
      m.classList.add('hidden');
      m.style.removeProperty('display');
      m.removeAttribute('data-modal-opened');
      /* не трогаем __iptvRequestClose — bind() один раз, обнуление ломает Esc */
    });
    document.body.classList.remove('modal-open');
  }

  function isOpen(elOrId) {
    var modal = resolve(elOrId);
    return !!(modal && !modal.classList.contains('hidden'));
  }

  /**
   * @param {string|Element} elOrId
   * @param {{onClose?: Function, backdrop?: boolean, escape?: boolean}} [opts]
   */
  function bind(elOrId, opts) {
    opts = opts || {};
    var modal = resolve(elOrId);
    if (!modal) return null;
    if (modal.getAttribute('data-iptv-modal') === '1') return modal;
    modal.setAttribute('data-iptv-modal', '1');

    function requestClose(ev) {
      if (typeof opts.onClose === 'function') {
        try { opts.onClose(ev); } catch (err) { console.warn(err); }
      }
      close(modal);
    }

    modal.__iptvRequestClose = requestClose;

    modal.addEventListener('click', function (e) {
      var t = e.target;
      if (t && t.nodeType === 3) t = t.parentElement;
      if (!t || !t.closest) return;

      if (opts.backdrop !== false && t === modal) {
        requestClose(e);
        return;
      }

      var btn = t.closest('[data-modal-close]');
      if (btn && modal.contains(btn)) {
        e.preventDefault();
        requestClose(e);
      }
    });

    if (opts.escape !== false) {
      modal.setAttribute('data-modal-escape', '1');
    }

    return modal;
  }

  document.addEventListener('keydown', function (e) {
    if (e.key !== 'Escape') return;
    var top = topOpen();
    if (!top || top.getAttribute('data-modal-escape') !== '1') return;
    if (typeof top.__iptvRequestClose === 'function') top.__iptvRequestClose(e);
    else close(top);
  });

  global.IptvModal = {
    open: open,
    close: close,
    closeAll: closeAll,
    isOpen: isOpen,
    anyOpen: anyOpen,
    syncBody: syncBody,
    topOpen: topOpen,
    bind: bind,
  };
})(window);
