"""Build native Android APK from locally installed official SDK tools; no Gradle/dependencies download."""
from pathlib import Path
import hashlib,json,os,secrets,subprocess,sys,zipfile,math,struct,wave
sys.stdout.reconfigure(encoding='utf-8',errors='backslashreplace')
sys.stderr.reconfigure(encoding='utf-8',errors='backslashreplace')

ROOT=Path(__file__).resolve().parent
if not os.environ.get('ANDROID_HOME') or not os.environ.get('JAVA_HOME'):
    raise SystemExit('Ustaw ANDROID_HOME (Android SDK) i JAVA_HOME (JDK 17). Patrz README.txt.')
SDK=Path(os.environ['ANDROID_HOME'])
JDK=Path(os.environ['JAVA_HOME'])
TOOLS=SDK/'build-tools'/'36.0.0'
ANDROID=SDK/'platforms'/'android-35'/'android.jar'
BUILD=ROOT/'build'
RELEASE=ROOT/'release'
SIGN=ROOT/'.signing'
VERSION='1.0.0'
EXE='.exe' if os.name=='nt' else ''
for directory in (BUILD,RELEASE,SIGN,BUILD/'classes',BUILD/'gen',BUILD/'dex',BUILD/'test-classes'):directory.mkdir(parents=True,exist_ok=True)
environment=os.environ.copy();environment['JAVA_HOME']=str(JDK);environment['PATH']=str(JDK/'bin')+os.pathsep+environment.get('PATH','')

def run(args,**kwargs):
    result=subprocess.run([str(arg) for arg in args],cwd=ROOT,env=environment,text=True,encoding='utf-8',errors='replace',capture_output=True,**kwargs)
    if result.stdout.strip():print(result.stdout.strip())
    if result.returncode:
        print(result.stderr,file=sys.stderr);raise SystemExit(result.returncode)
    if result.stderr.strip() and 'warning' not in result.stderr.lower():print(result.stderr.strip())
    return result

# Test transport against an isolated localhost fixture, never the real panel.
pure=[ROOT/'src'/'org'/'suplaforum'/'perfectalan'/name for name in ('LanEndpoint.java','EspApi.java','PanelPolicy.java','AlarmLatch.java')]
run([JDK/'bin'/('javac'+EXE),'-encoding','UTF-8','-d',BUILD/'test-classes',*pure,ROOT/'tests'/'PureTests.java'])
run([JDK/'bin'/('java'+EXE),'-cp',BUILD/'test-classes','org.suplaforum.perfectalan.PureTests'])
run([JDK/'bin'/('javac'+EXE),'-encoding','UTF-8','-cp',BUILD/'test-classes','-d',BUILD/'test-classes',
     ROOT/'src/org/suplaforum/perfectalan/CloudEndpoint.java',ROOT/'src/org/suplaforum/perfectalan/CloudWire.java',ROOT/'tests/CloudWireTests.java'])
run([JDK/'bin'/('java'+EXE),'-cp',BUILD/'test-classes','org.suplaforum.perfectalan.CloudWireTests'])
(ROOT/'res'/'raw').mkdir(exist_ok=True)
with wave.open(str(ROOT/'res'/'raw'/'alarm_signal.wav'),'wb') as sound:
    rate=22050;sound.setparams((1,2,rate,0,'NONE','not compressed'));frames=bytearray()
    for sample in range(2*rate):
        t=sample/rate;phase=t%1
        if phase<.35:frequency=900;position=phase
        elif .45<=phase<.8:frequency=1400;position=phase-.45
        else:frequency=0;position=0
        envelope=max(0,min(1,position/.012,(.35-position)/.012)) if frequency else 0
        frames.extend(struct.pack('<h',int(26000*envelope*math.sin(2*math.pi*frequency*t))))
    sound.writeframes(frames)
run([TOOLS/('aapt2'+EXE),'compile','--dir',ROOT/'res','-o',BUILD/'resources.zip'])
run([TOOLS/('aapt2'+EXE),'link','-o',BUILD/'base.apk','-I',ANDROID,'--manifest',ROOT/'AndroidManifest.xml',
     '--java',BUILD/'gen','--min-sdk-version','26','--target-sdk-version','35','--version-code','100','--version-name',VERSION,'-0','wav',BUILD/'resources.zip'])
sources=list((ROOT/'src').rglob('*.java'))+list((BUILD/'gen').rglob('*.java'))
run([JDK/'bin'/('javac'+EXE),'--release','8','-encoding','UTF-8','-classpath',ANDROID,'-d',BUILD/'classes',*sources])
classes=BUILD/'classes.zip'
with zipfile.ZipFile(classes,'w',zipfile.ZIP_DEFLATED) as archive:
    for path in sorted((BUILD/'classes').rglob('*.class')):archive.write(path,path.relative_to(BUILD/'classes').as_posix())
run([JDK/'bin'/('java'+EXE),'-cp',TOOLS/'lib'/'d8.jar','com.android.tools.r8.D8','--lib',ANDROID,'--min-api','26','--output',BUILD/'dex',classes])
with zipfile.ZipFile(BUILD/'base.apk','a',zipfile.ZIP_DEFLATED) as archive:
    for path in sorted((BUILD/'dex').glob('*.dex')):archive.write(path,path.name)
run([TOOLS/('zipalign'+EXE),'-f','4',BUILD/'base.apk',BUILD/'aligned.apk'])
secret_file=SIGN/'store-password.txt'
if not secret_file.exists():secret_file.write_text(secrets.token_urlsafe(32),encoding='ascii')
environment['PERFECTA_STOREPASS']=secret_file.read_text(encoding='ascii').strip()
keystore=SIGN/'perfecta-lan.p12'
if not keystore.exists():
    run([JDK/'bin'/('keytool'+EXE),'-genkeypair','-keystore',keystore,'-storetype','PKCS12','-alias','perfecta-lan',
         '-storepass:env','PERFECTA_STOREPASS','-keypass:env','PERFECTA_STOREPASS','-keyalg','RSA','-keysize','3072',
         '-validity','10000','-dname','CN=Perfecta LAN Community, O=Community','-noprompt'])
apk=RELEASE/('Perfecta-LAN-Community-'+VERSION+'.apk')
run([JDK/'bin'/('java'+EXE),'-jar',TOOLS/'lib'/'apksigner.jar','sign','--ks',keystore,'--ks-key-alias','perfecta-lan',
     '--ks-pass','env:PERFECTA_STOREPASS','--key-pass','env:PERFECTA_STOREPASS','--out',apk,BUILD/'aligned.apk'])
verification=run([JDK/'bin'/('java'+EXE),'-jar',TOOLS/'lib'/'apksigner.jar','verify','--verbose','--print-certs',apk]).stdout
digest=hashlib.sha256(apk.read_bytes()).hexdigest()
(RELEASE/(apk.name+'.sha256')).write_text(digest+'  '+apk.name+'\n',encoding='ascii')
(RELEASE/'manifest.json').write_text(json.dumps({'version':VERSION,'package':'org.suplaforum.perfectalan','bytes':apk.stat().st_size,
    'sha256':digest,'min_android':8,'target_api':35,'firmware_api':'Perfecta-ESP Community 1.0.0','contains_panel_credentials':False,
    'signing_verified':True,'physical_device_verified':False},indent=2),encoding='utf-8')
(RELEASE/'signature-verification.txt').write_text(verification,encoding='utf-8')
print(json.dumps({'apk':str(apk),'bytes':apk.stat().st_size,'sha256':digest}))
