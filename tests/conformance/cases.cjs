// Expected values are authored independently; this file never reads a compiler plan.
const fs = require('node:fs');
const path = require('node:path');
const fixtureRoot = path.join(__dirname, 'fixtures');
const action = (name, ordinal = 0) => ({component:'Probe', action:name, ordinal});
const state = (x,y,result) => ({x,y,result});
function scalar(id, declarations, actions, fields, steps, expected, checks = []) {
  const source = `jtml 2\n\nmake Probe\n${declarations.map(s=>'  '+s).join('\n')}\n${actions.map(([name,body])=>`  when ${name}\n${body.map(s=>'    '+s).join('\n')}`).join('\n')}\n  box class "probe"\n${fields.map(f=>`    p class "out-${f}" ${f}`).join('\n')}\n${actions.map(([name])=>`    button "${name}" click ${name}`).join('\n')}\n\npage\n  Probe\n`;
  return {id,source,fields,steps:steps.map(s=>typeof s==='string'?action(s):s),expected:expected.map(v=>[v]),checks};
}
const cases = [
 {id:'arithmetic-order-isolation', source:fs.readFileSync(path.join(fixtureRoot,'arithmetic.jtml'),'utf8'),
  fields:['x','y','result'], steps:[action('step'),action('step'),action('step',1)],
  expected:[[state(2,3,8),state(2,3,8)],[state(3,6,15),state(2,3,8)],[state(4,10,24),state(2,3,8)],[state(4,10,24),state(3,6,15)]],
  checks:[{text:'get result = x + y * 2',reads:['x','y'],irReads:['x','y']},{text:'y = y + x',reads:['y','x'],writes:['y']}]},
 scalar('branch-dependencies', ['let x = 2','let y = 9','let enabled = true','get result = enabled ? x : y'],
  [['left',['x += 3']],['right',['y += 2']],['toggle',['enabled = !enabled']]],
  ['x','y','result'], ['right','left','toggle','left','right','toggle'],
  [state(2,9,2),state(2,11,2),state(5,11,5),state(5,11,11),state(8,11,11),state(8,13,13),state(8,13,8)],
  [{text:'get result = enabled ? x : y',reads:['enabled','x','y'],irReads:['enabled','x','y']}]),
 scalar('boolean-comparison', ['let x = 0','let y = 3','get result = (x >= 2 && y < 5) ? "yes" : "no"'],
  [['step',['x += 1']],['raise',['y += 3']]], ['x','y','result'],['step','step','raise','step'],
  [state(0,3,'no'),state(1,3,'no'),state(2,3,'yes'),state(2,6,'no'),state(3,6,'no')],
  [{text:'get result = (x >= 2 && y < 5) ? "yes" : "no"',reads:['x','y'],irReads:['x','y']}]),
 scalar('conditional-action-order',['let x = 1','let y = 0','get result = x + y'],
  [['step',['if x < 3','  x += 1','  y = x * 2','else','  y += 1']]], ['x','y','result'],['step','step','step'],
  [state(1,0,1),state(2,4,6),state(3,6,9),state(3,7,10)], [{text:'if x < 3',reads:['x'],irReads:['x']}]),
 scalar('owned-member-alias',['let item = { "score": 1 }','let alias = item','get result = alias.score + item.score'],
  [['step',['item.score += 1']]], ['result'],['step','step'],[{result:2},{result:4},{result:6}],
  [{text:'get result = alias.score + item.score',reads:['alias','item'],irReads:['alias','item']}]),
 scalar('indexed-dependencies',['let items = [2, 5]','let index = 0','get result = items[index] * 2'],
  [['toggle',['index = 1 - index']],['step',['items[index] += 1']]], ['result'],['step','toggle','step','toggle'],
  [{result:4},{result:6},{result:10},{result:12},{result:6}],
  [{text:'get result = items[index] * 2',reads:['items','index'],irReads:['items','index']}]),
 scalar('string-literal-fidelity',['let x = "é <tag> & \\\"quote\\\""','get result = x + "!"'],
  [['step',['x = "next\\nline"']]], ['x','result'],['step'],
  [{x:'é <tag> & "quote"',result:'é <tag> & "quote"!'},{x:'next\nline',result:'next\nline!'}]),
];
// A bounded algebra grammar: (x + a) * b or (x - a) * b. Values stay exact
// IEEE-754 integers. The oracle is ordinary integer arithmetic, not JTML eval.
function generated(seed=24, count=8) {
 let random=seed>>>0;
 const next=()=>{random^=random<<13;random^=random>>>17;random^=random<<5;return random>>>0;};
 return Array.from({length:count},(_,index)=>{
  const descriptor={x:next()%7-3,a:next()%5,b:next()%4+1,sign:next()%2?1:-1,deltas:[next()%3+1,next()%3+1,next()%3+1]};
  return algebra(`generated-${seed}-${index}`,descriptor);
 });
}
function algebra(id,d) {
 const base=`x ${d.sign===1?'+':'-'} ${d.a}`;
 const expression=d.form==='identity'?'x':d.form==='group'?'(x)':d.form==='sum'?`(${base})`:`(${base}) * ${d.b}`;
 const model=x=>({x,result:d.form==='identity'||d.form==='group'?x:d.form==='sum'?x+d.sign*d.a:(x+d.sign*d.a)*d.b});
 let x=d.x;
 const fixture=scalar(id,[`let x = ${x}`,`get result = ${expression}`],
  d.deltas.map((delta,i)=>[`s${i}`,[`x += ${delta}`]]), ['x','result'],d.deltas.map((_,i)=>`s${i}`),
  [model(x),...d.deltas.map(delta=>model(x+=delta))],
  [{text:`get result = ${expression}`,reads:['x'],irReads:['x']}]);
 fixture.descriptor=d;
 return fixture;
}
cases.push(
 scalar('unary-negative', ['let x = -1'], [['step',['x += 1']]], ['x'], ['step'], [{x:-1},{x:0}]),
 scalar('ternary-minimal', ['let x = 1','get result = true ? x : 0'], [['step',['x += 1']]], ['result'], ['step'], [{result:1},{result:2}],
  [{text:'get result = true ? x : 0',reads:['x'],irReads:['x']}]),
 scalar('grouped-local-minimal', ['let x = 0','get result = (x)'], [], ['result'], [], [{result:0}],
  [{text:'get result = (x)',reads:['x'],irReads:['x']}]),
 scalar('ungrouped-local-control', ['let x = 0','get result = x'], [], ['result'], [], [{result:0}])
);
cases.push({id:'nested-parameter-retention', source:fs.readFileSync(path.join(fixtureRoot,'nested.jtml'),'utf8'),
 fields:['x','result'], views:{child:'.child-value'}, steps:[action('step'),action('step')],
 expected:[[{x:2,result:3},{x:2,result:3}],[{x:3,result:4},{x:2,result:3}],[{x:4,result:5},{x:2,result:3}]],
 expectedViews:[[{child:['4']},{child:['4']}],[{child:['6']},{child:['4']}],[{child:['8']},{child:['4']}]],
 retain:'.child-value', checks:[{component:'Child',text:'get result = value * 2',reads:['value'],irReads:['value']}]});
const conditional=scalar('conditional-render', ['let x = 0'], [['step',['x += 1']]], ['x'], ['step','step','step'], [{x:0},{x:1},{x:2},{x:3}], [{text:'if x >= 2',reads:['x'],irReads:['x']}]);
conditional.source=conditional.source.replace('    p class "out-x" x','    p class "out-x" x\n    if x >= 2\n      p class "flag" "high"\n    else\n      p class "flag" "low"');
conditional.views={flag:'.flag'};
conditional.expectedViews=[[{flag:['low']}],[{flag:['low']}],[{flag:['high']}],[{flag:['high']}]];
cases.push(conditional);
const nestedMinimal={id:'nested-parameter-minimal',source:'jtml 2\n\nmake Child value\n  get result = value * 2\n  p class "child-value" result\n\nmake Probe\n  let x = 2\n  box class "probe"\n    p class "out-x" x\n    Child x\n\npage\n  Probe\n',fields:['x'],views:{child:'.child-value'},steps:[],expected:[[{x:2}]],expectedViews:[[{child:['4']}]],checks:[{component:'Child',text:'get result = value * 2',reads:['value'],irReads:['value']}]};
cases.push(nestedMinimal);
// Authored fixtures are checked in as directly runnable original Friendly files.
// The source constructors above document how the fixed cases were assembled;
// equality prevents drift between the reviewable file and catalog definition.
for(const fixture of cases) {
 fixture.authoredSource=path.join(fixtureRoot,fixture.id+'.jtml');
 const authored=fs.readFileSync(fixture.authoredSource,'utf8');
 if(authored!==fixture.source) throw new Error('fixture/catalog source drift: '+fixture.id);
 fixture.source=authored;
}
module.exports={cases,generated,algebra,scalar,action};
