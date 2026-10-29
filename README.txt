PERFECTA + SUPLA + ESP32 — PROJEKT SPOŁECZNOŚCIOWY 1.0.0
=====================================================

Odczyt centrali SATEL PERFECTA 16-WRL przez port RS-232 (TTL), kanały SUPLA,
sterowanie przez dwa przekaźniki i aplikacja Perfecta LAN Community na Androida.
To niezależny projekt społecznościowy. Nie jest produktem SATEL ani SUPLA.

POTWIERDZONA BAZA SPRZĘTOWA
-------------------------
Kod bazowy pracował z PERFECTA 16-WRL, firmware 1.05, klasycznym ESP32 z 4 MB
Flash oraz telefonem z Androidem. To jest zakres tej publikacji. Program
sprawdza identyfikację centrali; nie rozszerzaj jej na inny model przez samo
wyłączenie tej kontroli. Publiczne wydanie ma własną konfigurację i pakiet APK.
Dokładny zakres testów wydania znajduje się w docs/WERYFIKACJA.txt.

CO DZIAŁA
---------
* Odczyt czuwania i alarmu dwóch stref oraz numeru źródła alarmu.
* Stany wejść 1–16, nazwy i przypisania odczytywane z centrali.
* Naruszenie, blokada odczytana z centrali, komunikat słabej baterii i RF.
* Daty ostatniego zaobserwowanego naruszenia i komunikatu słabej baterii.
* WWW z podglądem oraz konfiguracją harmonogramów PUSH.
* 13 kanałów SUPLA: czuwanie S1/S2, alarm S1/S2, wejścia 9–16, Alarm ON/OFF.
* PUSH: alarmy, zmiany czuwania, naruszenia i słabe baterie; 20 reguł.
* Osobne tryby powiadomień: zawsze, dzień, noc, harmonogram tygodniowy.
* Jeden przełącznik Alarm ON/OFF. Rzeczywisty stan pochodzi z UART.
* Most TCP RAW dla lokalnego PERFECTA Soft, z jednym uprawnionym komputerem.
* OTA z logowaniem i kontrolą sesji; narzędzie OTA sprawdza stan centrali.
* Android: LAN, SUPLA przez internet lub automatyczne LAN → SUPLA.
* Android: monitorowanie w tle, zapętlony dźwięk alarmu i wibracja.
* Android/LAN: checkbox S1 dla wejścia Z7 oraz edycja przypisań czujek S1/S2.

WAŻNE ROZRÓŻNIENIA
------------------
UART służy do odczytu, mostu i opisanych zmian przypisań. Załączenie oraz
wyłączenie czuwania realizują styki dwóch przekaźników na wejściach centrali.
Nie ma tu polecenia uzbrajania wysyłanego bezpośrednio protokołem UART.

Pole „Blokada” w aplikacji jest odczytem. Blokadę ustawia się manipulatorem.
Odznaczenie S1/S2 nie usuwa rejestracji radiowej ani usterki czujki.

RF to odczyt siły/jakości sygnału z centrali. Nie jest poziomem baterii ani
dowodem naruszenia. Bateria jest raportowana jako komunikat słabej baterii.
Daty naruszeń są historią obserwacji programu, nie pełnym logiem zdarzeń centrali.
Nie każde przejście człowieka powoduje nową transmisję czujki bezprzewodowej.

W trybie SUPLA aplikacja odczytuje opublikowane kanały i wykonuje ON/OFF.
Edycja wejść i harmonogramów urządzenia pozostaje lokalna. Numer źródła alarmu,
historia naruszeń i RF nie są odtwarzane przez zdalny adapter Androida.

PODZIAŁ PROJEKTU
---------------
komunikacja-perfecta/  — ramki, szyfrowanie, odczyt UART, most, zapis przypisań
integracja-supla/      — kanały, komendy przekaźników, PUSH i harmonogramy
aplikacja-android/     — aplikacja Android, źródła, zasoby i skrypt kompilacji
firmware-esp32/        — wspólny program, WWW, OTA, piny i PlatformIO
tools/                — konfiguracja lokalna, OTA, testy i kontrola publikacji
docs/                 — okablowanie, uruchomienie, API, testy i post na forum

ŻADNYCH DANYCH INSTALACJI W REPOZYTORIUM
--------------------------------------
config.example.json jest pustym wzorem. Generator tworzy config.local.json
oraz private_config.h i private_supla.h. Pliki te są ignorowane przez Git.
Każde urządzenie otrzymuje własne GUID i AUTHKEY SUPLA. Nie zmieniaj ich przy
zwykłej aktualizacji. Nie kopiuj konfiguracji jednego ESP do kolejnego ESP.
Token Androida wpisuje użytkownik w aplikacji; nie trafia on do źródeł APK.

Własny firmware zawiera dane użyte podczas jego kompilacji. Nie publikuj
własnego pliku BIN, kopii Flash/NVS, config.local.json ani nagłówków private_*.
Katalog .signing zawiera lokalny klucz podpisywania APK — również go nie publikuj.

URUCHOMIENIE — KOLEJNOŚĆ
-----------------------
1. Zachowaj kopię ustawień centrali w PERFECTA Soft. Najpierw przygotuj odczyt.
2. Przeczytaj docs/OKABLOWANIE.txt. Nie łącz wejść Z z GPIO ESP.
3. Zainstaluj Python 3.10+, Git oraz PlatformIO Core:
     python -m pip install platformio
4. Z katalogu projektu uruchom:
     python tools/configure.py --interactive
   Wpisz swoje Wi-Fi, 16-znakowy klucz komunikacji centrali i własne hasło WWW.
   Na pierwszy odczyt pozostaw sterowanie, zapis ustawień i most wyłączone.
   Jeżeli nie przygotowałeś jeszcze SUPLA, pozostaw integrację wyłączoną.
5. Skompiluj:
     pio run -d firmware-esp32
6. Wgraj przez USB, wybierając rzeczywisty port tego ESP:
     pio run -d firmware-esp32 -t upload --upload-port PORT_ESP
   PORT_ESP zastąp nazwą portu z komputera. Projekt nie wybiera portu za Ciebie.
7. Otwórz monitor portu 115200. Odczytaj adres nadany ESP przez Twój router.
   Otwórz stronę urządzenia albo http://perfecta-esp.local/ i zaloguj się danymi
   z sekcji web swojego config.local.json. mDNS musi działać w Twojej sieci.
8. Po sprawdzeniu stanu obu stref i wejść skonfiguruj SUPLA według
   docs/URUCHOMIENIE.txt. Certyfikat i tożsamość serwera weryfikowane są TLS.
9. Dopiero po przygotowaniu wejść i przekaźników włącz controls.enabled,
   ponownie uruchom generator, skompiluj i wgraj swój program.
10. Zainstaluj APK Community. W aplikacji wybierz Połączenie i wpisz własny
    adres ESP, login i hasło. Uruchomienie samego LAN nie wymaga danych SUPLA.

ZMIANA Z7 I PRZYPISAŃ CZUJEK
--------------------------
Opcja perfecta.configuration_writes domyślnie ma wartość false. Włącza lokalny
zapis przypisań po UART. Obie strefy muszą być rozbrojone, alarm skasowany,
port wolny, a przy zmianie Z7 wejście Z7 musi być nienaruszone.

Checkbox „Uzbrajaj” w prawym górnym rogu karty Strefa 1 dotyczy Z7:
zaznaczony — Z7 ma przypisanie S1 + S2; odznaczony — tylko S2.
To działa dla Z7 z obwodem NO, reakcją 14 i zachowanym przypisaniem do S2.
Sprawdź także przypisanie wejścia rozbrajającego Z8 do odpowiednich stref.

Zapis Z7 sprawdza i utrwala dwa bloki po 128 B, zapisuje jeden zmieniony bajt,
zatwierdza i porównuje oba bloki kontrolnie. Pozostałych bloków ten szybki
tryb nie weryfikuje. Przypisania zwykłych czujek zachowują kontrolę całych 7168 B.
Komendy zapisu nie są automatycznie ponawiane po niepewnym potwierdzeniu.
Pełny odczyt i kopia konfiguracji pozostają osobną operacją API prepare.

KOMPILACJA APK
--------------
Wymagane: JDK 17, Android SDK Platform 35, Build Tools 36.0.0.
Ustaw JAVA_HOME i ANDROID_HOME zgodnie z własną instalacją. Następnie:
     python aplikacja-android/build.py
APK powstaje w aplikacja-android/release/. Program tworzy własny lokalny klucz
podpisywania w .signing. Zachowaj go prywatnie do kolejnych swoich wydań APK.
Pakiet aplikacji: org.suplaforum.perfectalan. Domyślne pola Cloud i numeru
urządzenia są puste. Nie ma wbudowanego tokenu ani danych centrali.

AKTUALIZACJA OTA
---------------
Przy rozbrojonych strefach i zamkniętym PERFECTA Soft:
     python tools/ota.py --image firmware-esp32/.pio/build/esp32dev/firmware.bin
Podaj adres swojego ESP. Narzędzie pobierze login/hasło z lokalnej konfiguracji,
sprawdzi stan przed wysłaniem i porówna sumę działającego programu po restarcie.
Można również użyć lokalnej strony /update po zalogowaniu.

TESTY I PUBLIKACJA
-----------------
     python -m unittest discover -s tests
     python tools/audit_public.py
Test C++ komunikacji:
     c++ -std=c++17 -Ikomunikacja-perfecta/tests/host-stubs \
       -Ikomunikacja-perfecta/include komunikacja-perfecta/tests/z7_fast_test.cpp \
       -o z7-test
     ./z7-test
Test harmonogramów analogicznie: integracja-supla/tests/push_schedule_test.cpp,
z dodatkowym -Iintegracja-supla/include. Na Windows użyj dostępnego kompilatora
C++17 i odpowiedniej nazwy programu wykonywalnego.

CI generuje tylko konfigurację testową z BUILD_TEST_ONLY; taki obraz nie
uruchamia sieci ani połączenia z centralą i nie służy do instalacji.
Instrukcja publikacji i zakres testów: docs/PUBLIKACJA.txt i WERYFIKACJA.txt.

LICENCJA I ZALEŻNOŚCI
--------------------
Kod projektu: GPL-2.0-or-later, tekst w LICENSE. Oficjalne SDK SUPLA 26.9.2
pobierane jest z przypiętego commitu. Nie dołączamy firmware SATEL,
instalatorów producenta, zrzutów pamięci ani zapisów komunikacji z instalacji.
Szczegóły: THIRD_PARTY.txt. Źródła dokumentacji: docs/ZRODLA.txt.
