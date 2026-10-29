# PERFECTA + SUPLA + ESP32

Otwarty projekt odczytu SATEL PERFECTA 16-WRL 1.05, integracji SUPLA i aplikacji Android.

**Zacznij od [README.txt](README.txt)** — kompletna instrukcja w języku polskim.

[Pobierz APK i archiwum wydania](https://github.com/tijoto/supla-perfecta-bridge/releases/latest).

| Moduł | Zawartość |
|---|---|
| [Komunikacja PERFECTA](komunikacja-perfecta) | UART, ramki, odczyt, most i kontrolowany zapis przypisań |
| [Integracja SUPLA](integracja-supla) | 13 kanałów, sterowanie przekaźnikami, PUSH i harmonogramy |
| [Aplikacja Android](aplikacja-android) | LAN/SUPLA, podgląd i alarm dźwiękowy w tle |
| [Firmware ESP32](firmware-esp32) | WWW, OTA, obsługa sprzętu i wspólna kompilacja |

## Uruchomienie

1. [Połączenia i przekaźniki](docs/OKABLOWANIE.txt).
2. [Konfiguracja ESP, SUPLA i telefonu](docs/URUCHOMIENIE.txt).
3. [Pełny README.txt](README.txt) — kompilacja i zasady zmian ustawień.
4. [API](docs/API.txt), [testy](docs/WERYFIKACJA.txt), [przygotowany post na forum](docs/POST_FORUM_SUPLA.txt).
5. [Post gotowy do wklejenia w phpBB — BBCode](docs/POST_FORUM_PHPBB.txt).

Konfiguracja jest lokalna. Repozytorium nie zawiera danych instalacji.
Sterowanie, zapis przypisań i most są domyślnie wyłączone. Pierwsze uruchomienie służy odczytowi.

Uzbrajanie i rozbrajanie wykonują dwa przekaźniki. GPIO ESP nie łączy się bezpośrednio z wejściami Z centrali.

Niezależny projekt społecznościowy. Licencja [GPL-2.0-or-later](LICENSE).
