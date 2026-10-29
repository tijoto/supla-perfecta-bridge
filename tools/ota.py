"""Authenticated LAN OTA with fresh disarmed-state checks. Never operates relays."""
from pathlib import Path
import argparse, hashlib, ipaddress, json, socket, time
from urllib.parse import urlsplit
import requests
from requests.auth import HTTPDigestAuth

ROOT=Path(__file__).resolve().parents[1]
def local_url(value):
    u=urlsplit(value if '://' in value else 'http://'+value)
    if u.scheme!='http' or not u.hostname or u.username or u.password or u.query or u.fragment or u.path not in ('','/'):
        raise ValueError('Podaj sam lokalny adres HTTP ESP, bez hasła i ścieżki')
    for result in socket.getaddrinfo(u.hostname,u.port or 80,type=socket.SOCK_STREAM):
        address=ipaddress.ip_address(result[4][0])
        if not (address.is_private or address.is_link_local or address.is_loopback):raise ValueError('Adres ESP musi należeć do sieci lokalnej')
    return u.geturl().rstrip('/')
def safe(s):
    if not s.get('uart_ready') or not s.get('status_fresh'):raise RuntimeError('Brak aktualnego odczytu centrali')
    if s['bridge']['connected'] or s['input7_arming']['busy']:raise RuntimeError('Rozłącz Soft i poczekaj na koniec operacji ustawień')
    if s['control']['active_gpio'] or s['control']['cooldown_ms']:raise RuntimeError('Poczekaj na zakończenie sterowania')
    if len(s['partitions'])!=2 or any(z['state']!=0 or z['alarm']!=0 for z in s['partitions']):raise RuntimeError('Rozbrój obie strefy i skasuj alarm przed OTA')
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--host');parser.add_argument('--image',required=True);args=parser.parse_args()
    base=local_url(args.host or input('Lokalny adres ESP: ').strip())
    cfg=json.loads((ROOT/'config.local.json').read_text(encoding='utf-8-sig'))
    image=Path(args.image).read_bytes()
    if not image or image[0]!=0xe9 or len(image)>=0x180000:raise ValueError('Nieprawidłowy obraz aplikacji ESP32')
    expected=hashlib.md5(image).hexdigest()
    session=requests.Session();session.trust_env=False;session.auth=HTTPDigestAuth(cfg['web']['username'],cfg['web']['password'])
    def get(path):
        response=session.get(base+path,timeout=6,allow_redirects=False)
        if response.is_redirect:raise RuntimeError('Odrzucono przekierowanie serwera')
        response.raise_for_status();return response.json()
    before=get('/api/state');safe(before);token=get('/api/session')['token'];safe(get('/api/state'))
    print('Wysyłanie firmware; sterowanie przekaźnikami nie jest wykonywane.')
    try:
        r=session.post(base+'/update',headers={'Origin':base,'X-Perfecta-Token':token},files={'firmware':('firmware.bin',image,'application/octet-stream')},timeout=(6,50),allow_redirects=False)
        if r.is_redirect:raise RuntimeError('Odrzucono przekierowanie aktualizacji')
        r.raise_for_status()
    except requests.ConnectionError:pass
    deadline=time.monotonic()+90
    while time.monotonic()<deadline:
        time.sleep(1)
        try:
            after=get('/api/state')
            if after['sketch_md5']==expected and after.get('uart_ready') and after.get('status_fresh'):
                if not after['self_test'] or after['control']['active_gpio']:raise RuntimeError('Nowy program wymaga sprawdzenia')
                print('Potwierdzono sumę firmware i aktualny odczyt centrali.');return
        except requests.RequestException:pass
    raise RuntimeError('Nie potwierdzono obrazu i odczytu; sprawdź urządzenie. Aktualizacja nie została ponowiona.')
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,RuntimeError,requests.RequestException) as error:raise SystemExit(str(error))
