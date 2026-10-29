"""Generate ignored, installation-specific headers. Standard library only."""
from pathlib import Path
import argparse, getpass, ipaddress, json, re, secrets, ssl, sys, uuid

ROOT=Path(__file__).resolve().parents[1]
def load(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def yes(prompt):return input(prompt+' [t/N]: ').strip().lower() in ('t','tak','y','yes')
def interactive(cfg):
    cfg['wifi']['ssid']=input('Nazwa sieci Wi-Fi: ').strip()
    cfg['wifi']['password']=getpass.getpass('Hasło Wi-Fi: ')
    cfg['perfecta']['key']=getpass.getpass('Klucz komunikacji PERFECTA Soft (16 znaków): ')
    cfg['web']['username']=input('Login strony ESP [admin]: ').strip() or 'admin'
    cfg['web']['password']=getpass.getpass('Nowe hasło strony ESP (min. 12 znaków): ')
    cfg['controls']['enabled']=yes('Włączyć sterowanie przez przekaźniki po sprawdzeniu okablowania?')
    cfg['perfecta']['configuration_writes']=yes('Włączyć lokalny zapis przypisań wejść do stref?')
    cfg['bridge']['enabled']=yes('Włączyć most dla PERFECTA Soft?')
    if cfg['bridge']['enabled']:cfg['bridge']['allowed_client_ipv4']=input('Adres lokalnego komputera uprawnionego do mostu: ').strip()
    cfg['supla']['enabled']=yes('Włączyć integrację SUPLA?')
    if cfg['supla']['enabled']:
        s=cfg['supla'];s['server']=input('Serwer urządzeń SUPLA (bez https://): ').strip()
        s['email']=input('E-mail konta SUPLA Cloud: ').strip()
        s['tls_server_name']=input('Nazwa z certyfikatu TLS [jak serwer]: ').strip() or s['server']
        s['ca_file']=input('Plik PEM z zaufanym certyfikatem CA/serwera: ').strip()
    return cfg
def validate(cfg,ci):
    for section,key in [('controls','enabled'),('perfecta','configuration_writes'),('bridge','enabled'),('supla','enabled')]:
        if type(cfg[section][key]) is not bool:raise ValueError(section+'.'+key+' musi być true/false')
    if not ci and not cfg['wifi']['ssid']:raise ValueError('Uzupełnij nazwę sieci Wi-Fi')
    key=cfg['perfecta']['key']
    if not isinstance(key,str) or len(key)!=16 or not all(33<=ord(x)<=126 for x in key):raise ValueError('Klucz centrali: dokładnie 16 drukowalnych znaków ASCII')
    if not cfg['web']['username'] or ':' in cfg['web']['username']:raise ValueError('Nieprawidłowy login WWW')
    if len(cfg['web']['password'])<12:raise ValueError('Hasło WWW musi mieć przynajmniej 12 znaków')
    if cfg['bridge']['enabled']:
        ip=ipaddress.IPv4Address(cfg['bridge']['allowed_client_ipv4'])
        value=int(ip)
        if not (value>>24==10 or value>>20==0xac1 or value>>16==0xc0a8):raise ValueError('Most wymaga adresu komputera w LAN')
    s=cfg['supla'];cert=''
    if s['enabled']:
        if not s['server'] or '/' in s['server'] or any(x.isspace() for x in s['server']):raise ValueError('Podaj nazwę serwera urządzeń SUPLA bez protokołu i ścieżki')
        if not re.fullmatch(r'[^\s@]+@[^\s@]+',s['email']):raise ValueError('Uzupełnij e-mail konta SUPLA')
        if type(s['port']) is not int or not 1<=s['port']<=65535:raise ValueError('Nieprawidłowy port SUPLA')
        path=Path(s['ca_file']).expanduser()
        if not path.is_absolute():path=ROOT/path
        cert=path.read_text(encoding='ascii')
        if '-----BEGIN CERTIFICATE-----' not in cert or 'PRIVATE KEY' in cert:raise ValueError('Podaj certyfikat PEM, nigdy klucz prywatny')
        ssl.create_default_context(cadata=cert)
        s['tls_server_name']=s['tls_server_name'] or s['server']
    pin=s['certificate_sha256'].replace(':','').strip()
    if pin and not re.fullmatch('[0-9a-fA-F]{64}',pin):raise ValueError('Opcjonalny pin SHA256 musi mieć 64 cyfry hex')
    for field in ('guid','authkey'):
        if not s[field]:s[field]=uuid.uuid4().hex if field=='guid' else secrets.token_hex(16)
        if not re.fullmatch('[0-9a-fA-F]{32}',s[field]):raise ValueError('Nieprawidłowe '+field)
    return cert,pin
def write_headers(cfg,cert,pin,ci):
    folder=ROOT/'firmware-esp32/include';folder.mkdir(parents=True,exist_ok=True)
    quote=lambda value:json.dumps(value,ensure_ascii=True)
    array=lambda value:','.join(str(x) for x in bytes.fromhex(value))
    boolean=lambda value:'true' if value else 'false'
    head='#pragma once\n// LOCAL GENERATED FILE. NEVER COMMIT OR SHARE.\n#include <stdint.h>\n'
    base=head
    for name,value in [('WIFI_SSID',cfg['wifi']['ssid']),('WIFI_PASSWORD',cfg['wifi']['password']),('PANEL_KEY',cfg['perfecta']['key']),('ADMIN_USER',cfg['web']['username']),('ADMIN_PASSWORD',cfg['web']['password']),('BRIDGE_IP',cfg['bridge']['allowed_client_ipv4'])]:base+='static const char '+name+'[]='+quote(value)+';\n'
    for name,value in [('CONTROL_ENABLED',ci or cfg['controls']['enabled']),('CONFIG_WRITES_ENABLED',ci or cfg['perfecta']['configuration_writes']),('BRIDGE_ENABLED',ci or cfg['bridge']['enabled'])]:base+='constexpr bool '+name+'='+boolean(value)+';\n'
    # Volatile keeps all production branches linked in CI, while the true guard
    # prevents networking/UART at runtime. Installation builds use constant false.
    base+=('inline volatile bool BUILD_TEST_ONLY=true;\n' if ci else 'constexpr bool BUILD_TEST_ONLY=false;\n')
    (folder/'private_config.h').write_text(base,encoding='utf-8')
    s=cfg['supla'];text=head+'constexpr bool SUPLA_ENABLED='+boolean(ci or s['enabled'])+';\n'
    for name,value in [('SUPLA_SERVER',s['server']),('SUPLA_EMAIL',s['email']),('SUPLA_TLS_SERVER_NAME',s['tls_server_name']),('SUPLA_SERVER_CERT',cert)]:text+='static const char '+name+'[]='+quote(value)+';\n'
    text+='constexpr uint16_t SUPLA_SERVER_PORT='+str(s['port'])+';\n'
    text+='constexpr bool SUPLA_USE_PIN='+boolean(bool(pin))+';\n'
    for name,value in [('SUPLA_GUID_BYTES',s['guid']),('SUPLA_AUTHKEY_BYTES',s['authkey']),('SUPLA_CERT_SHA256',pin or '00'*32)]:text+='static const uint8_t '+name+'[]={'+array(value)+'};\n'
    text+='static const char* const SUPLA_SENSOR_NAMES[]={'+','.join(quote('Wejście '+str(n)) for n in range(9,17))+'};\n'
    (folder/'private_supla.h').write_text(text,encoding='utf-8')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--interactive',action='store_true');p.add_argument('--ci',action='store_true');p.add_argument('--new-identity',action='store_true');args=p.parse_args()
    local=ROOT/'config.local.json';cfg=load(local if local.exists() and not args.ci else ROOT/'config.example.json')
    if args.ci:
        cfg['perfecta']['key']='0123456789ABCDEF';cfg['web']['password']='BUILD-TEST-ONLY-UNUSABLE';cfg['supla']['guid']='00'*16;cfg['supla']['authkey']='00'*16
    elif args.interactive or not local.exists():cfg=interactive(cfg)
    if args.new_identity:cfg['supla']['guid']=cfg['supla']['authkey']=''
    cert,pin=validate(cfg,args.ci);write_headers(cfg,cert,pin,args.ci)
    if not args.ci:local.write_text(json.dumps(cfg,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Utworzono lokalne nagłówki. Nie publikuj nagłówków, config.local.json ani własnego firmware.')
    if args.ci:print('BUILD_TEST_ONLY: ten obraz nie uruchamia komunikacji z siecią ani centralą.')
if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,OSError) as error:raise SystemExit('Konfiguracja: '+str(error))
