// Greedy deterministic reduction, 1-minimal under the explicit finite moves.
// The predicate must preserve the same discrepancy class, not merely any failure.
async function minimize(initial,candidates,predicate) {
 let current=initial, attempts=0, accepted=0;
 const visited=new Set([JSON.stringify(initial)]);
 for(;;) {
  let changed=false;
  for(const candidate of candidates(current)) {
   const key=JSON.stringify(candidate);
   if(visited.has(key)) continue;
   visited.add(key);attempts++;
   if(await predicate(candidate)) {current=candidate;accepted++;changed=true;break;}
  }
  if(!changed) return {value:current,attempts,accepted};
 }
}
function algebraCandidates(d) {
 const candidates=[];
 // Remove event chunks before reducing the expression; this separates startup
 // failures from transition failures and minimizes the event trace as well.
 for(let i=0;i<d.deltas.length;i++) candidates.push({...d,deltas:d.deltas.filter((_,j)=>i!==j)});
 if(d.x!==0) candidates.push({...d,x:0});
 if(d.a!==0) candidates.push({...d,a:0});
 if(d.b!==1) candidates.push({...d,b:1});
 if(d.sign!==1) candidates.push({...d,sign:1});
 const forms=['product','sum','group','identity'];
 const position=forms.indexOf(d.form||'product');
 for(let i=position+1;i<forms.length;i++) candidates.push({...d,form:forms[i]});
 return candidates;
}
module.exports={minimize,algebraCandidates};
