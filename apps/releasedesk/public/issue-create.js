// Per-tab creation intent. JTML still owns POST, loading/error/success and snapshots.
(() => {
  const form = document.querySelector('[data-create-form]');
  if (!form) return;
  const title = form.querySelector('[data-draft-title]');
  const description = form.querySelector('[data-draft-description]');
  const severity = form.querySelector('[data-draft-severity]');
  const keyInput = form.querySelector('[data-request-key]');
  const submit = form.querySelector('[data-create-submit]');
  const separate = form.querySelector('[data-new-create-intent]');
  const notice = document.querySelector('[data-create-protection]');
  const feedback = document.querySelector('[data-create-feedback]');
  const storageKey = 'releasedesk.creation-intent.v1';
  let intent = null;
  let restoring = false;
  const draft = () => ({title: title.value, description: description.value, severity: severity.value});
  // Match Python str.strip(), including its additional control whitespace.
  const strip = text => text.replace(/^[\u0009-\u000d\u001c-\u0020\u0085\u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u3000]+|[\u0009-\u000d\u001c-\u0020\u0085\u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u3000]+$/g, '');
  const normalized = value => ({title: strip(value.title), description: strip(value.description), severity: value.severity});
  const samePayload = (left, right) => left.title === right.title && left.description === right.description && left.severity === right.severity;

  function message(text) {
    notice.textContent = text;
    notice.hidden = !text;
  }
  function store() {
    if (intent) sessionStorage.setItem(storageKey, JSON.stringify(intent));
    else sessionStorage.removeItem(storageKey);
  }
  function randomKey() {
    return [...crypto.getRandomValues(new Uint8Array(16))].map(byte => byte.toString(16).padStart(2, '0')).join('');
  }
  function setValue(control, value, event) {
    control.value = value;
    // Synchronize only the input's declared JTML binding through its normal event.
    control.dispatchEvent(new Event(event, {bubbles: true}));
  }
  function confirmed() {
    const success = feedback.querySelector('[data-confirmed-key]');
    if (intent && success?.getAttribute('data-confirmed-key') === intent.key && !intent.confirmed) {
      intent.confirmed = true;
      store();
    }
  }
  function initialize() {
    try {
      const saved = sessionStorage.getItem(storageKey);
      if (saved) {
        intent = JSON.parse(saved);
        if (!intent || typeof intent.key !== 'string' || !/^[0-9a-f]{32}$/.test(intent.key) || typeof intent.confirmed !== 'boolean' ||
            !['title', 'description', 'severity'].every(field => typeof intent.draft?.[field] === 'string' && typeof intent.payload?.[field] === 'string'))
          throw Error('Invalid saved creation intent');
        restoring = true;
        setValue(title, intent.draft.title, 'input');
        setValue(description, intent.draft.description, 'input');
        setValue(severity, intent.draft.severity, 'input');
        setValue(keyInput, intent.key, 'input');
      }
      randomKey(); // Verify secure randomness before enabling any submission.
      sessionStorage.setItem(storageKey + '.check', 'ready');
      sessionStorage.removeItem(storageKey + '.check');
      submit.disabled = false;
      separate.disabled = false;
      message(intent && !intent.confirmed ? 'Pending save restored. Save issue retries it; no request was sent automatically.' : '');
    } catch (_) {
      submit.disabled = true;
      separate.disabled = false;
      message('Safe saving is unavailable. Browser session storage and secure randomness are required. A pending save may already exist; inspect Saved issues before starting a separate issue.');
    } finally {
      restoring = false;
    }
  }
  form.addEventListener('submit', event => {
    try {
      if (submit.disabled || form.querySelector('fieldset').disabled) throw Error('Saving is unavailable');
      confirmed();
      const fields = draft();
      const payload = normalized(fields);
      if (!intent || (intent.confirmed && !samePayload(payload, intent.payload)))
        intent = {key: randomKey(), payload, draft: fields, confirmed: false};
      intent.draft = fields;
      intent.payload = payload;
      store(); // Retain the key/draft before JTML is allowed to start the POST.
      setValue(keyInput, intent.key, 'input');
      message('');
    } catch (_) {
      event.preventDefault();
      event.stopImmediatePropagation();
      message('Save was not sent because retry protection could not be retained. Check browser session storage, then retry.');
    }
  }, true);
  function retainDraft(event) {
    if (restoring || !intent || event.target === keyInput) return;
    intent.draft = draft();
    try { store(); } catch (_) { message('The draft could not be retained. Check session storage before retrying.'); }
  }
  form.addEventListener('input', retainDraft);
  form.addEventListener('change', retainDraft);
  function startSeparate(clearText) {
    if (form.querySelector('fieldset').disabled) return;
    try {
      intent = null;
      store();
      setValue(keyInput, '', 'input');
      submit.disabled = false;
      message(clearText ? '' : 'The next Save issue creates a separate issue, even if the fields match an earlier issue.');
    } catch (_) {
      submit.disabled = true;
      message('A separate issue could not be started. Browser session storage is required.');
    }
  }
  separate.addEventListener('click', () => startSeparate(false));
  document.addEventListener('click', event => {
    if (event.target.closest('[data-clear-create-intent]')) startSeparate(true);
  }, true);
  new MutationObserver(() => {
    try { confirmed(); } catch (_) { message('The confirmation could not be retained. Retrying the same fields still uses the pending request.'); }
  }).observe(feedback, {childList: true, subtree: true, attributes: true, attributeFilter: ['data-confirmed-key']});
  if (document.readyState === 'complete') initialize();
  else window.addEventListener('load', initialize, {once: true});
})();
