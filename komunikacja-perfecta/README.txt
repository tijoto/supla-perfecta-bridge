MODUŁ KOMUNIKACJI Z PERFECTĄ
===========================
src/monitor.cpp: sesja UART, kolejka zapytań, odczyt stanu/diagnostyki/RF,
wybranych ustawień, most TCP i kontrolowane transakcje przypisań wejść.
include/protocol.h: parser ramek, CRC i szyfrowanie/dekodowanie.
include/z7_policy.h: adresy i maski dla przypisań stref.
include/test_vectors.h: syntetyczne wektory testowe; nie są kluczem instalacji.

Wymagany model/wersja: PERFECTA 16-WRL 1.05. UART: 19200, 8N1.
GPIO i połączenia opisano w ../docs/OKABLOWANIE.txt.

Sterowanie czuwaniem wykonują przekaźniki wspólnego firmware, nie ten protokół.
Testy w tests/ kompilują rzeczywisty monitor.cpp z atrapami sprzętu i NVS.
Nie otwierają portów, sieci ani połączeń z centralą.
