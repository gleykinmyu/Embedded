/* Shared prompt dialog — requires #prompt-modal (via {{SITE_CONFIRM}}) */
(function (global) {
  var pending = null;

  function el(id) {
    return document.getElementById(id);
  }

  function close(result) {
    if (global.IptvModal) IptvModal.close('prompt-modal');
    else {
      var modal = el('prompt-modal');
      if (modal) modal.classList.add('hidden');
    }
    if (pending) {
      var done = pending;
      pending = null;
      done(result);
    }
  }

  function submit() {
    var input = el('prompt-input');
    var val = input ? String(input.value || '') : '';
    close(val);
  }

  function ask(titleOrOpts, defaultValue, okLabel) {
    var opts = typeof titleOrOpts === 'object' && titleOrOpts
      ? titleOrOpts
      : { title: titleOrOpts, value: defaultValue, okLabel: okLabel };

    var modal = el('prompt-modal');
    if (!modal) {
      var fallback = window.prompt(opts.title || '', opts.value != null ? opts.value : '');
      return Promise.resolve(fallback);
    }
    if (pending) close(null);

    el('prompt-title').textContent = opts.title || 'Ввод';
    var text = el('prompt-text');
    if (opts.message) {
      text.textContent = opts.message;
      text.classList.remove('hidden');
    } else {
      text.textContent = '';
      text.classList.add('hidden');
    }
    el('prompt-label').textContent = opts.label || 'Имя';
    el('prompt-ok').textContent = opts.okLabel || 'OK';
    var input = el('prompt-input');
    input.value = opts.value != null ? String(opts.value) : '';
    if (global.IptvModal) IptvModal.open(modal);
    else {
      modal.classList.remove('hidden');
      document.body.classList.add('modal-open');
    }
    setTimeout(function () {
      input.focus();
      input.select();
    }, 0);

    return new Promise(function (resolve) {
      pending = resolve;
    });
  }

  function bind() {
    var modal = el('prompt-modal');
    if (!modal || modal.getAttribute('data-bound') === '1') return;
    modal.setAttribute('data-bound', '1');
    el('prompt-ok').addEventListener('click', submit);
    el('prompt-input').addEventListener('keydown', function (e) {
      if (e.key === 'Enter') {
        e.preventDefault();
        submit();
      }
    });
    if (global.IptvModal) {
      IptvModal.bind(modal, {
        onClose: function () {
          if (pending) {
            var done = pending;
            pending = null;
            done(null);
          }
        },
      });
    } else {
      el('prompt-cancel').addEventListener('click', function () { close(null); });
      el('prompt-close').addEventListener('click', function () { close(null); });
      modal.addEventListener('click', function (e) {
        if (e.target === modal) close(null);
      });
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bind);
  } else {
    bind();
  }

  global.IptvPrompt = { ask: ask, close: close };
})(window);
