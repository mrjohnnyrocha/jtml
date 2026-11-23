const assert = require('node:assert/strict');
const {test} = require('node:test');
const {comparePhase, typed} = require('./episode_compare.cjs');
const {loadFixtures, verifySource} = require('./episode_fixtures.cjs');

test('reviewed fixture digests and original scenario inventory', () => {
  const fixtures = loadFixtures();
  assert.equal(fixtures.filter(fixture => /^V/.test(fixture.id)).length, 17);
  assert.equal(fixtures.filter(fixture => fixture.research).length, 1);
  const {source, digest} = fixtures.find(fixture => fixture.id === 'V19');
  assert.throws(() => verifySource(Buffer.concat([source, Buffer.from('\n')]), digest), /source bytes changed/);
});

test('comparator catches typed, ordered and passive trace differences', () => {
  const snapshot = {state: {x: typed(-0)}, derived: {}, dom: ['end']};
  const entry = id => ({kind: 'effect-enter', effect: id, scalar: typed(1)});
  const model = {outcome: 'commit', commit: snapshot, events: [entry('E1'), entry('E2')]};
  const actual = {...snapshot, csp: [], strict: true, life: {status: 'ACTIVE'}, errors: [],
    events: [entry('E2'), entry('E1')].map(event => ({...event, dom: ['start']})),
    state: {x: typed(0)}, passive: [['intermediate'], ['end']]};
  const result = comparePhase(model, actual, {dom: ['start']});
  for (const name of ['typed source', 'show and effect-entry order', 'no observed intermediate passive DOM snapshot']) {
    assert.equal(result[name].status, 'DIVERGENCE', name);
  }
  assert.equal(result['settled DOM'].status, 'MATCH');
  assert.equal(result['logical commit sequence and full atomicity'].status, 'UNVERIFIED');
  assert.notDeepEqual(typed(true), typed(1));
  assert.notDeepEqual(typed(Infinity), typed(null));
});
