// Browser-only regression scenario. Loaded before the generated application assets.
(() => {
  const network = [];
  const errors = [];
  const routeChanges = [];
  let routesReady = 0;
  window.addEventListener('error', event => errors.push(event.error && event.error.stack || event.message));
  window.addEventListener('unhandledrejection', event => errors.push(String(event.reason)));
  document.addEventListener('jtml:routes-ready', () => { routesReady += 1; });
  document.addEventListener('jtml:route-change', event => {
    routeChanges.push(event.detail && event.detail.route && event.detail.route.path || '');
  });
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
      if (regression === 'conditional-lifecycle') {
        const owner = document.querySelector('.conditional-owner');
        const counters = () => Array.from(owner.querySelectorAll('.counter'));
        const wrappers = () => counters().map(el => el.closest('[data-jtml-direct-instance]'));
        // show emits a body-node span, whose sibling p carries the parameter.
        const count = el => el.querySelector('button').nextElementSibling.textContent.trim();
        const registry = () => window.jtml.getRuntimeRegistrySnapshot();
        const activeIds = () => Object.keys(registry().dynamicGenerations);
        let mounted = counters();
        let mountedWrappers = wrappers();
        let initialRegistry = JSON.stringify(registry());
        assert(mounted.length === 2 && mounted.every(el => count(el) === '0'), 'two interactive children mount with independent local state');
        assert(mountedWrappers.every(el => activeIds().includes(el.getAttribute('data-jtml-direct-instance'))), 'connected children own live registry generations');
        click('Increment', mounted[0]);
        click('Increment', mounted[1]);
        await settle();
        assert(mounted.every(el => count(el) === '1'), 'nested actions dispatch exactly once');
        let first = 1, second = 1;
        for (let i = 0; i < 3; i += 1) {
          click('Mutate', owner);
          await settle();
          assert(counters().every((el, j) => el === mounted[j]) && wrappers().every((el, j) => el === mountedWrappers[j]), 'same branch preserves child DOM and instance identity after unrelated write');
          assert(JSON.stringify(registry()) === initialRegistry, 'same branch retains registry ownership without generations or handler leaks');
          click('Increment', mounted[0]); first += 1;
          await settle();
          assert(count(mounted[0]) === String(first) && count(mounted[1]) === String(second), 'retained controls remain functional and sibling state stays isolated');
        }
        for (let value = 1; value <= 2; value += 1) {
          click('Relevant', owner);
          await settle();
          assert(owner.querySelector('.consumed').textContent === String(value) && mounted[1].querySelector('.value').textContent === String(value), 'still-active branch updates consumed text, attributes and nested parameters');
          assert(counters().every((el, j) => el === mounted[j]) && mounted[1].closest('[title]').getAttribute('title') === String(value), 'relevant write retains mounted children while patching ancestor attributes');
          click('Increment', mounted[1]); second += 1;
          await settle();
          assert(count(mounted[0]) === String(first) && count(mounted[1]) === String(second) && JSON.stringify(registry()) === initialRegistry, 'relevant writes preserve local state and active handlers');
        }
        for (let cycle = 0; cycle < 2; cycle += 1) {
          const removed = mountedWrappers.slice();
          const removedIds = removed.map(el => el.getAttribute('data-jtml-direct-instance'));
          click('Hide', owner);
          await settle();
          assert(counters().length === 0 && removed.every(el => !el.isConnected), 'true to false unmounts interactive descendants');
          const destroyed = window.jtml.directComponentDestroyLifecycle || {};
          assert(removedIds.every(id => !activeIds().includes(id) && (destroyed.entries || []).some(entry => entry.id === id && entry.r && entry.s && entry.e && entry.d)), 'unmounted descendants release state handlers and registry ownership');
          click('Reveal', owner);
          await settle();
          mounted = counters(); mountedWrappers = wrappers();
          assert(mounted.length === 2 && mounted.every(el => count(el) === '0') && mountedWrappers.every(el => !removed.includes(el)), 'false to true recreates fresh component state and DOM');
          initialRegistry = JSON.stringify(registry());
          assert(activeIds().length === 2 && mountedWrappers.every(el => activeIds().includes(el.getAttribute('data-jtml-direct-instance'))), 'new connected descendants own exactly their live generations');
          click('Increment', mounted[0]); click('Increment', mounted[1]);
          await settle();
          assert(mounted.every(el => count(el) === '1'), 'recreated child actions dispatch once after repeated transitions');
          click('Mutate', owner);
          await settle();
          assert(counters().every((el, j) => el === mounted[j]) && mounted.every(el => count(el) === '1') && JSON.stringify(registry()) === initialRegistry, 'repeated invalidations retain recreated state and ownership');
        }
        assert(!window.jtml.directComponentPatchFallback && !window.jtml.directComponentBodyPlanFallback && !window.jtml.directComponentStaticUpdateError, 'conditional lifecycle uses static production paths without fallback');
      } else if (regression === 'owned-loop-writes') {
        const owned = document.querySelector('.owned-collection');
        for (const [label, expected] of [['Owned mutate', '2'], ['Owned index', '3'], ['Owned mutate', '5']]) {
          click(label, owned);
          await settle();
          assert(owned.firstElementChild.textContent.trim() === expected, 'owned iterator aliases and supported dynamic indices remain functional: ' + label);
        }
        assert(!window.jtml.directComponentPatchFallback && !window.jtml.directComponentBodyPlanFallback && !window.jtml.directComponentStaticUpdateError, 'owned loop writes use static production paths without fallback');
      } else if (regression === 'routing') {
        assert(errors.length === 0, 'routing starts without initialization errors');
        const route = path => document.querySelector('[data-jtml-route="' + path + '"]');
        const routeNodes = Array.from(document.querySelectorAll('[data-jtml-route]'));
        const componentNodes = routeNodes.map(el => el.querySelector('[data-jtml-instance]'));
        const registry = window.jtml.getRuntimeRegistrySnapshot();
        const waitForRoute = async path => {
          for (let attempt = 0; attempt < 50; attempt += 1) {
            const current = window.jtml.getCurrentRoute();
            if (current && current.path === path) return;
            await new Promise(resolve => setTimeout(resolve, 10));
          }
          throw new Error('Route did not settle: ' + path);
        };
        assert(routesReady === 1 && routeChanges.join(',') === '/', 'router initializes exactly once');
        assert(route('/').getClientRects().length > 0, 'home route initially visible');
        click('Visit', route('/'));
        await settle();
        assert(route('/').textContent.includes('1'), 'route-local action updates home state');
        const aboutLink = document.querySelector('[data-jtml-href="#/about"]');
        const tasksLink = document.querySelector('[data-jtml-href="#/"]');
        assert(aboutLink && tasksLink && !document.querySelector('a[to]'), 'route links use generated hash metadata');
        aboutLink.click();
        clicks += 1;
        await waitForRoute('/about');
        assert(route('/about').getClientRects().length > 0 && route('/').getClientRects().length === 0 && aboutLink.classList.contains('active'), 'about route navigation');
        assert(window.jtml.getCurrentRoute().name === 'About', 'about route metadata is current');
        tasksLink.click();
        clicks += 1;
        await waitForRoute('/');
        assert(route('/').getClientRects().length > 0 && tasksLink.classList.contains('active'), 'return to home route');
        history.back();
        await waitForRoute('/about');
        assert(route('/about').getClientRects().length > 0, 'browser back restores about route');
        history.back();
        await waitForRoute('/');
        assert(route('/').getClientRects().length > 0, 'browser back restores initial home route');
        history.forward();
        await waitForRoute('/about');
        assert(route('/about').getClientRects().length > 0, 'browser forward restores about route');
        location.hash = '#/';
        await waitForRoute('/');
        assert(route('/').getClientRects().length > 0, 'explicit hash change restores home route');
        location.hash = '#/missing';
        await waitForRoute('/missing');
        assert(route('/').getClientRects().length > 0 && route('/about').getClientRects().length === 0, 'unknown route deterministically falls back to home');
        const unknown = window.jtml.getCurrentRoute();
        assert(unknown.path === '/missing' && unknown.pattern === '' && unknown.name === '', 'unknown route publishes unmatched metadata');
        location.hash = '#/';
        await waitForRoute('/');
        assert(route('/').getClientRects().length > 0, 'home route recovers after unknown route');

        if (window.jtmlLoadDirectRoute) {
          // CDP-injected documents have an opaque origin. Use a fresh document
          // with its initial hash set before the generated app assets execute.
          const direct = await window.jtmlLoadDirectRoute('#/about');
          assert(direct.aboutVisible && direct.homeHidden && direct.route.name === 'About' &&
            direct.route.path === '/about' && direct.startup.hash === '#/about' &&
            direct.startup.ready === 1 && direct.startup.paths.join(',') === '/about' &&
            direct.startup.errors.length === 0, 'direct valid route load resolves about');
        } else {
          // Real HTTP documents (the native browser driver) retain navigation coverage.
          const directFrame = document.createElement('iframe');
          directFrame.hidden = true;
          let timeout;
          try {
            const directLoad = new Promise((resolve, reject) => {
              timeout = setTimeout(() => reject(new Error('Direct route load timed out')), 3000);
              directFrame.addEventListener('load', resolve, { once: true });
            });
            directFrame.src = 'index.html#/about';
            document.body.appendChild(directFrame);
            await directLoad;
            await settle();
            const directDocument = directFrame.contentDocument;
            if (!directDocument) throw new Error('Direct route document is not same-origin');
            const directAbout = directDocument.querySelector('[data-jtml-route="/about"]');
            const directHome = directDocument.querySelector('[data-jtml-route="/"]');
            const directRoute = directFrame.contentWindow.jtml.getCurrentRoute();
            assert(directAbout && !directAbout.hidden && directHome.hidden && directRoute.name === 'About', 'direct valid route load resolves about');
          } finally {
            clearTimeout(timeout);
            directFrame.remove();
          }
        }

        click('Visit', route('/'));
        await settle();
        assert(route('/').textContent.includes('2'), 'retained route handler and state remain active');
        const finalRegistry = window.jtml.getRuntimeRegistrySnapshot();
        assert(routeNodes.every((el, index) => el.isConnected && el === document.querySelectorAll('[data-jtml-route]')[index]) &&
          componentNodes.every(el => el && el.isConnected) && route('/').textContent.includes('2') &&
          JSON.stringify(finalRegistry) === JSON.stringify(registry),
          'route transitions preserve component identity without handler or state leaks');
        assert(routeChanges.join(',') === '/,/about,/,/about,/,/about,/,/missing,/',
          'route transitions emit exactly one lifecycle event each');
      } else if (regression === 'conditional-reactivity') {
        const boards = Array.from(document.querySelectorAll('.board'));
        const probes = boards.map(board => board.querySelector('.probe'));
        const retained = Array.from(document.querySelectorAll('[data-jtml-direct-instance], .probe, h2'));
        const ids = retained.map(el => el.getAttribute('data-jtml-direct-instance'));
        const registry = JSON.stringify(window.jtml.getRuntimeRegistrySnapshot());
        const state = [{ count: 0, score: 0, title: "Before", enabled: false, fallback: "Fallback 0", offset: 1, limit: 3, items: [0, 3], index: 0, alias: 0 }, { count: 0, score: 0, title: "Before", enabled: false, fallback: "Fallback 0", offset: 1, limit: 3, items: [0, 3], index: 0, alias: 0 }];
        const check = (probe, model) => {
          const high = model.count >= 2;
          const value = selector => probe.querySelector(selector).textContent;
          return value('.comparison') === model.title &&
            value('.arithmetic') === (model.count + model.offset >= model.limit ? 'High' : 'Low') &&
            value('.logical') === (model.enabled && high ? 'Both' : 'Other') &&
            value('.property') === (model.score >= 2 ? 'High' : 'Low') &&
            value('.alias') === (model.alias >= 2 ? 'High' : 'Low') &&
            value('.indexed') === (model.items[model.index] >= 2 ? 'High' : 'Low') &&
            value('.derived') === (high ? 'High' : 'Low') &&
            value('.ternary') === ((model.enabled ? high : model.score >= 2) ? 'Yes' : 'No') &&
            (model.enabled ? value('.branch') === model.title :
              probe.textContent.includes(model.fallback));
        };
        assert(boards.length === 2 && probes.every((probe, index) => check(probe, state[index])),
          'conditional matrix starts with independent nested state');
        const shadowRows = document.querySelector('.shadow-rows');
        const rowValues = () => Array.from(shadowRows.querySelectorAll('[data-jtml-direct-list-key]')).map(el => el.textContent.trim());
        assert(rowValues().join(',') === '1,2', 'shadowed derived collection starts with outer observable values');
        for (const [index, label] of [[0, 'Advance'], [0, 'Advance'], [0, 'Fallback'], [0, 'Toggle'],
          [0, 'Alias'], [0, 'Alias'], [0, 'Offset'], [0, 'Limit'], [0, 'Index'], [0, 'Index'],
          [0, 'Indexed score'], [0, 'Indexed score'], [0, 'Replace items'], [0, 'Title'], [0, 'Count'], [0, 'Score'], [0, 'Toggle'], [0, 'Fallback'], [0, 'Reset'],
          [0, 'Score'], [0, 'Score'], [0, 'Count'], [0, 'Count'], [0, 'Toggle'], [0, 'Reset'],
          [1, 'Advance'], [1, 'Advance']]) {
          click(label, boards[index]);
          if (label === 'Advance') { state[index].count += 1; state[index].score += 1; state[index].title = 'After'; state[index].fallback = 'Fallback ' + state[index].count; }
          if (label === 'Alias') state[index].alias += 1;
          if (label === 'Offset') state[index].offset = 0;
          if (label === 'Limit') state[index].limit = 1;
          if (label === 'Index') state[index].index = 1 - state[index].index;
          if (label === 'Indexed score') state[index].items[state[index].index] += 1;
          if (label === 'Replace items') state[index].items = [0, 0];
          if (label === 'Count') state[index].count += 1;
          if (label === 'Score') state[index].score += 1;
          if (label === 'Title') state[index].title = 'Before';
          if (label === 'Fallback') state[index].fallback = 'Independent fallback';
          if (label === 'Toggle') state[index].enabled = !state[index].enabled;
          if (label === 'Reset') { state[index].count = 0; state[index].score = 0; state[index].title = 'Before'; state[index].fallback = 'Fallback 0'; state[index].offset = 1; state[index].limit = 3; state[index].items = [0, 3]; state[index].index = 0; }
          await settle();
          assert(probes.every((probe, sibling) => check(probe, state[sibling])),
            'conditional dependencies update comparisons arithmetic logical properties both branches and derived values: ' + index + ' ' + label + ' ' + JSON.stringify(probes.map(probe => probe.textContent)));
          assert(retained.every((el, position) => el.isConnected &&
            el === document.querySelectorAll('[data-jtml-direct-instance], .probe, h2')[position] &&
            el.getAttribute('data-jtml-direct-instance') === ids[position]) &&
            JSON.stringify(window.jtml.getRuntimeRegistrySnapshot()) === registry,
            'conditional updates preserve component and unaffected DOM identity with sibling isolation: ' + label);
          assert(!window.jtml.directComponentPatchFallback && !window.jtml.directComponentBodyPlanFallback &&
            !window.jtml.directComponentStaticUpdateError,
            'conditional updates stay on generated static paths: ' + label);
        }
        click('Replace rows', shadowRows);
        await settle();
        assert(rowValues().join(',') === '3,4' && shadowRows === document.querySelector('.shadow-rows'),
          'shadowed derived collection invalidates on outer root replacement');
        assert(!window.jtml.directComponentPatchFallback && !window.jtml.directComponentBodyPlanFallback &&
          !window.jtml.directComponentStaticUpdateError,
          'derived collection replacement stays on generated static updates');
      } else if (regression === 'nested-heading') {
        const boards = Array.from(document.querySelectorAll('.board'));
        const summaries = boards.map(board => Array.from(board.querySelectorAll('.summary')));
        const retained = Array.from(document.querySelectorAll('[data-jtml-direct-instance], .summary, .detail, h2, h3'));
        const instanceIds = retained.map(el => el.getAttribute('data-jtml-direct-instance'));
        const registry = JSON.stringify(window.jtml.getRuntimeRegistrySnapshot());
        const values = summary => [summary.querySelector('h2').textContent,
          summary.querySelector('h3').textContent, summary.getAttribute('title'),
          summary.querySelector('h2').parentElement.getAttribute('title'),
          summary.querySelector('.detail').getAttribute('title')];
        assert(boards.length === 2 && summaries.every(items => items.length === 2 &&
          values(items[0]).every(value => value === 'Before') &&
          values(items[1]).every(value => value === 'Fixed')), 'nested text and attributes start with independent parameters');
        for (const [boardIndex, label, expected] of [
          [0, 'Rename', 'After'], [0, 'Reset', 'Before'], [0, 'Rename', 'After'],
          [0, 'Finish', 'Final'], [1, 'Rename', 'After']
        ]) {
          click(label, boards[boardIndex]);
          await settle();
          const target = summaries[boardIndex][0];
          assert(values(target).every(value => value === expected),
            'nested heading updates when parameter is named title: ' + boardIndex + ' ' + label + ' ' + JSON.stringify({ values: values(target) }));
          assert(summaries.every((items, index) => values(items[1]).every(value => value === 'Fixed') &&
            (index === boardIndex || values(items[0]).every(value => value === (boardIndex === 0 ? 'Before' : 'Final')))),
            'parameter updates isolate nested siblings and sibling parents: ' + label);
          const current = Array.from(document.querySelectorAll('[data-jtml-direct-instance], .summary, .detail, h2, h3'));
          assert(current.length === retained.length && retained.every((el, index) =>
            el.isConnected && el === current[index] && el.getAttribute('data-jtml-direct-instance') === instanceIds[index]) &&
            JSON.stringify(window.jtml.getRuntimeRegistrySnapshot()) === registry,
            'repeated nested updates retain DOM, component identity and registry ownership: ' + label);
          assert(window.jtml.directNestedComponentParamPatch &&
            window.jtml.directNestedComponentParamPatch.mode === 'nested-component-static-update' &&
            !window.jtml.directComponentPatchFallback && !window.jtml.directComponentBodyPlanFallback &&
            !window.jtml.directComponentStaticUpdateError,
            'nested updates use generated static patches without fallback: ' + label);
        }
      } else if (regression === 'keyed-local-actions') {
        const initial = cards();
        const [audit, ship] = initial;
        const auditWrapper = audit && audit.closest('[data-jtml-direct-instance]');
        const shipWrapper = ship && ship.closest('[data-jtml-direct-instance]');
        const auditId = auditWrapper && auditWrapper.getAttribute('data-jtml-direct-instance');
        assert(initial.length === 2 && audit.textContent.includes('Audit') && ship.textContent.includes('Ship'),
          'two keyed component instances start with independent state');
        const auditInput = audit.querySelector('input');
        auditInput.value = 'Audit note';
        auditInput.dispatchEvent(new Event('input', { bubbles: true }));
        click('Save note', audit);
        await settle();
        assert(audit.textContent.includes('Audit note'), 'input binding and named local action update visible DOM');
        assert(ship.textContent.includes('Not saved') && !ship.textContent.includes('Audit note'),
          'local component state does not leak to keyed siblings');
        click('Reorder');
        await settle();
        const reordered = cards();
        assert(reordered[0] === ship && reordered[1] === audit &&
          ship.closest('[data-jtml-direct-instance]') === shipWrapper &&
          audit.closest('[data-jtml-direct-instance]') === auditWrapper &&
          audit.textContent.includes('Audit note'),
          'keyed reorder preserves component state and DOM identity');
        const shipInput = ship.querySelector('input');
        shipInput.value = 'Ship note';
        shipInput.dispatchEvent(new Event('input', { bubbles: true }));
        click('Save note', ship);
        await settle();
        assert(ship.textContent.includes('Ship note') && audit.textContent.includes('Audit note'),
          'local action updates the correct component after reorder');
        click('Remove audit');
        await settle();
        const remaining = cards();
        assert(remaining.length === 1 && remaining[0] === ship && !auditWrapper.isConnected,
          'keyed removal retains the remaining component and removes the deleted DOM');
        const lifecycle = window.jtml.directComponentDestroyLifecycle || {};
        const entry = (lifecycle.entries || []).find(item => item.id === auditId);
        const registry = window.jtml.getRuntimeRegistrySnapshot();
        assert(entry && entry.r && entry.s && entry.e && entry.d &&
          !Object.prototype.hasOwnProperty.call(registry.dynamicGenerations, auditId),
          'keyed removal cleans component state, handlers and registry ownership');
      } else {
        const input = document.querySelector('input');
        input.value = 'Saved note';
        input.dispatchEvent(new Event('input', { bubbles: true }));
        if (regression === 'form-submit') {
          const form = document.querySelector('form');
          const locationBeforeSubmit = location.href;
          let submitEvent;
          form.addEventListener('submit', event => { submitEvent = event; });
          assert(form.querySelector('button').type === 'submit', 'form uses a native submit button');
          click('Save note', form);
          await settle();
          assert(document.body.textContent.includes('Saved note'), 'submit button dispatches the component-local action');
          assert(submitEvent && submitEvent.defaultPrevented && location.href === locationBeforeSubmit,
            'handled form submission prevents navigation');
          assert(document.querySelector('form') === form && document.querySelector('input') === input,
            'submit patch preserves form and input identity');
          input.value = 'Keyboard note';
          input.dispatchEvent(new Event('input', { bubbles: true }));
          input.focus();
          input.setSelectionRange(4, 4);
          submitEvent = null;
          if (window.jtmlPressEnter) await window.jtmlPressEnter();
          else form.requestSubmit();
          await settle();
          assert(document.body.textContent.includes('Keyboard note'), 'Enter key submission dispatches the component-local action');
          assert(document.querySelector('form') === form && document.querySelector('input') === input &&
            document.activeElement === input && input.selectionStart === 4,
            'keyboard submit preserves input identity, focus and cursor');
        } else {
          click('Save note', cards()[0]);
          await settle();
          assert(document.body.textContent.includes('Saved note'), regression + ' saves input in visible DOM');
        }
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
