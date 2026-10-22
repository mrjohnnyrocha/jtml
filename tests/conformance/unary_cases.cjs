// Exact integer/dyadic arithmetic tables authored independently of both backends.
const {scalar,action}=require('./cases.cjs');
const fs=require('node:fs'),path=require('node:path');
// Unary-minus Expression IR is currently Source/Unknown: require Runtime Plan
// reads and exact provenance without falsely asserting typed IR/path children.
const check=(text,reads)=>[{text,reads}];
const cases=[
 scalar('unary-initialized-reactivity', ['let x = -1.25','get result = x * 2 + 1'],
  [['step',['x += 0.25']]], ['x','result'], ['step','step'],
  [{x:-1.25,result:-1.5},{x:-1,result:-1},{x:-0.75,result:-0.5}], [{text:'get result = x * 2 + 1',reads:['x'],irReads:['x']}]),
 scalar('unary-negative-integers', ['let x = -2','let y = -3','get result = (x + y) * 2'],
  [['step',['x += 1']],['other',['y -= 1']]], ['x','y','result'], ['step','other'],
  [{x:-2,y:-3,result:-10},{x:-1,y:-3,result:-8},{x:-1,y:-4,result:-10}], [{text:'get result = (x + y) * 2',reads:['x','y'],irReads:['x','y']}]),
 scalar('unary-direct', ['get result = -1','get repeated = -(-1)','get nested = -(-(-1))','get sum = -1 + 1'], [], ['result','repeated','nested','sum'], [], [{result:-1,repeated:1,nested:-1,sum:0}]),
 scalar('unary-fractions', ['let x = 1.25','get result = -x','get repeated = -(-0.125)'],
  [['double',['x = x * 2']],['replace',['x = 0 - 0.5']],['flip',['x = 0 - x']]], ['result','repeated'], ['double','replace','flip'],
  [-1.25,-2.5,0.5,-0.5].map(result=>({result,repeated:0.125})), check('get result = -x',['x'])),
 scalar('unary-grouped-arithmetic', ['let x = 2','let y = 3','get result = -(x + (y * 2)) * 2 + 1'],
  [['left',['x += 1']],['right',['y -= 1']]], ['result'], ['left','right'], [-15,-17,-13].map(result=>({result})), check('get result = -(x + (y * 2)) * 2 + 1',['x','y'])),
 scalar('unary-derived-chain', ['let x = 1','get inverted = -x','get result = inverted * 2 + 3'],
  [['step',['x += 1']],['flip',['x = 0 - x']]], ['inverted','result'], ['step','flip','step'],
  [{inverted:-1,result:1},{inverted:-2,result:-1},{inverted:2,result:7},{inverted:1,result:5}],
  [...check('get inverted = -x',['x']),...check('get result = inverted * 2 + 3',['inverted'])]),
 scalar('unary-ternary-logical', ['let enabled = true','let x = 2','get result = enabled ? -x : -(-x)','get positive = !(result < 0)'],
  [['toggle',['enabled = !enabled']],['step',['x += 1']]], ['result','positive'], ['toggle','step','toggle'],
  [{result:-2,positive:false},{result:2,positive:true},{result:3,positive:true},{result:-3,positive:false}], check('get result = enabled ? -x : -(-x)',['enabled','x'])),
 scalar('unary-owned-index', ['let item = { score: 1.25 }','let alias = item','let index = 0','let items = [2, 5]','get result = -(alias.score + items[index])'],
  [['member',['item.score += 0.25']],['index',['index = 1 - index']],['write',['items[index] = 0 - 1']]], ['result'], ['member','index','write'],
  [-3.25,-3.5,-6.5,-0.5].map(result=>({result})), check('get result = -(alias.score + items[index])',['alias','items','index'])),
 scalar('unary-zero', ['get negative = -0','get restored = -(-0)','get result = (-0) + 0'], [], ['result','restored'], [], [{result:0,restored:0}]),
];
const siblings=scalar('unary-sibling-isolation', ['let x = 1','get result = -x'], [['step',['x += 1']],['flip',['x = 0 - x']]], ['x','result'], [], [], check('get result = -x',['x']));
siblings.source=siblings.source.replace('page\n  Probe\n','page\n  Probe\n  Probe\n');
siblings.steps=[action('step'),action('flip',1),action('step',1),action('flip')];
siblings.expected=[
 [{x:1,result:-1},{x:1,result:-1}], [{x:2,result:-2},{x:1,result:-1}],
 [{x:2,result:-2},{x:-1,result:1}], [{x:2,result:-2},{x:0,result:0}],
 [{x:-2,result:2},{x:0,result:0}],
];
// Exclude raw negative-zero formatting from this bounded DOM contract: native
// ostream prints -0 while JavaScript String prints 0. Sign is tested directly.
// Adding numeric zero makes this displayed result positive zero at x=0.
siblings.source=siblings.source.replace('get result = -x','get result = (-x) + 0');
siblings.checks=check('get result = (-x) + 0',['x']);
cases.push(siblings);
for(const fixture of cases) {
 fixture.authoredSource=path.join(__dirname,'fixtures/unary',fixture.id+'.jtml');
 const authored=fs.readFileSync(fixture.authoredSource,'utf8');
 if(authored!==fixture.source) throw new Error('unary fixture/oracle source drift: '+fixture.id);
 fixture.source=authored;
}
module.exports=cases;
