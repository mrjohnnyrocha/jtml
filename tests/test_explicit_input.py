from pathlib import Path
import os,shutil,re
import subprocess,json
repo=Path(__file__).resolve().parents[1];compiler=os.environ.get('JTML_BIN',str(repo/'build/jtml'));out=repo/'dist/explicit-input-negatives';out.mkdir(parents=True,exist_ok=True)
header='jtml 2\nlet email = "seed"\nmake Editor bind value\n  page\n    input into value aria-label "Value"\n'
cases={
 'missing':'jtml 2\nlet email = "seed"\nmake Editor bind value\n  input into value\npage\n  Editor\n',
 'constant':header.replace('let email','const email')+'page\n  Editor bind value to email\n',
 'derived':header.replace('let email = "seed"','get email = "seed"')+'page\n  Editor bind value to email\n',
 'borrowed':header.replace('let email = "seed"','let original = "seed"\nlet email = original')+'page\n  Editor bind value to email\n',
 'collection':header.replace('let email = "seed"','let email = ["seed"]')+'page\n  Editor bind value to email\n',
 'unknown':header+'page\n  Editor bind value to missing\n',
 'member':header+'page\n  Editor bind value to email.name\n',
 'wrong-name':header+'page\n  Editor bind other to email\n',
 'duplicate':header+'page\n  Editor bind value to email bind value to email\n',
 'local-redeclaration':header.replace('  page','  let value = "local"\n  page')+'page\n  Editor bind value to email\n',
 'assignment':header.replace('  page','  when mutate\n    value = "bad"\n  page')+'page\n  Editor bind value to email\n',
 'checkbox':header.replace('input into value','checkbox into value')+'page\n  Editor bind value to email\n',
 'number':header.replace('input into value','input into value type "number"')+'page\n  Editor bind value to email\n',
 'forwarding':header+'make Parent bind outer\n  Editor bind value to outer\npage\n  Parent bind outer to email\n',
 'unauthorized':header.replace('make Editor bind value','make Editor value')+'page\n  Editor bind value to email\n',
 'route':header+'route "/:value" as Editor\n',
}
(out/'state.jtml').write_text('jtml 2\nexport let imported = "seed"\n')
cases['imported-state']=header.replace('let email = "seed"','use { imported } from "./state.jtml"')+'page\n  Editor bind value to imported\n'
(out/'collision.jtml').write_text('jtml 2\nlet email = "private"\n')
cases['ambiguous']=header.replace('let email = "seed"','use "./collision.jtml"\nlet email = "seed"')+'page\n  Editor bind value to email\n'
rows=[]
for name,source in cases.items():
 f=out/(name+'.jtml');f.write_text(source);dest=out/(name+'-out')
 shutil.rmtree(dest,ignore_errors=True)
 r=subprocess.run([compiler,'build',str(f),'--target','browser','--out',str(dest)],capture_output=True,text=True)
 rows.append(dict(name=name,source=str(f),exit=r.returncode,error=r.stderr,assets=list(str(x.relative_to(dest)) for x in dest.rglob('*') if x.is_file())))
expected={
 'missing':'requires explicit', 'constant':'mutable let', 'derived':'mutable let',
 'borrowed':'literal string initializer', 'collection':'literal string initializer',
 'unknown':'mutable let', 'member':'requires explicit', 'wrong-name':'requires explicit',
 'duplicate':'duplicate binding', 'local-redeclaration':'cannot be redeclared',
 'assignment':'input-only', 'checkbox':'text input only', 'number':'text input types',
 'forwarding':'forwarding/nested authority', 'unauthorized':'does not declare binding authority',
 'route':'cannot receive input binding authority', 'imported-state':'mutable let',
 'ambiguous':'ambiguous module storage',
}
for row in rows:
 source=cases[row['name']].splitlines()
 line=next(i+1 for i,text in enumerate(source) if text.startswith('  let value')) if row['name']=='local-redeclaration' else next(i+1 for i,text in enumerate(source) if text.startswith('  Editor bind')) if row['name']=='forwarding' else len(source)
 column=1 if row['name']=='route' else 3
 row['expectedLocation']=f"{row['source']}:{line}:{column}"
 row['pass']=row['exit']!=0 and expected[row['name']] in row['error'] and not row['assets'] and row['expectedLocation'] in row['error']
# A binding declaration in another file retains that original definition location.
(out/'bad-editor.jtml').write_text('jtml 2\nexport make Bad bind value\n  page\n    input into value type "number"\n')
f=out/'imported-diagnostic.jtml';f.write_text('jtml 2\nuse { Bad } from "./bad-editor.jtml"\nlet email = "seed"\npage\n  Bad bind value to email\n')
dest=out/'imported-diagnostic-out';shutil.rmtree(dest,ignore_errors=True)
r=subprocess.run([compiler,'build',str(f),'--target','browser','--out',str(dest)],capture_output=True,text=True)
rows.append(dict(name='imported-diagnostic',exit=r.returncode,error=r.stderr,assets=list(str(x.relative_to(dest)) for x in dest.rglob('*') if x.is_file()),pass_=r.returncode!=0 and not any(x.is_file() for x in dest.rglob('*')) and str(f)+':5:3' in r.stderr and str(out/'bad-editor.jtml')+':4:5' in r.stderr))
rows[-1]['pass']=rows[-1].pop('pass_')
# Native/live and inline transpilation have no capability projection and must reject.
for command in [['transpile',str(repo/'tests/fixtures/production/explicit-input/index.jtml'),'-o',str(out/'native.html')],['build',str(repo/'tests/fixtures/production/explicit-input/index.jtml'),'--out',str(out/'native')]]:
 output=Path(command[-1]);shutil.rmtree(output,ignore_errors=True) if output.is_dir() else output.unlink(missing_ok=True)
 r=subprocess.run([compiler,*command],capture_output=True,text=True)
 rows.append(dict(name='unsupported-'+command[0],exit=r.returncode,error=r.stderr,pass_=r.returncode!=0 and not (output.is_file() or any(x.is_file() for x in output.rglob('*'))) and 'explicit input bindings require a browser Runtime Project Plan' in r.stderr and re.search(r'editor\.jtml:2:8',r.stderr) is not None))
 rows[-1]['pass']=rows[-1].pop('pass_')
try:
 r=subprocess.run([compiler,'serve',str(repo/'tests/fixtures/production/explicit-input/index.jtml'),'--port','38421'],capture_output=True,text=True,timeout=5)
 rows.append(dict(name='unsupported-live',exit=r.returncode,error=r.stderr,pass_=r.returncode!=0 and 'explicit input bindings require a browser Runtime Project Plan' in r.stderr and re.search(r'editor\.jtml:2:8',r.stderr) is not None))
 rows[-1]['pass']=rows[-1].pop('pass_')
except subprocess.TimeoutExpired:
 rows.append(dict(name='unsupported-live',error='live backend did not reject before startup',pass_=False))
 rows[-1]['pass']=rows[-1].pop('pass_')
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(rows,indent=2))
raise SystemExit(0 if all(row['pass'] for row in rows) else 1)
