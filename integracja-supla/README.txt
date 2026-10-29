MODUŁ INTEGRACJI Z SUPLA
=======================
src/supla_bridge.cpp: 13 kanałów, potwierdzenie stanu z UART, kolejka jawnych
poleceń ON/OFF do przekaźników i powiadomienia.
src/push_schedule.cpp: zegar, zapis NVS i reguły PUSH.
include/push_schedule.h: logika dzień/noc i harmonogramów tygodniowych.

Kanały i uruchomienie: ../docs/URUCHOMIENIE.txt.
Dane SUPLA dostarcza wygenerowany lokalnie private_supla.h.
Serwer, e-mail, CA, nazwa TLS i ewentualny pin pochodzą od użytkownika.
Weryfikacja TLS jest aktywna. Nie ma automatycznego przejścia na połączenie
bez TLS. Moduł nie przechowuje tokenu REST Androida.

Urządzenie nie przywraca po restarcie ostatniego polecenia jako impulsu.
Wartość kanału sterującego jest potwierdzana rzeczywistym odczytem stref.
Próba wysłania impulsu może zostać odrzucona, gdy dane nie są świeże, trwa
zapis konfiguracji, pracuje most lub obowiązuje blokada przekaźników.
