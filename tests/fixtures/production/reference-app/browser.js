// Browser-only regression scenario. Loaded before the generated application assets.
(() => {
  const network = [];
  const errors = [];
  window.addEventListener('error', event => errors.push(event.message));
  window.addEventListener('unhandledrejection', event => errors.push(String(event.reason)));
  window.WebSocket = class {
    constructor(url) { network.push(String(url)); throw new Error('WebSocket is forbidden'); }
  };
  const fetch = window.fetch;
  window.fetch = function (url, ...args) {
    network.push(String(url));
    return fetch.call(this, url, ...args);
  };
  const open = XMLHttpRequest.prototype.open;
  XMLHttpRequest.prototype.open = function (method, url, ...args) {
    network.push(String(url));
    return open.call(this, method, url, ...args);
  };
  window.runReferenceAppChecks = async function () {
    const checks = [];
    let clicks = 0;
    const assert = (condition, message) => {
      if (!condition) throw new Error(message + ' ' + JSON.stringify({ errors, action: window.jtml.directComponentLastAction, binding: window.jtml.directComponentBindingDiagnostics, fallback: window.jtml.directComponentBodyPlanFallback, patch: window.jtml.directComponentLastPatch }));
      checks.push(message);
    };
    const settle = () => new Promise(resolve => setTimeout(resolve, 30));
    const cards = () => Array.from(document.querySelectorAll('.task-card'));
    const click = (label, root = document) => {
      const button = Array.from(root.querySelectorAll('button')).find(el => el.textContent.trim() === label);
      if (!button) throw new Error('Missing button: ' + label);
      button.click();
      clicks += 1;
    };
    const regression = window.jtmlReferenceRegression;
    if (regression) {
      if (regression === 'routing') {
        assert(errors.length === 0, 'routing starts without initialization errors');
        const route = path => document.querySelector('[data-jtml-route="' + path + '"]');
        assert(route('/').getClientRects().length > 0, 'home route initially visible');
        document.querySelector('[data-jtml-href="#/about"]').click();
        clicks += 1;
        await settle();
        assert(route('/about').getClientRects().length > 0 && route('/').getClientRects().length === 0, 'about route navigation');
        document.querySelector('[data-jtml-href="#/"]').click();
        clicks += 1;
        await settle();
        assert(route('/').getClientRects().length > 0, 'return to home route');
      } else if (regression === 'nested-heading') {
        click('Rename');
        await settle();
        assert(document.querySelector('h2').textContent === 'After', 'nested heading updates when parameter is named title');
      } else {
        const input = document.querySelector('input');
        input.value = 'Saved note';
        input.dispatchEvent(new Event('input', { bubbles: true }));
        if (regression === 'form-submit') {
          document.querySelector('form').dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
        } else {
          click('Save note', cards()[0]);
        }
        await settle();
        assert(document.body.textContent.includes('Saved note'), regression + ' saves input in visible DOM');
      }
      assert(network.length === 0 && errors.length === 0, 'regression runs without network or browser errors');
      return { checks, clicks };
    }
    const original = cards();
    assert(original.length === 3, 'three initial task components');
    const [audit, production, docs] = original;
    assert(document.querySelector('.board-summary').textContent.includes('Tasks: 3'), 'derived task count');
    const keyed = Array.from(document.querySelectorAll('[data-jtml-direct-list-key]'));
    const wrapper = audit.closest('[data-jtml-instance]');
    const instance = wrapper.getAttribute('data-jtml-instance');
    const input = audit.querySelector('input');
    input.focus();
    input.value = 'Review <release> & CSP';
    input.setSelectionRange(6, 6);
    input.dispatchEvent(new Event('input', { bubbles: true }));
    await settle();
    assert(audit.querySelector('input') === input && document.activeElement === input && input.selectionStart === 6,
      'input identity, focus and cursor retained');
    click('Save note', audit);
    await settle();
    assert(audit.textContent.includes('Review <release> & CSP'), 'named save action renders escaped note');
    assert(!audit.querySelector('release'), 'user note is not interpreted as HTML');
    click('Toggle complete', audit);
    await settle();
    assert(audit.textContent.includes('Complete') && production.textContent.includes('Open'), 'local completion and sibling isolation');
    click('Prioritize documentation');
    await settle();
    const reordered = cards();
    const rows = Array.from(document.querySelectorAll('[data-jtml-direct-list-key]'));
    assert(rows.length === 3 && rows[0] === keyed[2] && rows[1] === keyed[0] && rows[2] === keyed[1], 'keyed reorder retains and moves priority rows');
    assert(reordered[0] === audit && reordered[1] === production && reordered[2] === docs, 'task editor DOM retained during priority reorder');
    assert(audit.closest('[data-jtml-instance]') === wrapper && wrapper.getAttribute('data-jtml-instance') === instance,
      'task component instance identity retained');
    assert(audit.textContent.includes('Complete') && audit.textContent.includes('Review <release> & CSP') && input.value === 'Review <release> & CSP',
      'completion, saved note and draft survive reorder');
    click('Help');
    await settle();
    assert(document.body.textContent.includes('Save a note, complete a task, and prioritize the release.'), 'conditional help appears');
    assert(network.length === 0, 'no WebSocket, fetch or XHR dependency');
    assert(errors.length === 0, 'no browser errors: ' + errors.join('; '));
    assert(!window.__jtml_static_update_plans, 'no legacy update-plan global');
    return { checks, clicks };
  };
})();
