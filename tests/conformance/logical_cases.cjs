// Independently authored truth tables and event traces. No plan/backend oracle.
const {scalar,action}=require('./cases.cjs');
const fs=require('node:fs'),path=require('node:path');
const checks=(text,reads)=>[{text,reads,irReads:reads}];
const cases=[
 scalar('logical-not', ['get result = !true','get inverse = !false','get nested = !!false'], [],
  ['result','inverse','nested'], [], [{result:false,inverse:true,nested:false}]),
 scalar('logical-and', ['get ff = false && false','get ft = false && true','get tf = true && false','get tt = true && true'], [],
  ['ff','ft','tf','tt'], [], [{ff:false,ft:false,tf:false,tt:true}]),
 scalar('logical-or', ['get ff = false || false','get ft = false || true','get tf = true || false','get tt = true || true'], [],
  ['ff','ft','tf','tt'], [], [{ff:false,ft:true,tf:true,tt:true}]),
 scalar('logical-reactive', ['let x = 0','let y = 3','get conjunction = (x >= 2) && (y < 5)','get disjunction = (x >= 2) || (y < 5)','get result = !((x >= 2) && (y < 5))'],
  [['step',['x += 1']],['raise',['y = 6']],['lower',['y = 3']]], ['conjunction','disjunction','result'], ['step','step','raise','step','lower'],
  [{conjunction:false,disjunction:true,result:true},{conjunction:false,disjunction:true,result:true},{conjunction:true,disjunction:true,result:false},{conjunction:false,disjunction:true,result:true},{conjunction:false,disjunction:true,result:true},{conjunction:true,disjunction:true,result:false}],
  [...checks('get conjunction = (x >= 2) && (y < 5)',['x','y']),...checks('get result = !((x >= 2) && (y < 5))',['x','y'])]),
 scalar('logical-and-guard', ['let enabled = false','let divisor = 0','get result = enabled && ((1 / divisor) > 0)'],
  [['safe',['divisor = 2']],['enable',['enabled = true']],['disable',['enabled = false']],['zero',['divisor = 0']]], ['result'], ['safe','enable','disable','zero','safe','enable'],
  [false,false,true,false,false,false,true].map(result=>({result})), checks('get result = enabled && ((1 / divisor) > 0)',['enabled','divisor'])),
 scalar('logical-or-guard', ['let enabled = true','let divisor = 0','get result = enabled || ((1 / divisor) > 0)'],
  [['safe',['divisor = 2']],['disable',['enabled = false']],['enable',['enabled = true']],['zero',['divisor = 0']]], ['result'], ['safe','disable','enable','zero','safe','disable'],
  [true,true,true,true,true,true,true].map(result=>({result})), checks('get result = enabled || ((1 / divisor) > 0)',['enabled','divisor'])),
 scalar('logical-nested-member-index', ['let enabled = true','let index = 0','let items = [2, 5]','let item = { limit: 3 }','let alias = item','get result = enabled ? ((items[index] > alias.limit) || !(item.limit > 0)) : ((items[index] > 0) && false)'],
  [['index',['index = 1 - index']],['limit',['item.limit = 6']],['off',['enabled = false']],['on',['enabled = true']]], ['result'], ['index','limit','off','index','on'],
  [false,true,false,false,false,false].map(result=>({result})), checks('get result = enabled ? ((items[index] > alias.limit) || !(item.limit > 0)) : ((items[index] > 0) && false)',['enabled','items','index','alias','item'])),
];
const siblings=scalar('logical-sibling-isolation', ['let x = 0','get result = !(x > 0)'], [['step',['x += 1']]], ['x','result'], [], [], checks('get result = !(x > 0)',['x']));
siblings.source=siblings.source.replace('page\n  Probe\n','page\n  Probe\n  Probe\n');
siblings.steps=[action('step'),action('step',1),action('step')];
siblings.expected=[
 [{x:0,result:true},{x:0,result:true}], [{x:1,result:false},{x:0,result:true}],
 [{x:1,result:false},{x:1,result:false}], [{x:2,result:false},{x:1,result:false}],
];
cases.push(siblings);
for(const fixture of cases) {
 fixture.authoredSource=path.join(__dirname,'fixtures/logical',fixture.id+'.jtml');
 const authored=fs.readFileSync(fixture.authoredSource,'utf8');
 if(authored!==fixture.source) throw new Error('logical fixture/oracle source drift: '+fixture.id);
 fixture.source=authored;
}
module.exports=cases;
