"""Static source-export checks. Findings contain locations, never matched secrets."""
from pathlib import Path
import argparse, ipaddress, json, re

IGNORED_DIRS={'.git','.pio','.signing','build','release','__pycache__'}
TEXT_SUFFIXES={'.txt','.md','.h','.cpp','.java','.py','.json','.xml','.yml','.yaml','.ini','.csv','.gitignore'}
def source_files(root):
    for path in sorted(root.rglob('*')):
        if not path.is_file() or any(part in IGNORED_DIRS for part in path.relative_to(root).parts):continue
        if path.name=='config.local.json':continue
        if path.name in ('private_config.h','private_supla.h') and 'host-stubs' not in path.parts:continue
        yield path
def audit(root):
    findings=[];count=0
    patterns={
      'access-token':r'\b(?:gh[pousr]_[A-Za-z0-9]{25,}|github_pat_[A-Za-z0-9_]{25,})\b',
      'jwt-token':r'\beyJ[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}\b',
      'private-key':r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
      'local-profile-path':r'[A-Za-z]:[\\/]Users[\\/][^\s/\\]+',
      'hardcoded-home-network':r'\b192\.168\.\d{1,3}\.\d{1,3}\b',
      'certificate-material':r'-----BEGIN CERTIFICATE-----\s+[A-Za-z0-9+/=]{32,}',
    }
    for path in source_files(root):
        rel=path.relative_to(root).as_posix();count+=1
        if path.suffix.lower() in {'.bin','.elf','.p12','.jks','.keystore','.pem','.crt','.key','.log','.jsonl','.apk','.zip'}:
            findings.append({'file':rel,'rule':'private-or-generated-artifact'});continue
        if path.suffix.lower() not in TEXT_SUFFIXES and path.name not in ('LICENSE','.gitignore'):continue
        text=path.read_text(encoding='utf-8')
        for rule,pattern in patterns.items():
            for match in re.finditer(pattern,text):findings.append({'file':rel,'line':text.count('\n',0,match.start())+1,'rule':rule})
    example=json.loads((root/'config.example.json').read_text())
    for section,field in [('wifi','ssid'),('wifi','password'),('web','password'),('perfecta','key'),('bridge','allowed_client_ipv4'),('supla','server'),('supla','email'),('supla','ca_file'),('supla','certificate_sha256'),('supla','guid'),('supla','authkey')]:
        if example[section][field]:findings.append({'file':'config.example.json','rule':'nonempty-installation-field'})
    for section,field in [('controls','enabled'),('perfecta','configuration_writes'),('bridge','enabled'),('supla','enabled')]:
        if example[section][field] is not False:findings.append({'file':'config.example.json','rule':'unsafe-default'})
    return {'passed':not findings,'files_checked':count,'findings':findings}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);args=p.parse_args()
    report=audit(args.root.resolve());print(json.dumps(report,indent=2));raise SystemExit(0 if report['passed'] else 1)
if __name__=='__main__':main()
