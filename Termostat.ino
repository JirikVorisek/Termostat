#include <DHT.h>                     // Knihovna pro práci s Teploměrem/Vlhkoměrem
#include "RTClib.h"                  // Knihovna pro práci s modulem Reálného času
#include <ESP8266WebServer.h>        // Knihovna pro práci s ESP8266 Web Serverem
#include <ESP8266WiFi.h>             // Knihovna pro připojení ESP8266 k WiFi síti
#include <EEPROM.h>                  // Knohovna pro čtení a zápis do EEPROM
#include <NTPClient.h>               // Knihovna pro komunikaci s NTP Serverem (pro získání přesného času z internetu)
#include <WiFiUdp.h>                 // Knihovna pro ovládání síťové komunikace na úrovni UDP

#define RELE_PIN D7                   // Definice PINu pro Relé
#define DHT_PIN D6                    // Definice PINu pro DHT Sensor (Teploměr/Vlhkoměr)

RTC_DS1307 rtc;                                                 // Objekt pro práci s Hodinami reálného času
DHT dht(DHT_PIN, DHT22);                                        // Objekt pro práci s Teploměrem/Vlhkoměrem
ESP8266WebServer server(80);                                    // Objekt pro práci s WebServerem
WiFiClient  client;                                             // Objekt pro práci s Wifi klienty
WiFiUDP ntpUDP;                                                 // Objekt pro práci s pakety na úrovni UDP - pro NTP
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600);      // Objekt pro práci s NTP API. Poslední parametr udává posun v sekundách oproti UTC

// Globální proměnné a konstanty
char denTydne[7][12] {"Nedele", "Pondeli", "Utery", "Streda", "Ctvrtek", "Patek", "Sobota"};       // Pole pro převod čísla dne v týdnu na název
const char* ssid = "Network2";                                                                     // Nastavení připojení k Wifi - SSID
const char* password = "ondrout5";                                                                 // Nastavení připojení k Wifi - heslo
byte posledniCisloPlanu;                                                                           // Poslední použité číslo plánu.
unsigned long milis;                                                                               // hodnota milisekund od spuštění - pro přerušení while cyklů
unsigned long milis2;                                                                              // hodnota milisekund od spuštění - pro omezení loop
// Inicializace ESP8266
void setup() {
  // Nastavení výstupních pinů
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage LOW
  pinMode(RELE_PIN, OUTPUT);          // Nastavení PIN pro Relé jako výstup
  digitalWrite(RELE_PIN, HIGH);       // Po restartu vždy vypnout relé

  // Zahajeni komunikace pres Seriovou linku
  Serial.begin(115200);
  Serial.println();

  // Zahájení komunikace s EEPROM pamětí
  EEPROM.begin(512);

  // Připojení ESP8266 k WiFi síti
  Serial.print("Connecting to : ");
  Serial.println(ssid);
  IPAddress staticIP(192, 168, 15, 32);         // ESP8266 Statická IP
  IPAddress gateway(192, 168, 15, 1);           // IP Adresa Gateway (Router)
  IPAddress subnet(255, 255, 255, 0);           // Maska podsítě
  IPAddress dns(8, 8, 8, 8);                    // DNS (Google)
  WiFi.mode(WIFI_STA);                          // Nastaví WiFi modul do modu Stanice
  WiFi.config(staticIP, gateway, subnet, dns);  // Nastaví statickou IP
  WiFi.begin(ssid, password);
  milis = millis();
  while (WiFi.status() != WL_CONNECTED) {   // Připojení chvíli trvá - Kontrola jestli už se podařilo připojit
    if (millis() - milis > 10000) break;      // Aby se nepřipojoval donekonečna přerušit po 10 sekundách připojování

    delay(500);
    Serial.print(".");
  }

  // Nastartování ESP8266 Web Serveru
  server.begin();
  Serial.println("Server started");       // Informace, že server naběhl
  Serial.print("Server IP: ");            // Vypíše přidělenou IP adresu
  Serial.println(WiFi.localIP());

  // Definice URL a spouštěných funkcí
  server.on("/", homePage);               // Zobrazit ovládání definované na hlavní stránce
  server.onNotFound(strankaNeExistuje);   // Akce pokud je zadána neexistující volba
  server.on("/casovac", nastavCasovac);   // Časovač
  server.on("/plan", nastavPlan);         // Nastavit plán a uložit do EEPROM
  server.on("/vymaz", vymazatNastaveni);  // Vymazat nastavení z EEPROM
  server.on("/on", zapnout);              // Zapnout
  server.on("/off", vypnout);             // Vypnout
  server.on("/data", zobrazData);         // Zobrazit aktuální data z Arduina
  server.on("/cas", nastavCas);           // Načíst datum a čas z internetu

  // zahájení komunikace se senzorem DHT (Teploměr a vlhkoměr)
  dht.begin();

  // Výpis Teploty a Vlkosti z DHT sensoru
  float teplota = dht.readTemperature();
  float vlhkost = dht.readHumidity();
  if (isnan(teplota) || isnan(vlhkost)) {
    Serial.println("Chyba při čtení z DHT senzoru!");
  }
  else {
    Serial.print("Teplota: ");
    Serial.print(teplota);
    Serial.print(" stupnu Celsia, ");
    Serial.print("vlhkost: ");
    Serial.print(vlhkost);
    Serial.println("% RH.");
  }

  // Zahájení komunikace a Kontrola RTC
  if (! rtc.begin()) Serial.println("RTC nenalezeno");
  else Serial.println("RTC nalezeno");
  if (! rtc.isrunning()) Serial.println("RTC neběží");
  else Serial.println("RTC běží");

  // Výpis aktuálního data a času z RTC
  DateTime ted = rtc.now();
  Serial.print("Aktuální čas: ");
  Serial.print(ted.year(), DEC);
  Serial.print("-");
  Serial.print(ted.month(), DEC);
  Serial.print("-");
  Serial.print(ted.day(), DEC);
  Serial.print("   ");
  Serial.print(ted.hour(), DEC);
  Serial.print(":");
  Serial.print(ted.minute(), DEC);
  Serial.print(":");
  Serial.println(ted.second(), DEC);

  // Zahájení komunikace s NTP
  timeClient.begin();
}

// Nekonečná smyčka
void loop() {
  server.handleClient();                              // pravidelné volání detekce klienta

  if (abs(millis() - milis2 > 1000)) {                // Omezení na preovedení každou sekundu
    milis2 = millis();                                // Reset odpočtu

    // Reconnect pokud spojení bylo přerušeno
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected");
      digitalWrite(LED_BUILTIN, LOW);                 // Zapnutí In-buils LED
    }
    else {
      digitalWrite(LED_BUILTIN, HIGH);                // Vypnutí In-buils LED
      Serial.println("Disconnected");
      WiFi.reconnect();                               // Pokusí se znovu připojit k síti
      milis = millis();                               // Uložení okamžiku, kdy se začal připojovat
      while (WiFi.status() != WL_CONNECTED) {         // Připojení chvíli trvá - Kontrola jestli už se podařilo připojit
        if (abs(millis() - milis > 10000)) break;     // Aby se nepřipojoval donekonečna přerušit po 10 sekundách připojování

        delay(500);
        Serial.print(".");
      }
    }

    // Zapnutí/Vypnutí relé podle nastavení Časovače, Plánu a Stavu
    if ((casovacSpusten() || naplanovano()) && EEPROM.read(0) == 1) {   // Pokud časovač stále běží nebo je naplánován a zároveň je stav zapnuto
      digitalWrite(RELE_PIN, LOW);                                      // Zapnout relé
    }
    else {
      digitalWrite(RELE_PIN, HIGH);                                     // Vypnout relé
    }
  }
}

// Funkce pro zjištění jestli podle Časovače má být Relé zapnuto
boolean casovacSpusten() {
  // Načteno z EEPROM
  DateTime casDo (EEPROM.read(1) + 2000, EEPROM.read(2), EEPROM.read(3), EEPROM.read(4), EEPROM.read(5), EEPROM.read(6));
  DateTime ted = rtc.now();

  // Kontrola, jestli konec časovače je v budoucnosti nebo již v minulosti.
  if (casDo > ted) {
    return true;
  }
  else {
    return false;
  }
}

// Funkce pro zjištění jestli podle alespoň jednoho plánu má být Relé zapnuto
boolean naplanovano() {
  float teplota = dht.readTemperature();

  milis = millis();                                 // Přiřazení aktuální hodnoty milisekund od spuštění - pro budoucí porovnání
  while (isnan(teplota)) {                          // Občas vrací "nan" místo hodnoty. Dokud nebude hodnota jiná než "nan" bude se ji snažit načíst ze sensoru znovu
    if (millis() - milis > 100) break;            // Aby se nepokoušel zjišťovat teplotu/vlhkost do nekonečna po 10 s ukončí cyklus

    teplota = dht.readTemperature();
  }

  int i = 7;                                        // Prvních 7 paměťových polí obsahuje základní nastavení. Zbytek je možné použít pro uložení plánů

  DateTime ted = rtc.now();

  // Najit alespon jeden plan
  while (EEPROM.read(i) > 0) {
    int casOd = EEPROM.read(i + 2) * 100 + EEPROM.read(i + 3); // Čas od plánu
    int casDo = EEPROM.read(i + 4) * 100 + EEPROM.read(i + 5); // Čas do plánu
    int tedPorovnani = ted.hour() * 100 + ted.minute();   // Aktuální čas
    float teplotaEEPROM = EEPROM.read(i + 6) / 5.0;
    float teplotaHranicni = teplotaEEPROM;

    // Korekce hraniční teploty aby se při kulminaci okolo hraniční teploty stále nezapínalo a nevypínalo Relé.
    if (digitalRead(RELE_PIN) == LOW) {                     // Jestliže je Relé už zapnuto
      teplotaHranicni = teplotaEEPROM + 0.5;                // zvýšit hraniční teplotu o 0.5°C
    }
    else {
      teplotaHranicni = teplotaEEPROM - 0.5;                // jinak snížit hraniční teplotu o 0.5°C
    }
    Serial.print("EEPROM - cislo v týdnu : ");
    Serial.println(EEPROM.read(i + 1));
    Serial.print("Ted - cislo v týdnu : ");
    Serial.println(ted.dayOfTheWeek());
    Serial.print("Cas Od : ");
    Serial.println(casOd);
    Serial.print("Cas Do : ");
    Serial.println(casDo);
    Serial.print("Ted - porovnani : ");
    Serial.println(tedPorovnani);
    Serial.print("Teplota mistnosti: ");
    Serial.println(teplota);
    Serial.print("Teplota - EEPROM: ");
    Serial.println(teplotaEEPROM);
    Serial.print("Teplota - Hranicni: ");
    Serial.println(teplotaHranicni);

    if (EEPROM.read(i + 1) == ted.dayOfTheWeek() && casOd <= tedPorovnani && casDo >= tedPorovnani && teplota <= teplotaHranicni) {
      return true;
    }
    i = i + 7;
  }
  return false;
}

// Funkce pro zapnutí plánu a časovače
void zapnout() {
  EEPROM.write(0, 1);
  homePage();
}

// Funkce pro vypnutí plánu a časovače
void vypnout() {
  EEPROM.write(0, 0);
  homePage();
}

// Vypočítat a nastavit a zapsat čas do EEPROM do kdy má časovač běžet
void nastavCasovac() {
  if (server.arg("cas") != "") {
    DateTime ted = rtc.now();
    DateTime casDo = ted + TimeSpan(0, server.arg("cas").substring(0, 2).toInt(), server.arg("cas").substring(3).toInt(), 0);

    EEPROM.write(1, casDo.year() - 2000);
    EEPROM.write(2, casDo.month());
    EEPROM.write(3, casDo.day());
    EEPROM.write(4, casDo.hour());
    EEPROM.write(5, casDo.minute());
    EEPROM.write(6, casDo.second());
    EEPROM.commit();
  }
  homePage();
}

// Přidat nastavení plánu a zapsat do EEPROM.
void nastavPlan() {
  float teplota;
  int teplotaInt;
  byte posledniByte = zjistitPosledniObsazenyByte();

  if (server.arg("casOd") < server.arg("casDo")) {
    if (server.arg("Pondeli") == "1") {
      EEPROM.write(posledniByte, posledniCisloPlanu);                         // Číslo plánu
      posledniByte++;
      EEPROM.write(posledniByte, 1);                                          // Číslo dne v týdnu - 1=Pondělí
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(0, 2).toInt()); // Čas Od - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(3, 5).toInt()); // Čas Od - minuty
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(0, 2).toInt()); // Čas Do - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(3, 5).toInt()); // Čas Do - minuty
      posledniByte++;
      teplota = server.arg("teplota").toFloat() * 5;
      teplotaInt = (int)teplota;
      EEPROM.write(posledniByte, teplotaInt);                                 // Teplota - příchozí hodnota se vynásobí 5x aby ukládaná hodnota byla mezi 0 a 255.
      posledniByte++;                                                         // To umožňuje uložit teplotu 0 - 51°C po kroku 0.2°C
    }
    if (server.arg("Utery") == "1") {
      EEPROM.write(posledniByte, posledniCisloPlanu);                         // Číslo plánu
      posledniByte++;
      EEPROM.write(posledniByte, 2);                                          // Číslo dne v týdnu - 2=Úterý
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(0, 2).toInt()); // Čas Od - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(3, 5).toInt()); // Čas Od - minuty
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(0, 2).toInt()); // Čas Do - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(3, 5).toInt()); // Čas Do - minuty
      posledniByte++;
      teplota = server.arg("teplota").toFloat() * 5;
      teplotaInt = (int)teplota;
      EEPROM.write(posledniByte, teplotaInt);                                 // Teplota - příchozí hodnota se vynásobí 5x aby ukládaná hodnota byla mezi 0 a 255.
      posledniByte++;                                                         // To umožňuje uložit teplotu 0 - 51°C po kroku 0.2°C
    }
    if (server.arg("Streda") == "1") {
      EEPROM.write(posledniByte, posledniCisloPlanu);                         // Číslo plánu
      posledniByte++;
      EEPROM.write(posledniByte, 3);                                          // Číslo dne v týdnu - 3=Středa
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(0, 2).toInt()); // Čas Od - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(3, 5).toInt()); // Čas Od - minuty
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(0, 2).toInt()); // Čas Do - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(3, 5).toInt()); // Čas Do - minuty
      posledniByte++;
      teplota = server.arg("teplota").toFloat() * 5;
      teplotaInt = (int)teplota;
      EEPROM.write(posledniByte, teplotaInt);                                 // Teplota - příchozí hodnota se vynásobí 5x aby ukládaná hodnota byla mezi 0 a 255.
      posledniByte++;                                                         // To umožňuje uložit teplotu 0 - 51°C po kroku 0.2°C
    }
    if (server.arg("Ctvrtek") == "1") {
      EEPROM.write(posledniByte, posledniCisloPlanu);                         // Číslo plánu
      posledniByte++;
      EEPROM.write(posledniByte, 4);                                          // Číslo dne v týdnu - 4=Čtvrtek
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(0, 2).toInt()); // Čas Od - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(3, 5).toInt()); // Čas Od - minuty
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(0, 2).toInt()); // Čas Do - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(3, 5).toInt()); // Čas Do - minuty
      posledniByte++;
      teplota = server.arg("teplota").toFloat() * 5;
      teplotaInt = (int)teplota;
      EEPROM.write(posledniByte, teplotaInt);                                 // Teplota - příchozí hodnota se vynásobí 5x aby ukládaná hodnota byla mezi 0 a 255.
      posledniByte++;                                                         // To umožňuje uložit teplotu 0 - 51°C po kroku 0.2°C
    }
    if (server.arg("Patek") == "1") {
      EEPROM.write(posledniByte, posledniCisloPlanu);                         // Číslo plánu
      posledniByte++;
      EEPROM.write(posledniByte, 5);                                          // Číslo dne v týdnu - 5=Pátek
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(0, 2).toInt()); // Čas Od - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(3, 5).toInt()); // Čas Od - minuty
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(0, 2).toInt()); // Čas Do - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(3, 5).toInt()); // Čas Do - minuty
      posledniByte++;
      teplota = server.arg("teplota").toFloat() * 5;
      teplotaInt = (int)teplota;
      EEPROM.write(posledniByte, teplotaInt);                                 // Teplota - příchozí hodnota se vynásobí 5x aby ukládaná hodnota byla mezi 0 a 255.
      posledniByte++;                                                         // To umožňuje uložit teplotu 0 - 51°C po kroku 0.2°C
    }
    if (server.arg("Sobota") == "1") {
      EEPROM.write(posledniByte, posledniCisloPlanu);                         // Číslo plánu
      posledniByte++;
      EEPROM.write(posledniByte, 6);                                          // Číslo dne v týdnu - 6=Sobota
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(0, 2).toInt()); // Čas Od - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(3, 5).toInt()); // Čas Od - minuty
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(0, 2).toInt()); // Čas Do - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(3, 5).toInt()); // Čas Do - minuty
      posledniByte++;
      teplota = server.arg("teplota").toFloat() * 5;
      teplotaInt = (int)teplota;
      EEPROM.write(posledniByte, teplotaInt);                                 // Teplota - příchozí hodnota se vynásobí 5x aby ukládaná hodnota byla mezi 0 a 255.
      posledniByte++;                                                         // To umožňuje uložit teplotu 0 - 51°C po kroku 0.2°C
    }
    if (server.arg("Nedele") == "1") {
      EEPROM.write(posledniByte, posledniCisloPlanu);                         // Číslo plánu
      posledniByte++;
      EEPROM.write(posledniByte, 0);                                          // Číslo dne v týdnu - 0=Neděle
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(0, 2).toInt()); // Čas Od - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casOd").substring(3, 5).toInt()); // Čas Od - minuty
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(0, 2).toInt()); // Čas Do - hodiny
      posledniByte++;
      EEPROM.write(posledniByte, server.arg("casDo").substring(3, 5).toInt()); // Čas Do - minuty
      posledniByte++;
      teplota = server.arg("teplota").toFloat() * 5;
      teplotaInt = (int)teplota;
      EEPROM.write(posledniByte, teplotaInt);                                 // Teplota - příchozí hodnota se vynásobí 5x aby ukládaná hodnota byla mezi 0 a 255.
      posledniByte++;                                                         // To umožňuje uložit teplotu 0 - 51°C po kroku 0.2°C
    }
  }
  EEPROM.commit();
  homePage();
}

// Funkce pro zjištení počtu plánů a posledního obsazeného byte - pro zjištění pozice pro zapsání nového plánu do nejbližší neobsazené pozice v EEPROM.
byte zjistitPosledniObsazenyByte() {
  int i = 7;
  posledniCisloPlanu = 0;                 // Nutno vynulovat pro případ, kdy je v paměti hodnota > 0 a EEPROM je vymazaná.
  while (EEPROM.read(i) > 0) {            // Projíždět plány dokud nedorazím na poslední
    posledniCisloPlanu = EEPROM.read(i);  // Zapsat poslední nalezený plán
    i = i + 7;
  }
  posledniCisloPlanu++;                   // další nový plán má hodnotu poslední nalezený plán + 1
  return i;
}

// Vymazat Celou paměť EEPROM a všechna (časovač a plány) nastavení v EEPROM kromě stavu (zapnuto/Vypnuto)
void vymazatNastaveni() {
  //  0 byte - stav činnosti časovače a plánů - 0=Vypnuto, 1=Zapnuto
  //  1 byte - Definice Časovače - Rok
  //  2 byte - Definice Časovače - Měsíc
  //  3 byte - Definice Časovače - Den
  //  4 byte - Definice Časovače - Hodina
  //  5 byte - Definice Časovače - Minuta
  //  6 byte - Definice Časovače - Sekunda
  //  7 byte -> každých 7 bytů použito na jeden plán (číslo plánu,číslo dne v týdnu, hodiny od, minuty od, hodiny do, minuty do,teplota)


  for (int i = 1; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("EEPROM vymazána (0-512 byte)");
  // Den a mesic nesmi mit 0. Proto jej pri vymazu nastavim na 1.
  EEPROM.write(2, 1);
  EEPROM.write(3, 1);

  homePage();
}

// Nastavit čas z internetu
void nastavCas() {
  if (timeClient.update() == true) {             // Získání aktuální času z NTP a výsledek jestli se to podařilo. Výsledek v prvním kroku nebude počítat s posunem mezi letním a zimním časem.
    rtc.adjust(timeClient.getEpochTime());       // aktualizace času z NTP do RTC
  }
  else {
    Serial.println("Získání aktuálního času z NTP se nepodařilo");
    Serial.println("RTC nebylo aktualizováno");
  }

  DateTime ted = rtc.now();

  if (ted > najdiNedeli(ted.year(), 3) && ted < najdiNedeli(ted.year(), 10)) {      // V druhém kroku, kdy už ví jaké je dnes datum rozhodne jestli jsme v letním čase (UTC+2) nebo zimním čase (UTC+1)
    timeClient.setTimeOffset(7200);                                                 // Nastaví posun pro letní čas = UTC + 2
    Serial.println("Letní čas");
  } else {
    timeClient.setTimeOffset(3600);                                                 // Nastaví posun pro zimní čas = UTC + 1
    Serial.println("Zimní čas");
  }

  // Druhé načtení aktuálního času z NTP, tentokrát se správným offsetem podle letního a zimního času
  if (timeClient.update() == true) {                        // Získání aktuální času z NTP a výsledek jestli se to podařilo
    rtc.adjust(timeClient.getEpochTime());                  // aktualizace času z NTP do RTC
    ted = rtc.now();
  }
  else {
    Serial.println("Získání aktuálního času z NTP se nepodařilo");
    Serial.println("RTC nebylo aktualizováno");
  }

  Serial.print("Aktuální čas: ");
  Serial.print(ted.year(), DEC);
  Serial.print("-");
  Serial.print(ted.month(), DEC);
  Serial.print("-");
  Serial.print(ted.day(), DEC);
  Serial.print("   ");
  Serial.print(ted.hour(), DEC);
  Serial.print(":");
  Serial.print(ted.minute(), DEC);
  Serial.print(":");
  Serial.println(ted.second(), DEC);

  homePage();
}

// Funkce vrací hodnotu DateTime odpovídající poslední neděli v měsíci v 1:00 AM. Potřebuji pouze pro březen a říjen, proto funkce bude fungovat jen pro měsíce, které mají 31 dní.
DateTime najdiNedeli (int rok, byte mesic) {
  DateTime posledniNedele = DateTime(rok, mesic, 31, 1, 0, 0);

  posledniNedele = posledniNedele - TimeSpan(posledniNedele.dayOfTheWeek(), 0, 0, 0);
  return posledniNedele;
}

// Zjistí teplotu z teploměru
float teplota() {
  float teplota = dht.readTemperature();
  milis = millis();
  while (isnan(teplota)) {                          // Občas vrací "nan" místo hodnoty. Dokud nebude hodnota jiná než "nan" bude se ji snažit načíst ze sensoru znovu
    if (millis() - milis > 100) break;
    teplota = dht.readTemperature();
  }

  return teplota;
}

// Zjištění teploty termistoru - Venkovní
float teplotaTermistor () {
  int termNom = 12000;  // Referenční odpor termistoru
  int refTep = 25;      // Teplota pro referenční odpor
  int beta = 3977;      // Beta faktor
  int rezistor = 10000; // hodnota odporu v sérii - napěťový dělič
  float napeti;

  // Hodnota napětí na termistoru
  napeti = analogRead(A0);
  napeti = 1023 / napeti - 1;
  napeti = rezistor / napeti;

  //Výpočet teploty podle vztahu pro beta faktor
  float teplota;
  teplota = napeti / termNom;         // (R/Ro)
  teplota = log(teplota);             // ln(R/Ro)
  teplota /= beta;                    // 1/B * ln(R/Ro)
  teplota += 1.0 / (refTep + 273.15); // + (1/To)
  teplota = 1.0 / teplota;            // Převrácená hodnota
  teplota -= 273.15;                  // Převod z Kelvinů na stupně Celsia

  return teplota;
}

// Zjistí vlhkost
float vlhkost() {
  float vlhkost = dht.readHumidity();
  milis = millis();
  while (isnan(vlhkost)) {                          // Občas vrací "nan" místo hodnoty. Dokud nebude hodnota jiná než "nan" bude se ji snažit načíst ze sensoru znovu
    if (millis() - milis > 100) break;
    vlhkost = dht.readHumidity();
  }

  return vlhkost;
}

// nastavení akce pro připojení na hlavní stránku.
void homePage() {
  server.send(200, "text/html", HTMLStranka ());
}

// Funkce pro zobrazení stránky s údaji z Arduina
void zobrazData() {
  server.send(200, "text/html", HTMLData());
}

//Nastaví akce pro případ, kdy stránka neexistuje
void strankaNeExistuje () {
  String  zprava = "<!DOCTYPE html><html><head><title>Stránka nenalezena</title></head><body>požadovaná stránka nemá na Arduino nastavenu funkci</body></html>";
}

// --- HTML ---
// Sestavení hlavní HTML stránky
String HTMLStranka () {
  byte stavRele = digitalRead(RELE_PIN);

  String Text = "";

  Text += "<!DOCTYPE html><html><head><meta charset=\"ANSI\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  Text += "<style>body {font-family: Arial;}.tab {overflow: hidden; border: 1px solid #ccc; background-color: #f1f1f1;} .tab button { background-color: inherit; float: left; border: none; outline: none; cursor: pointer; padding: 15px 15px; transition: 0.3s; font-size: 17px;}.tab button:hover { background-color: #ddd;} .tab button.active { background-color: #cc0000; color: white} .tabcontent { padding: 6px 12px; border: 1px solid #ccc; border-top: none; background-color: white;} Fieldset { border: 1px solid #cc0000; color: #cc0000; } input { margin: 5px; height: 20px; width: 100px; padding: 0px;} input[type=checkbox] { margin: 5px; height: 15px; width: 15px;} input[type=submit] { height: 24px;} table, th, td { background-color: white; border: 1px solid #cc0000; border-collapse: collapse; font-family: Courier; font-size: 12px; color: black; padding: 5px;} hr { border-top: 1px solid #cc0000;} th { padding: 5px; text-align: left; background-color: #cc0000; color: white;} .float { float: left;} .right { float: right;} .stavLED { ";
  if (EEPROM.read(0) == 1) {
    Text += "background-color: green;";
  }
  else {
    Text += "background-color: red;";
  }
  Text += "height: 15px; width: 15px; margin-left:10px; display: inline-block;} .stavRele {";
  if (stavRele == LOW) {
    Text += "background-color: green;";
  }
  else {
    Text += "background-color: red;";
  }
  Text += "height: 15px; width: 15px; margin-left:10px; display: inline-block;}";
  Text += ".inlinebox { display: inline-block;} </style></head>";
  Text += "<body><div class=\"tab\"><button class=\"tablinks\" onClick=\"window.location.href = 'http://192.168.15.30';\">TV LED</button><button class=\"tablinks\" onClick=\"window.location.href = 'http://192.168.15.31';\">LED Postel</button><button class=\"active\" >Termostat</button><button class=\"tablinks\" onClick=\"window.location.href = 'http://192.168.15.33';\">Kamera</button></div>";
  Text += "<div class=\"tabcontent\"><fieldset ><legend>Obecne</legend>On/Off:<div class=\"stavLED\"></div>&nbsp; &nbsp;Rele:<div class=\"stavRele\"></div><div class=\"right\">";
  Text += datumCas();
  Text += "</div><form action=\"/on\" ><input type=\"submit\" Value='Zapnout' class=\"float\"></form><form action=\"/off\" ><input type=\"submit\" Value='Vypnout' class=\"float\"></form><form action=\"/vymaz\" ><input type=\"submit\" Value=\"Vymazat vse\" class=\"float\"></form><form action=\"/cas\" ><input type=\"submit\" Value=\"Nastavit cas\" class=\"float\"></form><form action=\"/data\" ><input type=\"submit\" Value=\"Zobrazit data\" class=\"float\"></form></fieldset><br>";
  Text += "<fieldset><legend>Casovac:</legend><form action=\"/casovac\" method=\"get\"><input type=\"time\" value=\"00:15\" name=\"cas\"><input type=\"submit\" Value='Nastavit'></form></fieldset><br>";
  Text += "<fieldset><legend>Planovac:</legend><form action=\"/plan\" method=\"get\">Cas od:<input type=\"time\" value=\"00:00\" name=\"casOd\"><br>Cas do:<input type=\"time\" value=\"00:00\" name=\"casDo\"><br>Teplota:<input type=\"number\" value=\"26.6\" step=\"0.2\" min=\"10\" max=\"35\" name=\"teplota\"><br><div class=\"inlinebox\"><input type=\"checkbox\" value=\"1\" name=\"Pondeli\">Pondeli</div><div class=\"inlinebox\"><input type=\"checkbox\" value=\"1\" name=\"Utery\">Utery</div><div class=\"inlinebox\"><input type=\"checkbox\" value=\"1\" name=\"Streda\">Streda</div><div class=\"inlinebox\"><input type=\"checkbox\" value=\"1\" name=\"Ctvrtek\">Ctvrtek</div><div class=\"inlinebox\"><input type=\"checkbox\" value=\"1\" name=\"Patek\">Patek</div><div class=\"inlinebox\"><input type=\"checkbox\" value=\"1\" name=\"Sobota\">Sobota</div><div class=\"inlinebox\"><input type=\"checkbox\" value=\"1\" name=\"Nedele\">Nedele</div><br><br><input type=\"submit\" Value='Nastavit'></form><br><hr>";
  Text += nacistPlanyHTML();
  Text += "<br></fieldset><br></div></body></html>" ;
  return Text;
}

// Sestavuje část HTML - Plány uložené v EEPROM
String nacistPlanyHTML() {
  float teplota;
  String text;
  int i = 1;                                                                                              // nutno declarovat proměnné zde kvůli while cyklu
  int j = 0;                                                                                              // nutno declarovat proměnné zde kvůli while cyklu

  text += "<table><tr><th>Plan</th><th>Den</th><th>Cas Od</th><th>Cas Do</th><th>Teplota</th></tr>";
  while (EEPROM.read(7 * i + j) > 0) {
    text += "<tr>";
    for (int j = 0; j < 7; j++) {
      switch (j) {
        case 0:                                                                                           // Číslo plánu
          text += "<td>";
          text += String(EEPROM.read(7 * i + j));
          text += "</td>";
          break;
        case 1:                                                                                           // Den v týdnu
          text += "<td>";
          text += String(denTydne[EEPROM.read(7 * i + j)]);
          text += "</td>";
          break;
        case 2:                                                                                           // Cas Od - Hodiny + minuty HH:MM.
          text += "<td>";
          text += String(EEPROM.read(7 * i + j));
          text += ":";
          text += String(EEPROM.read(7 * i + j + 1));
          text += "</td>";
          break;
        case 4:                                                                                           // Cas Do - hodiny + minuty HH:MM
          text += "<td>";
          text += String(EEPROM.read(7 * i + j));
          text += ":";
          text += String(EEPROM.read(7 * i + j + 1));
          text += "</td>";
          break;
        case 6:                                                                                           // Teplota
          text += "<td>";
          teplota = EEPROM.read(7 * i + j) / 5.0;
          text += String(teplota);
          text += "</td>";
          break;
      }
    }
    text += "</tr>";
    i++;
  }
  text += "</table>";
  return text;
}

// Sestavuje část HTML - zbývající čas z časovače uložené v EEPROM
String zbyva() {
  String Text;
  DateTime ted = rtc.now();
  DateTime casDo = DateTime(EEPROM.read(1) + 2000, EEPROM.read(2), EEPROM.read(3), EEPROM.read(4), EEPROM.read(5), EEPROM.read(6));
  TimeSpan zbyva = casDo - ted;

  Text += zbyva.hours();
  Text += ":";
  Text += zbyva.minutes();
  Text += ":";
  Text += zbyva.seconds();

  return Text;
}

// Sestavení HTML stránky pro data z Arduina
String HTMLData () {
  DateTime casTo (EEPROM.read(1) + 2000, EEPROM.read(2), EEPROM.read(3), EEPROM.read(4), EEPROM.read(5), EEPROM.read(6));
  DateTime casTed = rtc.now();
  byte stavRele = digitalRead(RELE_PIN);
  String Text = "";

  Text += "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  Text += "<style>body {font-family: Arial;} .tab { overflow: hidden; border: 1px solid #ccc; background-color: #f1f1f1;} .tab button { background-color: inherit; float: left; border: none; outline: none; cursor: pointer; padding: 15px 15px; transition: 0.3s; font-size: 17px;} .tab button:hover { background-color: #ddd;} .tab button.active { background-color: #cc0000; color: white} .tabcontent { padding: 6px 12px; border: 1px solid #ccc; border-top: none; background-color: white;} Fieldset { border: 1px solid #cc0000; color: #cc0000; } input { margin: 5px; height: 20px; width: 100px; padding: 0px;} input[type=checkbox] { margin: 5px; height: 15px; width: 15px;} input[type=submit] { height: 24px;} table, th, td { background-color: white; border: 1px solid #cc0000; border-collapse: collapse; font-family: Courier; font-size: 12px; color: black; padding: 5px;} hr { border-top: 1px solid #cc0000;} th { padding: 5px; text-align: left; background-color: #cc0000; color: white;} div { margin: 5px;} .float { float: left;} .right { float: right;}";
  Text += ".stavLED { ";
  if (EEPROM.read(0) == 1) {
    Text += "background-color: green;";
  }
  else {
    Text += "background-color: red;";
  }
  Text += " height: 15px; width: 15px; margin-left:10px; display: inline-block; float: left;} .stavRele { ";
  if (stavRele == LOW) {
    Text += "background-color: green;";
  }
  else {
    Text += "background-color: red;";
  }
  Text += " height: 15px; width: 15px; margin-left:10px; display: inline-block; }</style></head>";
  Text += "<body><div class=\"tab\"><button class=\"tablinks\" onClick=\"window.location.href = 'http://192.168.15.30';\">TV LED</button><button class=\"tablinks\" onClick=\"window.location.href = 'http://192.168.15.31';\">LED Postel</button><button class=\"active\" onClick=\"window.location.href = 'http://192.168.15.32';\">Termostat</button><button class=\"tablinks\" onClick=\"window.location.href = 'http://192.168.15.33';\">Kamera</button></div>";
  Text += "<div class=\"tabcontent\"><fieldset ><legend>Obecne</legend><div class=\"float\">Stav:</div><div class=\"stavLED\"></div><div class=\"float\">Stav Sepnuti Rele:</div><div class=\"stavRele\"></div><div>Teplota mistnosti: ";
  Text += teplota();

  Text += "</div><div>Teplota telesa: ";
  Text += teplotaTermistor();

  Text += "</div><div>Vlhkost [%RH]: ";
  Text += vlhkost();
  Text += "</div></fieldset><br><fieldset><legend>Casovac:</legend><div>Aktualni cas: ";
  Text += datumCas();
  if (casTo < casTed) {
    Text += "</div><div>Casovac neni spusten";
  }
  else {
    Text += "</div><div>Cas Do: ";
    Text += casDo();
    Text += "</div><div>Zbyva: ";
    Text += zbyva();
  }
  Text += "</div></fieldset><br></div></body></html>";

  return Text;
}

// Sestavení HTML - aktuální čas
String datumCas() {
  DateTime ted = rtc.now();
  String Text ;

  Text += ted.day(), DEC;
  Text += ".";
  Text += ted.month(), DEC;
  Text += ".";
  Text += ted.year(), DEC;
  Text += "&nbsp; &nbsp; &nbsp; &nbsp;";
  Text += ted.hour(), DEC;
  Text += ":";
  Text += ted.minute(), DEC;
  Text += ":";
  Text += ted.second(), DEC;
  return Text;
}

// Sesatvení HTML - Čas časovače Do
String casDo() {
  DateTime casDo = DateTime(EEPROM.read(1) + 2000, EEPROM.read(2), EEPROM.read(3), EEPROM.read(4), EEPROM.read(5), EEPROM.read(6));
  String Text;

  Text += casDo.day(), DEC;
  Text += ".";
  Text += casDo.month(), DEC;
  Text += ".";
  Text += casDo.year(), DEC;
  Text += "&nbsp; &nbsp; &nbsp; &nbsp;";
  Text += casDo.hour(), DEC;
  Text += ":";
  Text += casDo.minute(), DEC;
  Text += ":";
  Text += casDo.second(), DEC;
  return Text;
}
