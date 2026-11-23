const {isDeepStrictEqual} = require('node:util');

function typed(value) {
  if (typeof value === 'number') {
    const encoded = Object.is(value, -0) ? '-0' : Number.isFinite(value) ? value : String(value);
    return {kind: 'number', value: encoded};
  }
  return {kind: typeof value, value};
}
const typedRecord = record => Object.fromEntries(Object.entries(record).map(([id, value]) => [id, typed(value)]));

// Compare only the projection actually captured by the browser adapter.
function comparePhase(model, actual, previous, initial = false) {
  const checks = {};
  const equal = (name, observed, expected) => {
    checks[name] = {status: isDeepStrictEqual(observed, expected) ? 'MATCH' : 'DIVERGENCE', observed, expected};
  };
  const unverified = (name, reason) => { checks[name] = {status: 'UNVERIFIED', reason}; };
  const success = initial || model.outcome === 'commit';
  const fault = model.outcome === 'episode-error';
  const snapshot = success ? model.commit : model.lastCommit;
  equal('CSP violations', actual.csp, []);
  equal('strict profile booted', actual.strict, true);
  equal('publication status', actual.life.status, success ? 'ACTIVE' : 'FAILED');
  equal('uncaught page error count', actual.errors.length, fault ? 1 : 0);
  equal('settled DOM', actual.dom, snapshot.dom);
  if (success) {
    equal('typed source', actual.state, snapshot.state);
    equal('typed derived', actual.derived, snapshot.derived);
    if (!initial) {
      if (!isDeepStrictEqual(previous.dom, snapshot.dom)) {
        equal('changed projection observed by MutationObserver', actual.passive.at(-1), snapshot.dom);
      }
      if (actual.passive.length) {
        equal('no observed intermediate passive DOM snapshot',
          actual.passive.filter(sample => !isDeepStrictEqual(sample, snapshot.dom)), []);
      } else {
        unverified('no observed intermediate passive DOM snapshot', 'No passive DOM callback delivered this episode.');
      }
    }
  } else {
    equal('failed partial source', actual.partial, fault ? model.failedPartial : previous.partial);
    equal('unpublished failure preserves publication revision', actual.life.revision, previous.life.revision);
    equal('no pending writes after failure', actual.life.pendingWrites, []);
    equal('no observed failed DOM publication', actual.passive, []);
  }

  if (fault) {
    const faultCodes = {E_SCALAR_DOMAIN: 'E_NON_FINITE_ARITHMETIC'};
    equal('fault code', actual.life.fault?.code, faultCodes[model.code]);
    const effect = model.events.findLast(event => event.kind === 'effect-enter');
    const expectedInvocation = effect && model.phase === effect.effect
      ? {kind: 'effect', dependency: effect.watch}
      : {kind: 'root', action: model.action};
    const {kind, dependency, action} = actual.life.fault?.failedInvocation || {};
    equal('fault phase', kind === 'effect' ? {kind, dependency: 'main::' + dependency}
      : {kind, action: 'main::' + action}, expectedInvocation);
    unverified('derived state after fault', 'The failed partial diagnostic exposes source cells only.');
  }
  if (model.outcome === 'dispatch-rejected') {
    equal('next entry preserved fault', actual.life.fault, previous.life.fault);
    unverified('dispatch result from UI', 'A click has no direct return value. The comparison checks all observable non-execution witnesses.');
  }

  const events = model.events.flatMap(event => {
    if (event.kind === 'show') return [{kind: 'show', text: event.text}];
    if (event.kind === 'effect-enter') return [{kind: 'effect-enter', effect: event.effect, scalar: event.scalar}];
    return [];
  });
  equal('show and effect-entry order', actual.events.map(({dom, ...event}) => event), events);
  if (!initial) equal('DOM during shows and effect entries', actual.events.map(event => event.dom),
    events.map(() => previous.dom));
  unverified('show scalar type and statement identity', 'Console output is text and carries no statement identity.');
  unverified('notification and effect exit', 'The effect body accessor samples entry and watch value, with no direct delivery or exit hook.');
  unverified('logical commit sequence and full atomicity', 'Post-entry snapshots and sampled DOM do not expose logical commit markers or every observer/paint.');
  return checks;
}

module.exports = {comparePhase, typed, typedRecord};
