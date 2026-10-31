/* Shared confirm dialog — requires {{SITE_CONFIRM}} / #confirm-modal in page */
(function (global) {
  var pending = null;

  function el(id) {
    return document.getElementById(id);
  }

  function close(result) {
    if (global.IptvModal) IptvModal.close('confirm-modal');
    else {
      var modal = el('confirm-modal');
      if (modal) modal.classList.add('hidden');
    }
    if (pending) {
      var done = pending;
      pending = null;
      done(!!result);
    }
  }

  function ask(message, okLabel) {
    var modal = el('confirm-modal');
    if (!modal) {
      return Promise.resolve(window.confirm(message));
    }
    if (pending) close(false);
    el('confirm-text').textContent = message || '';
    var ok = el('confirm-ok');
    var label = okLabel || 'OK';
    ok.textContent = label;
    /* destructive primary — красный; иначе accent */
    var danger = /удал|delete|remove|kill|сброс|reset/i.test(label);
    ok.className = danger ? 'kill' : 'refresh';
    if (global.IptvModal) IptvModal.open(modal);
    else {
      modal.classList.remove('hidden');
      document.body.classList.add('modal-open');
    }
    setTimeout(function () {
      var ok = el('confirm-ok');
      if (ok) ok.focus();
    }, 0);
    return new Promise(function (resolve) {
      pending = resolve;
    });
  }

  function bind() {
    var modal = el('confirm-modal');
    if (!modal || modal.getAttribute('data-bound') === '1') return;
    modal.setAttribute('data-bound', '1');
    el('confirm-ok').addEventListener('click', function () { close(true); });
    if (global.IptvModal) {
      IptvModal.bind(modal, {
        onClose: function () {
          if (pending) {
            var done = pending;
            pending = null;
            done(false);
          }
        },
      });
    } else {
      el('confirm-cancel').addEventListener('click', function () { close(false); });
      var closeBtn = el('confirm-close');
      if (closeBtn) closeBtn.addEventListener('click', function () { close(false); });
      modal.addEventListener('click', function (e) {
        if (e.target === modal) close(false);
      });
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bind);
  } else {
    bind();
  }

  global.IptvConfirm = { ask: ask, close: close };
})(window);
