"""Small, reproducible Arduino-ESP32 2.x compatibility changes for SUPLA SDK 26.9.2.

Upstream files remain GPL-2.0-or-later, AC SOFTWARE SP. Z O.O.
No upstream security settings or application logic are disabled.
"""
from pathlib import Path

def replace_once(text,old,new):
    if new in text:return text
    if text.count(old)!=1:raise RuntimeError('SDK compatibility patch does not match the pinned source')
    return text.replace(old,new,1)
def patch(folder):
    p=folder/'src/supla/crypto.cpp';t=p.read_text(encoding='utf-8')
    t=replace_once(t,'#include <mbedtls/error.h>','#include <mbedtls/error.h>\n#include <mbedtls/version.h>')
    t=replace_once(t,'  int ret = mbedtls_pkcs5_pbkdf2_hmac_ext(','''#if MBEDTLS_VERSION_NUMBER < 0x03000000
  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  int ret=mbedtls_md_setup(&context,mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),1);
  if(ret==0)ret=mbedtls_pkcs5_pbkdf2_hmac(&context,
      reinterpret_cast<const unsigned char *>(password),strlen(password),
      salt,saltLen,iterations,derivedKeyLen,derivedKey);
  mbedtls_md_free(&context);
#else
  int ret = mbedtls_pkcs5_pbkdf2_hmac_ext(''')
    t=replace_once(t,'      derivedKey);\n  if (ret != 0)','      derivedKey);\n#endif\n  if (ret != 0)')
    p.write_text(t,encoding='utf-8',newline='\n')
    p=folder/'src/supla/io.cpp';t=p.read_text(encoding='utf-8')
    for before,legacy in [('analogWriteFrequency(pin, pwmFrequency);','analogWriteFrequency(pwmFrequency);'),('analogWriteResolution(pin, resolutionBits);','analogWriteResolution(resolutionBits);')]:
        t=replace_once(t,'  '+before,'#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR < 3\n  (void)(pin);\n  '+legacy+'\n#else\n  '+before+'\n#endif')
    p.write_text(t,encoding='utf-8',newline='\n')

if 'Import' in globals():
    Import('env')
    folder=Path(env.subst('$PROJECT_LIBDEPS_DIR'))/env.subst('$PIOENV')/'SuplaDevice'
    patch(folder)
    # SDK 26.9.2 compiles its EEPROM backend without declaring the framework
    # EEPROM include dependency. The firmware's lib_deps supplies its object code.
    framework=Path(env.PioPlatform().get_package_dir('framework-arduinoespressif32'))
    env.Append(CPPPATH=[str(framework/'libraries/EEPROM/src')])
