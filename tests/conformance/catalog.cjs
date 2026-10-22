const assert=require('node:assert/strict');
const crypto=require('node:crypto');
const hash=value=>crypto.createHash('sha256').update(JSON.stringify(value)).digest('hex');
function verify(reports,catalog) {
 const discrepancies=reports.filter(r=>r.failures.length);
 assert.deepEqual(discrepancies.map(r=>r.id).sort(),Object.keys(catalog).sort(),'new or resolved discrepancies require review');
 for(const report of discrepancies) {
  const entry=catalog[report.id];
  assert.equal(report.sha256,entry.sourceSha256,'reviewed source changed');
  assert.equal(hash(report.expected),entry.expectedSha256,'independent oracle changed');
  assert.deepEqual(report.failures.map(({layer,observation,actual,details})=>JSON.parse(JSON.stringify({layer,observation,actual,details}))),entry.failures,'reviewed discrepancy changed');
 }
}
module.exports={verify,hash};
