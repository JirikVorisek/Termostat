# Termostat
Termostat postavený na Arduino. Funguje jako spínaná zásuvka pro elektrický topný žebřík. Konfigurační interface je založen na Webserveru ESP8266.

# Elektronika:
1. Wemos D1 mini s ESP8266 WiFi chipem a seriovým převodníkem (https://docs.wemos.cc/en/latest/d1/d1_mini.html)
2. RTC DS1307 - pro práci s časem. Tento modul není nezbytný a lze jej nahradit funkcí Wemos D1 mini a zpřesňovat čas přes NTP server.
3. Teplotní sensor a vlhkoměr DHT22. Tento modul zjišťuje okolní teplotu v místnosti a udržuje teplotu na nastavených limitech.
4. 5V, 1-kanálové relé. Modul spíná zásuvku 230V/10A.
