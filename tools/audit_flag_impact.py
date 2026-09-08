"""Compare legacy conditional fallbacks without writing/rebuilding game sources.
Run from any directory; optional argv1 is the baseline root Git commit.
"""
from pathlib import Path
import sys,subprocess,tempfile,types,json,re,time
root=Path(__file__).resolve().parents[1];reference=sys.argv[1] if len(sys.argv)>1 else '741d5e2';vendor=root/'third_party/xboxrecomp';sys.path.insert(0,str(vendor))
from tools.recomp import config
from tools.recomp.translator import BatchTranslator
config.configure_from_xbe(str(root/'local/assets/default.xbe'))
with tempfile.TemporaryDirectory() as d:
 d=Path(d)
 patch=subprocess.check_output(['git','show',reference+':patches/xboxrecomp-lifter.patch'])
 paths=[line.split(' b/',1)[1] for line in patch.decode().splitlines() if line.startswith('diff --git ')]
 for path in paths:
  p=d/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(subprocess.check_output(['git','show','HEAD:'+path],cwd=vendor))
 subprocess.run(['git','apply','-'],cwd=d,input=patch,check=True)
 def module(name,path,replace=None):
  m=types.ModuleType('tools.recomp.'+name);m.__package__='tools.recomp';m.__file__=str(path);sys.modules[m.__name__]=m
  source=path.read_text()
  if replace:source=source.replace(*replace)
  exec(compile(source,str(path),'exec'),m.__dict__);return m
 old_l=module('_audit_lifter',d/'tools/recomp/lifter.py')
 old_t=module('_audit_translator',d/'tools/recomp/translator.py',('from .lifter import','from ._audit_lifter import'))
 args=dict(xbe_path=str(root/'local/assets/default.xbe'),func_json_path=str(root/'local/reports/disasm/functions.json'),labels_json_path=str(root/'local/reports/disasm/labels.json'),identified_json_path=str(root/'local/reports/func_id/identified_functions.json'),abi_json_path=str(root/'local/reports/abi/abi_functions.json'))
 old=old_t.BatchTranslator(**args);new=BatchTranslator(**args)
 defined=set()
 for p in (root/'local/generated').glob('recomp_*.c'):
  defined.update(int(x,16) for x in re.findall(r'^void sub_([0-9A-F]{8})\(void\)',p.read_text(),re.M))
 report={'reference_commit':reference,'new_sites':[],'functions':0,'old_fallbacks':0,'new_fallbacks':0,'increased':[],'changed':[]};start=time.monotonic()
 def sites(source):
  label=None;result=set()
  for line in source.splitlines():
   match=re.match(r'loc_([0-9A-F]{8}):',line)
   if match:label=match.group(1)
   if 'if (_flags' in line:result.add((label,line.strip()))
  return result
 for address in sorted(defined & old.func_db.keys() & new.func_db.keys()):
  before=old.translator.translate_function(address,old.func_db[address]);after=new.translator.translate_function(address,new.func_db[address])
  if before is None or after is None:continue
  a=before.count('if (_flags');b=after.count('if (_flags');report['functions']+=1;report['old_fallbacks']+=a;report['new_fallbacks']+=b
  if a!=b:report['changed'].append({'address':hex(address),'before':a,'after':b})
  for site in sorted(sites(after)-sites(before)):report['new_sites'].append({'function':hex(address),'label':site[0],'condition':site[1]})
  if b>a:report['increased'].append({'address':hex(address),'before':a,'after':b})
 report['seconds']=round(time.monotonic()-start,2)
 (root/'local/reports/flag-impact-staged.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
