// New expectations are authored arithmetic/state tables. Neither compiler plans
// nor either backend supplies the oracle. Original cases.cjs remains unchanged.
// Explicit true/false writes isolate dependency collection from the separately
// catalogued native !/&& boolean-value construction defect. The old logical
// fixtures and their independent expectations remain mandatory known failures.
const {scalar,action}=require('./cases.cjs');
const fs=require('node:fs'),path=require('node:path');
const checks=(text,reads)=>[{text,reads,irReads:reads}];
const cases=[
 scalar('ternary-nested-branches',
  ['let enabled = true','let inner = false','let x = 2','let y = 4','let z = 9','get result = (enabled ? ((inner ? (x) : (y))) : (z))'],
  [['x',['x += 1']],['y',['y += 1']],['z',['z += 1']],['outerOff',['enabled = false']],['outerOn',['enabled = true']],['innerOn',['inner = true']],['innerOff',['inner = false']]],
  ['result'],['x','innerOn','z','outerOff','y','innerOff','outerOn','y'],
  [4,4,3,3,10,10,10,5,6].map(result=>({result})),
  checks('get result = (enabled ? ((inner ? (x) : (y))) : (z))',['enabled','inner','x','y','z'])),
 scalar('ternary-computed-index',
  ['let enabled = true','let index = 0','let other = 1','let items = [2, 5]','get result = items[(enabled ? index : other)]'],
  [['index',['index = 1 - index']],['other',['other = 1 - other']],['disable',['enabled = false']],['enable',['enabled = true']],['write',['items[other] += 1']]],
  ['result'],['index','disable','other','write','enable','index'],
  [2,5,5,2,3,5,3].map(result=>({result})),
  checks('get result = items[(enabled ? index : other)]',['items','enabled','index','other'])),
 scalar('ternary-owned-member-alias',
  ['let enabled = true','let item = { left: 2, right: 9 }','let alias = item','get result = enabled ? alias.left : item.right'],
  [['left',['item.left += 1']],['right',['item.right += 2']],['disable',['enabled = false']],['enable',['enabled = true']]],
  ['result'],['left','right','disable','left','right','enable'],
  [2,3,3,11,11,13,4].map(result=>({result})),
  checks('get result = enabled ? alias.left : item.right',['enabled','alias','item'])),
 scalar('ternary-indexed-branches',
  ['let enabled = true','let index = 0','let items = [2, 5]','let fallback = [7, 9]','get result = (enabled ? items[(index)] : fallback[(1 - index)])'],
  [['index',['index = 1 - index']],['disable',['enabled = false']],['enable',['enabled = true']],['write',['fallback[1 - index] += 1']]],
  ['result'],['index','write','disable','write','index','enable'],
  [2,5,5,8,9,9,2].map(result=>({result})),
  checks('get result = (enabled ? items[(index)] : fallback[(1 - index)])',['enabled','items','index','fallback'])),
 scalar('ternary-derived-chain',
  ['let enabled = true','let x = 2','let y = 9','get choice = enabled ? x : y','get result = choice * 2'],
  [['left',['x += 1']],['right',['y += 1']],['disable',['enabled = false']],['enable',['enabled = true']]],
  ['choice','result'],['left','right','disable','right','enable'],
  [2,3,3,10,11,3].map(choice=>({choice,result:choice*2})),
  [...checks('get choice = enabled ? x : y',['enabled','x','y']),...checks('get result = choice * 2',['choice'])]),
 scalar('ternary-boolean-condition',
  ['let x = 1','let y = 3','let enabled = true','get result = ((x >= 2) ? ((y < 5) ? (enabled ? 7 : 9) : 1) : 1)'],
  [['x',['x += 1']],['raise',['y = 6']],['lower',['y = 3']],['disable',['enabled = false']],['enable',['enabled = true']]],
  ['result'],['x','disable','raise','enable','x','lower'],
  [1,7,9,1,1,1,7].map(result=>({result})),
  checks('get result = ((x >= 2) ? ((y < 5) ? (enabled ? 7 : 9) : 1) : 1)',['x','y','enabled'])),
];
const siblings=scalar('ternary-sibling-isolation',
 ['let x = 1','let enabled = true','get result = enabled ? x : 0'],
 [['step',['x += 1']],['disable',['enabled = false']],['enable',['enabled = true']]], ['x','result'],[],[],
 checks('get result = enabled ? x : 0',['enabled','x']));
siblings.source=siblings.source.replace('page\n  Probe\n','page\n  Probe\n  Probe\n');
siblings.steps=[action('step'),action('disable',1),action('step',1),action('enable',1),action('step')];
siblings.expected=[
 [{x:1,result:1},{x:1,result:1}], [{x:2,result:2},{x:1,result:1}],
 [{x:2,result:2},{x:1,result:0}], [{x:2,result:2},{x:2,result:0}],
 [{x:2,result:2},{x:2,result:2}], [{x:3,result:3},{x:2,result:2}],
];
cases.push(siblings);

// String parameters have the same observable interface in both backends;
// numeric parameter scope representation is not part of this bounded comparison.
const parameters={...siblings,id:'ternary-parameter-isolation',
 source:siblings.source.replace('make Probe\n','make Probe initial\n').replace('let x = 1','let x = initial').replace('x += 1','x = x + "!"').replace('get result = enabled ? x : 0','get result = enabled ? x : initial').replace('page\n  Probe\n  Probe\n','page\n  Probe "left"\n  Probe "right"\n'),
 checks:checks('get result = enabled ? x : initial',['enabled','x','initial']),
 expected:[
  [{x:'left',result:'left'},{x:'right',result:'right'}],
  [{x:'left!',result:'left!'},{x:'right',result:'right'}],
  [{x:'left!',result:'left!'},{x:'right',result:'right'}],
  [{x:'left!',result:'left!'},{x:'right!',result:'right'}],
  [{x:'left!',result:'left!'},{x:'right!',result:'right!'}],
  [{x:'left!!',result:'left!!'},{x:'right!',result:'right!'}],
 ]};
cases.push(parameters);

for(const fixture of cases) {
 fixture.authoredSource=path.join(__dirname,'fixtures/ternary',fixture.id+'.jtml');
 const authored=fs.readFileSync(fixture.authoredSource,'utf8');
 if(authored!==fixture.source) throw new Error('ternary fixture/oracle source drift: '+fixture.id);
 fixture.source=authored;
}
module.exports=cases;
