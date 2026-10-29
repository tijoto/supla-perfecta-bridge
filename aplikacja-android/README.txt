PERFECTA LAN COMMUNITY — ANDROID
==============================
Pakiet: org.suplaforum.perfectalan; Android 8+, docelowe API 35.
Kompilacja: python build.py po ustawieniu JAVA_HOME i ANDROID_HOME.
Wymagane JDK 17, Android Platform 35, Build Tools 36.0.0.

Źródła: src/org/suplaforum/perfectalan. Hostowe testy logiki i transportu
uruchamiane są automatycznie podczas kompilacji. APK trafia do release/.
Klucz podpisywania powstaje w lokalnym .signing/ i nie należy do repozytorium.

Połączenie LAN podaje użytkownik. Cloud i numer urządzenia domyślnie są puste.
Tryb Tylko LAN pozwala zapisać połączenie bez serwera, numeru i tokenu SUPLA.
Tryby Auto/SUPLA wymagają własnego HTTPS Cloud, numeru urządzenia i tokenu.
Hasło ESP oraz token są szyfrowane Android Keystore/AES-GCM.

Token potrzebuje Urządzenia/Odczyt, Kanały/Odczyt i Wykonywanie akcji.
Modyfikacja ustawień wejść jest dostępna wyłącznie po LAN i wymaga włączenia
tej funkcji w firmware. Pole Blokada wyświetla stan, nie zmienia blokady.

Alarm telefonu jest monitorowaniem w tle oraz zapętlonym dźwiękiem i wibracją.
Skonfiguruj zgody Androida, stałą pracę w tle i głośność alarmów. Szczegóły
oraz próba dźwięku: ../docs/URUCHOMIENIE.txt.
