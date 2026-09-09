#!/usr/bin/env python3
"""Exercise optional profile output using the real RPC/generated-wrapper fixture."""
from pathlib import Path
import importlib.util,os,re,subprocess,sys
HERE=Path(__file__).resolve().parent

def main():
    if '--fixture' in sys.argv:
        spec=importlib.util.spec_from_file_location('rpc_ordering',HERE/'test_gl_rpc_ordering.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        anchor='    wumpa_gl_rpc_stop();assert(!wumpa_gl_rpc_active());'
        assert module.FIXTURE.count(anchor)==1
        module.FIXTURE=module.FIXTURE.replace(anchor,'    for(unsigned i=0;i<58;++i)assert(!wumpa_gl_rpc_swap(&window));\n'+anchor)
        module.main();return
    for enabled in ('0','1'):
        result=subprocess.run([sys.executable,str(Path(__file__).resolve()),'--fixture'],
            env=dict(os.environ,WRATH_GL_RPC_PROFILE=enabled),capture_output=True,text=True,check=True)
        summary=[line for line in result.stderr.splitlines() if line.startswith('[shield rpc profile]')]
        rows=[line for line in result.stderr.splitlines() if line.startswith('[shield rpc call]')]
        if enabled=='0':assert not summary and not rows;continue
        assert len(summary)==1 and 'swaps=60 all_callers=1' in summary[0]
        assert 'untracked=0' in summary[0]
        assert 0<len(rows)<=12
        assert any('name=unnamed count=2000 ' in row for row in rows)
        assert any('name=swap count=60 ' in row for row in rows)
        for name in ('glBufferData','glShaderSource','glGetIntegerv','glGetString'):
            assert any('name='+name+' count=1 ' in row for row in rows),name
        for field in ('total_ms_per_frame','caller_cpu_ms','render_wall_ms','render_cpu_ms','handoff_ms'):
            value=float(re.search(field+r'=([0-9.]+)',summary[0]).group(1))
            assert value>=0,(field,value)
    print('PASS: optional RPC profiles count real named calls, report caller/renderer timing, cap top12, and remain silent when disabled')
if __name__=='__main__':main()
