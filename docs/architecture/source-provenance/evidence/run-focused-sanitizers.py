import pathlib,subprocess,os,tempfile
root=pathlib.Path.cwd()
gtest=pathlib.Path(os.environ.get('JTML_GOOGLETEST_SOURCE', str(root/'build/_deps/googletest-src/googletest')))
for name,san in [('asan','address,undefined'),('tsan','thread')]:
 out=pathlib.Path(tempfile.mkdtemp(prefix='jtml-source-'+name+'-'))
 flags=['c++','-std=c++17','-O1','-gline-tables-only','-fno-omit-frame-pointer','-fsanitize='+san,'-I'+str(root),'-I'+str(root/'include'),'-I'+str(root/'third_party'),'-I'+str(gtest/'include'),'-DJTML_SOURCE_DIR="'+str(root)+'"']
 objects=[]
 for file in ['cli/module_loader.cpp','tests/test_source_origin.cpp']:
  obj=out/(pathlib.Path(file).stem+'.o');subprocess.run(flags+['-c',file,'-o',str(obj)],check=True);objects.append(str(obj))
 exe=out/'probe'
 subprocess.run(flags+objects+[str(root/'build/libjtml_cli_common.a'),str(root/'build/libjtml_core.a'),str(root/'build/lib/libgtest_main.a'),str(root/'build/lib/libgtest.a'),'-o',str(exe)],check=True)
 env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',TSAN_OPTIONS='halt_on_error=1')
 with (out/'result.log').open('w') as f: subprocess.run([str(exe)],env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=30)
 print(name,'PASS six source-origin tests (mapper and loader instrumented; unchanged dependencies Release)',flush=True)
