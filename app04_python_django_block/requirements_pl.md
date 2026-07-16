
# ROLA 1: ZGŁOŚ SIĘ JAKO GŁÓWNY ARCHITEKT SYSTEMÓW I WŁAŚCICIEL PRODUKTU (ENTERPRISE SYSTEM ARCHITECT & PO)

## CEL
Twoim zadaniem jest przygotowanie kompleksowej architektury systemu, specyfikacji wymagań (Frontend i Backend) oraz epików i User Stories dla legalnej, edukacyjno-funkcjonalnej aplikacji opartej na technologii Blockchain 3.0, NFT i Smart Kontraktach. Aplikacja ma stanowić przyczółek (MVP) dla zaawansowanego, zdecentralizowanego systemu, zbudowanego na Django i Python.
Aplikacja musi być z przełącznikiem, aby wyświetlać ją w języku polskim lub w angielskim za pomocą prostego przełącznika z flagą.

## ZADANIA DO WYKONANIA W TYM KROKU:
1. **User Stories & Epics:** Zdefiniuj profesjonalne User Stories (w formacie: *Jako [rola], chcę [akcja], aby [korzyść]*), pokrywające:
   - Funkcjonalny, legalny rejestr general ledger (blockchain).
   - Tworzenie i certyfikację cyfrowego posiadania (NFT) i tożsamości twórcy (SSI - Self-Sovereign Identity).
   - Mechanizmy Crowdfundingu.
   - Synchronizację rejestru dla dwóch zdalnych użytkowników z zachowaniem prywatności (Zero-Knowledge Proofs).
2. **Wymagania Backend (Django i Python):** Opisz architekturę implementacji kryptografii post-kwantowej (NIST PQC, QKD), zarządzania węzłami, algorytmów konsensusu i silnika Smart Kontraktów dla medycyny/rolnictwa.
3. **Wymagania Frontend:** Zaprojektuj architekturę modułu edukacyjnego połączonego z żywym kodem. Frontend musi wizualizować: bloki, łańcuchy, transakcje, węzły, tokenomię oraz wyjaśniać rodzaje konsensusu (PoW, PoS, Hybrydowe, DPoS, PoReputation, PoUtility, BFT, PoH, PoET).
4. **Connectors & Skills:** Wylistuj wymagane konektory do integracji (np. WebSockets dla P2P, BouncyCastle PQC provider, IPFS Connector dla metadanych NFT, REST/GraphQL) oraz "Skills" (umiejętności AI/Agentów), które będą potrzebne do implementacji (np. `quantum-crypto-engineering`, `spring-boot-ledger-management`, `react-dapp-visualization`).

## FORMAT ZWRACANY
Użyj profesjonalnego języka inżynierii oprogramowania (UML opisowy, Markdown). Skup się na legalności, transparentności i cyberbezpieczeństwie. Zakończ ten etap gotową listą wymagań przygotowaną do implementacji kodu w kolejnym kroku.
Aplikacja musi być z przełącznikiem, aby wyświetlać ją w języku polskim lub w angielskim za pomocą prostego przełącznika z flagą.

#


# ROLA 2: ZGŁOŚ SIĘ JAKO GŁÓWNY INŻYNIER BLOCKCHAIN, KRYPTOGRAFII I BACKENDU (LEAD Django i Python BLOCKCHAIN ENGINEER)

## CEL
Na podstawie zatwierdzonej architektury, zaimplementuj fundamenty legalnego rejestru rozproszonego (Blockchain 3.0) w Django i Python. Kod ma działać jako faktyczny rejestr (General Ledger), a nie tylko atrapa.
Aplikacja musi być z przełącznikiem, aby wyświetlać ją w języku polskim lub w angielskim za pomocą prostego przełącznika z flagą.

## WYMAGANIA IMPLEMENTACYJNE (KOD DO NAPISANIA):
1. **Blockchain Core & Ledger:** Stwórz struktury danych dla Bloków i Łańcucha. Zaimplementuj mechanizm dodawania bloków i nagród (tokenomia). 
2. **Kryptografia Post-Kwantowa (PQC):** Zaimplementuj zabezpieczenia przed komputerami kwantowymi. Zintegruj biblioteki (np. Bouncy Castle PQC), aby zasymulować lub wdrożyć algorytmy zgodne z NIST (np. Dilithium, Kyber). Zastosuj mechanizmy inspirowane QKD (Quantum Key Distribution) dla zabezpieczenia komunikacji między węzłami.
3. **Smart Kontrakty (Blockchain 3.0):** Zaimplementuj legalny interfejs Smart Kontraktu, który można zaadaptować dla:
   - Instytucji medycznych (zarządzanie historią pacjenta w oparciu o zgodę).
   - Rolnictwa (transparentność łańcucha dostaw).
4. **Konsensus i Skalowalność:** Przygotuj wzorce projektowe (Strategy Pattern) dla różnych algorytmów konsensusu, kładąc nacisk na BFT (Byzantine Fault Tolerance) oraz PoS.
5. **Synchronizacja P2P:** Zaimplementuj mechanizm synchronizacji pełnego rejestru dla 2 zdalnych węzłów połączonych przez sieć. Użyj WebSockets lub gRPC. Zabezpiecz kanał komunikacyjny przed atakami Man-In-The-Middle (MITM) oraz wyjaśnij w komentarzach do kodu (Javadoc), jak algorytmy te zapewniają zaufanie stron bez ujawniania ich pełnej tożsamości.

## ZASADY KODOWANIA
Kod musi być produkcyjny, używać rekordów (Records z Javy), wirtualnych wątków (Project Loom) i być ściśle udokumentowany, z naciskiem na wyjaśnienie "dlaczego ta technologia jest legalna i bezpieczna". Koduj w sposób zoptymalizowany.
Aplikacja musi być z przełącznikiem, aby wyświetlać ją w języku polskim lub w angielskim za pomocą prostego przełącznika z flagą.


#


# ROLA 3: ZGŁOŚ SIĘ JAKO GŁÓWNY PROGRAMISTA FRONTENDU I TECHNICZNY EDUKATOR (LEAD FRONTEND & EDU-TECH DEV)

## CEL
Twoim zadaniem jest stworzenie warstwy interfejsu użytkownika (React/Vue/Angular - zaproponuj optymalny), który będzie konsumował API z napisanego wcześniej backendu Spring Boot. Interfejs musi być "żywą demonstracją" technologii blockchain oraz interaktywną encyklopedią.
Aplikacja musi być z przełącznikiem, aby wyświetlać ją w języku polskim lub w angielskim za pomocą prostego przełącznika z flagą.

## WYMAGANIA FUNKCJONALNE FRONTENDU:
1. **Wizualizacja Rejestru (General Ledger):** Stwórz dynamiczny widok pokazujący tworzenie bloków, łańcuchy, hasze kryptograficzne oraz sieć P2P (komunikacja między dwoma zdalnymi węzłami). 
2. **Sekcja Edukacyjna - Mechanizmy Konsensusu:** Zaimplementuj interaktywne komponenty UI (np. tooltipy, akordeony, animacje), które szczegółowo i przystępnie tłumaczą:
   - Dowód Pracy (PoW), Dowód Stawki (PoS), Hybrydowe, Dowody Reputacji (PoReputation), Dowody Użyteczności (PoUtility), BFT, Dowód Historii (PoH), Dowód Upływu Czasu (PoET).
3. **Sekcja Edukacyjna - Cyberbezpieczeństwo i PQC:** Przygotuj widoki, które wizualizują, jak działa kryptografia post-kwantowa (NIST PQC, QKD) w tej aplikacji w porównaniu do standardowych kryptowalut (jak Bitcoin). Wyjaśnij mechanizmy zabezpieczeń przed złośliwymi aktorami.
4. **Moduł Zastosowań Gospodarczych (Use Cases):** Stwórz panele informacyjno-funkcjonalne demonstrujące użyteczność i legalność wprowadzonych rozwiązań (Smart Kontrakty, NFT, Tokeny) dla sektorów:
   - Przemysły kreatywne i zarządzanie prawami autorskimi (DRM).
   - Logistyka i łańcuch dostaw (transparentność).
   - Medycyna i Rolnictwo.
   - Szkolnictwo wyższe (weryfikacja dyplomów).
   - Demokracja (projekt zdalnego, legalnego głosowania).
   - Społeczności blockchainowe i systemy zarządzania przedsiębiorstwami społecznymi.

## OCZEKIWANY WYNIK:
Wygeneruj strukturę komponentów frontendu, kod głównych widoków oraz szczegółowe teksty (w języku polskim), które znajdą się w aplikacji jako element wyjaśniający decentralizację, suwerenną tożsamość cyfrową i skalowalność.
Aplikacja musi być z przełącznikiem, aby wyświetlać ją w języku polskim lub w angielskim za pomocą prostego przełącznika z flagą.
