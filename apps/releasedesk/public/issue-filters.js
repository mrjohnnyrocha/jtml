// ReleaseDesk owns filter controls and row visibility. JTML owns the API/list.
(() => {
  const browser = document.querySelector('[data-issue-browser]');
  if (!browser) return;
  const controls = browser.querySelector('[data-issue-filters]');
  const query = controls.querySelector('[data-issue-query]');
  const severity = controls.querySelector('[data-issue-severity]');
  const results = browser.querySelector('[data-issue-results]');
  const status = browser.querySelector('[data-filter-status]');
  const noResults = browser.querySelector('[data-no-results]');
  const clear = controls.querySelector('[data-clear-filters]');
  const normalize = text => text.trim().toLowerCase();

  function readURL() {
    const parameters = new URL(window.location.href).searchParams;
    query.value = parameters.get('q') || '';
    const selected = parameters.get('severity') || '';
    severity.value = [...severity.options].some(option => option.value === selected) ? selected : '';
  }

  function applyFilters() {
    const search = normalize(query.value);
    const selectedSeverity = severity.value;
    const rows = [...results.querySelectorAll('[data-testid="issue"]')];
    let visible = 0;
    for (const row of rows) {
      const title = normalize(row.getAttribute('data-issue-title') || '');
      const description = normalize(row.querySelector('.description')?.textContent || '');
      const matchesText = !search || title.includes(search) || description.includes(search);
      const matchesSeverity = !selectedSeverity || row.getAttribute('data-issue-severity') === selectedSeverity;
      const matches = matchesText && matchesSeverity;
      row.hidden = !matches;
      if (matches) visible += 1;
    }
    const loaded = !!results.querySelector('.issues, [data-testid="empty-state"]');
    const summary = visible + ' of ' + rows.length + ' saved issues shown';
    if (status.textContent !== summary) status.textContent = summary;
    status.hidden = !loaded;
    noResults.hidden = !loaded || rows.length === 0 || visible > 0;
    clear.disabled = query.value === '' && selectedSeverity === '';
    const url = new URL(window.location.href);
    if (query.value) url.searchParams.set('q', query.value);
    else url.searchParams.delete('q');
    if (selectedSeverity) url.searchParams.set('severity', selectedSeverity);
    else url.searchParams.delete('severity');
    if (url.href !== window.location.href) window.history.replaceState(window.history.state, '', url);
  }

  controls.addEventListener('submit', event => event.preventDefault());
  query.addEventListener('input', applyFilters);
  severity.addEventListener('change', applyFilters);
  clear.addEventListener('click', () => {
    query.value = '';
    severity.value = '';
    applyFilters();
    query.focus();
  });
  window.addEventListener('popstate', () => {
    readURL();
    applyFilters();
  });
  // A confirmed GET/POST may replace the rendered list while filters are active.
  new MutationObserver(applyFilters).observe(results, {
    childList: true,
    subtree: true,
    characterData: true,
    attributes: true,
    attributeFilter: ['data-issue-title', 'data-issue-severity'],
  });
  readURL();
  applyFilters();
  controls.hidden = false;
})();
