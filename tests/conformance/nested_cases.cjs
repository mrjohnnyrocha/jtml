const fs=require('node:fs'),path=require('node:path');
const {action}=require('./cases.cjs');
const row=x=>({x,result:x+1});
const views=(child,leaf)=>({child,leaf,local:['10','10']});
module.exports=[
 {id:'nested-deep-siblings',file:'deep-siblings.jtml',fields:['x','result'],
  views:{child:'.child-value',leaf:'.leaf-value',local:'.child-local'},retain:'.child-value, .leaf-value',
  steps:[action('step'),action('step',1),action('step')],
  expected:[[row(2),row(2)],[row(3),row(2)],[row(3),row(3)],[row(4),row(3)]],
  expectedViews:[
   [views(['4','6'],['12','18']),views(['4','6'],['12','18'])],
   [views(['6','8'],['18','24']),views(['4','6'],['12','18'])],
   [views(['6','8'],['18','24']),views(['6','8'],['18','24'])],
   [views(['8','10'],['24','30']),views(['6','8'],['18','24'])]],
  checks:[{component:'Child',text:'get result = value * 2',reads:['value'],irReads:['value']},{component:'Leaf',text:'get triple = value * 3',reads:['value'],irReads:['value']}]},
 {id:'nested-lifecycle',file:'lifecycle.jtml',fields:['x','result'],views:{child:'.child-value',local:'.child-local'},
  steps:['toggle','step','toggle','step','toggle','toggle','step'].map(name=>action(name)),
  expected:[2,2,3,3,4,4,4,5].map(x=>[row(x)]),
  expectedViews:[
   [{child:['4'],local:['10']}],[{child:[],local:[]}],[{child:[],local:[]}],
   [{child:['6'],local:['10']}],[{child:['8'],local:['10']}],[{child:[],local:[]}],
   [{child:['8'],local:['10']}],[{child:['10'],local:['10']}]],
  checks:[{component:'Child',text:'get result = value * 2',reads:['value'],irReads:['value']}]}]
 .map(fixture=>({...fixture,authoredSource:path.join(__dirname,'fixtures/nested',fixture.file),source:fs.readFileSync(path.join(__dirname,'fixtures/nested',fixture.file),'utf8')}));
