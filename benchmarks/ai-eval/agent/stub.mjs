// A scripted tool-call producer, not a model. All synthetic usage is explicitly labelled.
export class StubProvider {
  constructor(source) { this.source=source; this.calls=0; }
  async send(endpoint, body) {
    if(endpoint==='responses/input_tokens') return {kind:'deterministic-stub',input_tokens:100};
    if(endpoint!=='responses') throw Error('Unexpected stub endpoint');
    this.calls++;
    const script=`require('node:fs').writeFileSync('main.jtml',Buffer.from('${Buffer.from(this.source).toString('base64')}','base64'))`;
    const output=this.calls===1?[{type:'function_call',name:'shell',call_id:'stub-write',arguments:JSON.stringify({command:`node -e "${script}"`})}]:[{type:'message',role:'assistant',content:[{type:'output_text',text:'Stub submission complete.'}]}];
    if(this.calls>2)throw Error('Unexpected extra stub generation');
    return {kind:'deterministic-stub',id:`stub-response-${this.calls}`,model:body.model,status:'completed',output,usage:{input_tokens:100,output_tokens:100}};
  }
}
