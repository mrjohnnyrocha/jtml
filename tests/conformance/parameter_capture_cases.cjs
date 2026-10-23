// Authored arithmetic oracles; never derive expectations from emitted plans.
const {action}=require('./cases.cjs');
const row=count=>({count,result:count+1});
const view=(child,leaf)=>({child,leaf,local:['10','10']});
module.exports=[{id:'parameter-capture-siblings',file:'capture-siblings.jtml',fields:['count','result'],
 views:{child:'.child-value',leaf:'.leaf-value',local:'.child-local'},retain:'.child-value, .leaf-value',
 steps:[action('step'),action('step',1),action('step')],
 expected:[[row(100),row(100)],[row(101),row(100)],[row(101),row(101)],[row(102),row(101)]],
 expectedViews:[
  [view(['200','202'],['600','606']),view(['200','202'],['600','606'])],
  [view(['202','204'],['606','612']),view(['200','202'],['600','606'])],
  [view(['202','204'],['606','612']),view(['202','204'],['606','612'])],
  [view(['204','206'],['612','618']),view(['202','204'],['606','612'])]],
 expectedExpanded:[
  {Child:[200,200,202,202],Leaf:[600,600,606,606]},
  {Child:[200,202,202,204],Leaf:[600,606,606,612]},
  {Child:[202,202,204,204],Leaf:[606,606,612,612]},
  {Child:[202,204,204,206],Leaf:[606,612,612,618]}],
 checks:[{component:'Child',text:'get result = value * 2',reads:['value'],irReads:['value']},
  {component:'Leaf',text:'get triple = value * 3',reads:['value'],irReads:['value']}]},
 {id:'grouped-component-argument',file:'known-grouped-argument.jtml',knownDiscrepancy:true,
  fields:['count'],steps:[],views:{child:'.child-value'},expected:[[{count:100}]],expectedViews:[[{child:['202']}]],
  checks:[{component:'Child',text:'get result = value * 2',reads:['value'],irReads:['value']}]},
 {id:'global-parameter-public-alias',file:'known-global-parameter-alias.jtml',knownDiscrepancy:true,
  fields:['count','result'],steps:[],expected:[[{count:10,result:200}]],
  checks:[{text:'get result = value * 2',reads:['value'],irReads:['value']}]}];
