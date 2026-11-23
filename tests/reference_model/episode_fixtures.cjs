// Hand-transcribed from the source entries identified by each SHA-256 below.
// IDs are main-module declarations or action-local parameter IDs. Line numbers
// refer to exact JTML source bytes. Catalog expectations are never model inputs.
const assert = require('node:assert/strict');
const {createHash} = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const {vectors} = require('../../docs/spec/reactive-core-v1/vectors.json');

const number = value => ['literal', 'number', value];
const bool = value => ['literal', 'boolean', value];
const text = value => ['literal', 'string', value];
const read = id => ['read', id];
const binary = (op, left, right) => ['binary', op, left, right];
const add = (left, right) => binary('+', left, right);
const times = (left, right) => binary('*', left, right);
const divide = (left, right) => binary('/', left, right);
const unary = (op, expr) => ['unary', op, expr];
const decl = (kind, line, id, expr) => ({kind, line, id, expr});
const letNumber = (line, id, value) => decl('let', line, id, number(value));
const get = (line, id, expr) => decl('get', line, id, expr);
const set = (line, id, expr) => [line, ['assign', id, expr]];
const show = (line, expr) => [line, ['show', expr]];
const action = (line, id, params, ...body) => ({line, id, params, body});
const effect = (line, id, watch, ...body) => ({line, id, watch, body});
const step = (button, name = 'step', ...args) => ({button, action: name, args});
const out = (...names) => names.flatMap((name, i) => i ? [text('|'), read(name)] : [read(name)]);
const one = number('1'), zero = number('0'), two = number('2');
const valueText = [text('Value '), read('x')];

const previous = Object.fromEntries(['V01', 'V03', 'V04', 'V05'].map(id =>
  [id, require('./fixtures/' + id + '.json')]));
const cases = [
  ['V02', 'feaae4e74773348d0f9087876a6f70c80b5693d275f6cfef0558ae0bc19b9a3e', {
    declarations: [letNumber(2, 'x', '0'), letNumber(3, 'hits', '0')],
    actions: [action(7, 'step', [], set(8, 'x', one), set(9, 'x', two))],
    effects: [effect(4, 'E1', 'x', set(5, 'hits', add(read('hits'), one)), show(6, read('x')))],
    textLine: 12, text: out('x', 'hits'), steps: [step('Step')],
  }],
  ['V06', 'c938f4e762dfae6e6db1a7467e51ab11a347861905926112bfd85483de4894ef', {
    declarations: [letNumber(2, 'x', '10'), letNumber(3, 'hits', '0')],
    actions: [action(6, 'shadow', ['shadow::x'],
      set(7, 'shadow::x', add(read('shadow::x'), one)), show(8, read('shadow::x')))],
    effects: [effect(4, 'E1', 'x', set(5, 'hits', add(read('hits'), one)))],
    textLine: 11, text: out('x', 'hits'),
    steps: [step('Step', 'shadow', number('3')), step('Step', 'shadow', number('3'))],
  }],
  ['V07', '942a4a7faf89cbd2211eb699cd4ff816f9fb0754558ef742e74df1084a014c5a', {
    declarations: [letNumber(2, 'x', '1'), letNumber(3, 'hits', '0')],
    actions: [action(6, 'step', [], set(7, 'x', read('x')))],
    effects: [effect(4, 'E1', 'x', set(5, 'hits', add(read('hits'), one)))],
    textLine: 10, text: out('x', 'hits'), steps: [step('Step'), step('Step')],
  }],
  ['V08', '30ac17b962982df2de902ba4f831d5a8a7aacad278f74696f786df9e9ba5d9bc', {
    declarations: [decl('let', 2, 'enabled', bool(true)), letNumber(3, 'left', '2'), letNumber(4, 'right', '9'),
      get(5, 'chosen', ['select', read('enabled'), read('left'), read('right')])],
    actions: [action(6, 'rightWrite', [], set(7, 'right', number('11'))),
      action(8, 'leftWrite', [], set(9, 'left', number('5'))),
      action(10, 'toggle', [], set(11, 'enabled', unary('!', read('enabled'))))],
    textLine: 14, text: [read('chosen')],
    steps: [step('Right', 'rightWrite'), step('Toggle', 'toggle'), step('Left', 'leftWrite'), step('Toggle', 'toggle')],
  }],
  ['V11', '19884d6b68fdf4ae83c695471b057d7af890725adb2fbf5d9cb4082867f8d0c6', {
    declarations: [letNumber(2, 'z', '2'), decl('let', 3, 'a', add(read('z'), one)),
      get(4, 'result', add(read('a'), read('z')))],
    textLine: 7, text: out('a', 'result'),
  }],
  ['V12', 'a4e49d9cf581e8002b07130919af90271b10bdf15d6fa0fd595cf0c464198a29', {
    declarations: [decl('let', 2, 'first', add(read('later'), one)), letNumber(3, 'later', '2')],
    textLine: 6, text: [text('never')],
  }],
  ['V13', 'dbd97dfbc13186f729d28fcde69a4c904cebd260b1f923d572cd557738ff7b1f', {
    declarations: [get(2, 'a', add(read('b'), one)), get(3, 'b', add(read('a'), one))],
    textLine: 6, text: [text('never')],
  }],
  ['V14', '0f1e997166ec2cec82cbd2f8e0362da2181e92ace845b66d89755e30be23ce42', {
    declarations: [letNumber(2, 'x', '0')],
    actions: [action(3, 'step', [], set(4, 'x', read('missing')))],
    textLine: 7, text: valueText,
  }],
  ['V15', 'dce6b646d088353c76c97878544daf42c1ba4702c087bb93ccc938ee81612138', {
    declarations: [get(2, 'result', ['call', 'mystery'])], textLine: 5, text: [read('result')],
  }],
  ['V17', '18a2275e362666f5bae8ada9317ccdc09103a0f1763fcca130a0638695686921', {
    declarations: [letNumber(2, 'x', '0'), letNumber(3, 'y', '0'), letNumber(4, 'z', '0')],
    actions: [action(9, 'step', [], set(10, 'x', one))],
    effects: [effect(5, 'E1', 'x', set(6, 'y', add(read('x'), one))),
      effect(7, 'E2', 'y', set(8, 'z', add(read('y'), one)))],
    textLine: 13, text: out('x', 'y', 'z'), steps: [step('Step')],
  }],
  ['V18', '567056d0c4ff31e8f7403c510598efdf784f152e51864b593a3cc0dfd28cefbe', {
    declarations: [letNumber(2, 'x', '0'), letNumber(3, 'y', '0')],
    effects: [effect(4, 'E1', 'x', set(5, 'y', add(read('y'), one))),
      effect(6, 'E2', 'y', set(7, 'x', add(read('x'), one)))],
    textLine: 10, text: [text('never')],
  }],
  ['V19', 'ae2be1e10f9c7411467128c83b6837489d6d0415046b05cb844b1599152267ee', {
    declarations: [letNumber(2, 'x', '1')],
    actions: [action(3, 'step', [], set(4, 'x', divide(one, zero)))],
    textLine: 7, text: valueText, steps: [step('Step'), step('Step')],
  }],
  ['V20', 'd09f1b52fcad7a3f9c175a5f401d5473d7fa54a8ab03cf57f9832fd736d4566f', {
    declarations: [letNumber(2, 'x', '0'), letNumber(3, 'seen', '0'), get(4, 'twice', times(read('x'), two))],
    actions: [action(7, 'step', [], set(8, 'x', one), show(9, read('twice')), set(10, 'x', two), show(11, read('twice')))],
    effects: [effect(5, 'E1', 'x', set(6, 'seen', read('twice')))],
    textLine: 14, text: out('x', 'twice', 'seen'), steps: [step('Step')],
  }],
  ['typed-order', '7b76b9f624150008ebdf3d130826b6c2d594f44a3f4250caf5d63920bb1451dd', {
    declarations: [letNumber(2, 'x', '1'), letNumber(3, 'y', '0'), decl('let', 4, 'active', bool(false)),
      decl('let', 5, 'word', text('old')),
      get(6, 'chosen', binary('||', read('active'), binary('>', divide(one, read('x')), zero)))],
    effects: [effect(7, 'E1', 'y', show(8, text('downstream'))),
      effect(9, 'E2', 'x', set(10, 'y', add(read('x'), one)), show(11, text('writer'))),
      effect(12, 'E3', 'x', show(13, read('y')))],
    actions: [action(14, 'step', ['step::n'], [15, ['if', binary('==', read('step::n'), zero),
      [show(16, unary('-', read('step::n'))), set(17, 'active', bool(true)),
        set(18, 'word', add(text('new'), text('!'))), show(19, read('chosen'))],
      [set(21, 'active', bool(false))]]], set(22, 'x', read('step::n')))],
    textLine: 25, text: out('word', 'chosen', 'x', 'y'), steps: [step('Step', 'step', zero)],
  }],
  ['failure-prefix', 'c932313a35ab2d3880ca29d61290827068351aa3cb6a4bb522415b8ce374b475', {
    declarations: [letNumber(2, 'x', '0'), letNumber(3, 'y', '0'), letNumber(4, 'stopped', '0'),
      decl('let', 5, 'recovered', bool(false)), get(6, 'doubled', times(read('y'), two))],
    effects: [effect(7, 'E1', 'x', set(8, 'y', two), show(9, read('doubled'))),
      effect(10, 'E2', 'y', show(11, text('failing')), set(12, 'stopped', divide(one, zero))),
      effect(13, 'E3', 'y', show(14, text('downstream')))],
    actions: [action(15, 'step', [], set(16, 'x', one), show(17, read('x'))),
      action(18, 'recover', [], set(19, 'recovered', bool(true)), show(20, text('recovered')))],
    textLine: 23, text: out('x', 'doubled', 'stopped', 'recovered'), steps: [step('Step'), step('Recover', 'recover')],
  }],
  ['derived-repair', 'a936a367fe01f6e1588f3c55819138f68547e76e0bab8f0fdbcd1a5a1ef458da', {
    declarations: [letNumber(2, 'trigger', '1'), letNumber(3, 'divisor', '1'),
      get(4, 'quotient', divide(one, read('divisor')))],
    effects: [effect(5, 'E1', 'trigger', show(6, text('repair')), set(7, 'divisor', two))],
    actions: [action(8, 'step', [], set(9, 'trigger', two), set(10, 'divisor', zero))],
    textLine: 13, text: out('quotient', 'divisor'), steps: [step('Step')],
  }],
];

function verifySource(bytes, digest) {
  assert.equal(createHash('sha256').update(bytes).digest('hex'), digest, 'Reviewed source bytes changed');
}

function sourceAST(program, source) {
  const lines = source.toString('utf8').split('\n');
  const at = line => {
    assert(lines[line - 1]?.trim(), 'Missing AST source line ' + line);
    return {file: 'index.jtml', line, column: lines[line - 1].search(/\S/) + 1};
  };
  const binding = id => 'main::' + id;
  const expression = ([tag, first, second, third]) => {
    switch (tag) {
      case 'literal': return {literal: {kind: first, value: second}};
      case 'read': return {read: binding(first)};
      case 'call': return {call: first};
      case 'unary': return {unary: first, operand: expression(second)};
      case 'binary': return {binary: first, left: expression(second), right: expression(third)};
      case 'select': return {select: expression(first), then: expression(second), else: expression(third)};
      default: throw Error('Unknown fixture expression ' + tag);
    }
  };
  const body = statements => statements.map(([line, [tag, first, second, third]]) => {
    const location = {source: at(line)};
    switch (tag) {
      case 'assign': return {...location, assign: binding(first), expression: expression(second)};
      case 'show': return {...location, show: expression(first)};
      case 'if': return {...location, if: expression(first), then: body(second), else: body(third)};
      default: throw Error('Unknown fixture statement ' + tag);
    }
  });
  return {
    schema_version: 1, scope: 'main',
    declarations: program.declarations.map(({kind, line, id, expr}) =>
      ({kind, source: at(line), id: binding(id), name: id, expression: expression(expr)})),
    actions: (program.actions || []).map(({id, line, params, body: statements}) =>
      ({id: binding(id), name: id, source: at(line), params: params.map(binding), body: body(statements)})),
    effects: (program.effects || []).map(({id, watch, line, body: statements}) =>
      ({id, watch: binding(watch), source: at(line), body: body(statements)})),
    observers: [{id: '.out', segments: program.text.map(expression), source: at(program.textLine)}],
    steps: (program.steps || []).map(({button, action, args}) =>
      ({button, action: binding(action), args: args.map(expression)})),
  };
}

function loadFixtures() {
  const reviewed = [...cases, ...Object.entries(previous).map(([id, fixture]) => [id, fixture.source.sha256])];
  return reviewed.sort(([left], [right]) => left.localeCompare(right)).map(([id, digest, compact]) => {
    const original = vectors.find(vector => vector.id === id);
    if (original) {
      assert.equal(original.entry, 'index.jtml');
      assert.deepEqual(Object.keys(original.files), ['index.jtml']);
    }
    const source = original ? Buffer.from(original.files[original.entry], 'utf8')
      : fs.readFileSync(path.join(__dirname, '../fixtures/core_reference', id + '.jtml'));
    verifySource(source, digest);
    const existing = previous[id];
    if (existing) assert.equal(existing.source.text, source.toString('utf8'));
    const ast = compact ? sourceAST(compact, source) : {
      ...existing.program, effects: [], steps: existing.steps.map(({action, args}) => {
        assert.deepEqual(args, []);
        return {button: 'Step', action, args};
      }),
    };
    const {steps, ...program} = ast;
    if (original) {
      // The extra V19 click is labeled as a post-fault probe. All preceding
      // entries are the original authored scenario, with unchanged arguments.
      assert.equal(steps.length, original.steps.length + (id === 'V19' ? 1 : 0));
      assert.deepEqual(steps.slice(0, original.steps.length).map(({action, args}) =>
        ({action, args: args.map(arg => Number(arg.literal.value))})),
            original.steps.map(({action, args}) => ({action, args})));
    }
    return {id, digest, source, research: id === 'derived-repair', steps,
      program: {...program, profile: 'strict-root-scalar-episodes-v1'}};
  });
}

module.exports = {loadFixtures, verifySource};
