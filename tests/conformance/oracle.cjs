function roots(plan,result=new Set()) {
 if(!plan || typeof plan!=='object') return result;
 if(plan.kind==='path') result.add(plan.root);
 for(const value of Object.values(plan)) if(value && typeof value==='object') roots(value,result);
 return result;
}
function checkPlans(fixture,native) {
 const failures=[];
 for(const check of fixture.checks||[]) {
  const component=native.plan.componentDefinitions.find(c=>c.name===(check.component||'Probe'));
  const nodes=component?.bodyPlan.filter(n=>n.text===check.text)||[];
  if(nodes.length!==1) {failures.push(`expected one Runtime Plan node: ${check.text}; got ${nodes.length}`);continue;}
  const node=nodes[0];
  for(const key of ['reads','writes']) for(const name of check[key]||[])
   if(!node[key].includes(name)) failures.push(`${check.text}: missing ${key} ${name}`);
  for(const name of check.irReads||[]) if(!roots(node.expressionPlan).has(name)) failures.push(`${check.text}: Expression IR missing ${name}`);
  const origin=native.origins.find(o=>o.component===component.name&&o.text===check.text)?.origin;
  const lines=fixture.source.split('\n');
  const expectedLine=lines.findIndex(line=>line.trim()===check.text)+1;
  const expectedColumn=lines[expectedLine-1]?.indexOf(check.text)+1;
  if(!origin || (fixture.sourceFile&&origin.file!==fixture.sourceFile) || origin.line!==expectedLine || origin.column!==expectedColumn || !origin.exact)
   failures.push(`${check.text}: original provenance mismatch ${JSON.stringify(origin)} expected ${expectedLine}:${expectedColumn}`);
 }
 const component=native.plan.componentDefinitions.find(c=>c.name==='Probe');
 const semantic=native.semanticComponents.find(c=>c.name==='Probe');
 for(const step of fixture.steps||[]) {
  if(!component?.localActions.includes(step.action)||!semantic?.actions.includes(step.action)) failures.push(`compiler metadata missing action ${step.action}`);
 }
 for(const field of fixture.fields) {
  if(!component || ![...component.localState,...component.localDerived].includes(field)) failures.push(`Runtime Plan missing observable ${field}`);
  if(!semantic || ![...semantic.state,...semantic.derived].includes(field)) failures.push(`Semantic IR missing observable ${field}`);
 }
 return failures;
}
function compare(fixture,observed,planFailures) {
 const expected=fixture.expected;
 const visible=expected.map((step,i)=>step.map((values,j)=>({...Object.fromEntries(Object.entries(values).map(([k,v])=>[k,String(v)])),...fixture.expectedViews?.[i]?.[j]})));
 const failures=[];
 if(planFailures.length) failures.push({layer:'compiler-plan',details:planFailures});
 for(const [backend,trace] of Object.entries(observed)) {
  for(const kind of ['state','visible']) if(JSON.stringify(trace[kind])!==JSON.stringify(kind==='state'?expected:visible))
   failures.push({layer:backend,observation:kind,expected:kind==='state'?expected:visible,actual:trace[kind]});
 }
 return failures;
}

module.exports={roots,checkPlans,compare};
