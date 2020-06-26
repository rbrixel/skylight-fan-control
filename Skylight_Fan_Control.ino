/*
 * Skylight Fan Control
 * René Brixel <mail@campingtech.de>
 * 26.06.2020 - v0.2
 * 09.02.2020 - v0.1
 * 
 * Used Libs:
 * ----------
 * DHT sensor library v1.3.10 by Adafruit
 * Adafruit SSD1306 v2.3.0 by Adafruit
 * Adafruit GFX Library v1.9.0 by Adafruit
 */

// ==[ CONFIG: DHT22-Sensor (temperature and humidity; indoor) ]==
#include "DHT.h"
#define DHTPIN 2 // "D2"
#define DHTTYPE DHT22 // DHT11 or DHT22
DHT dht(DHTPIN, DHTTYPE);

// ==[ CONFIG: EEPROM ]==
#include <EEPROM.h>
#define EFANTEMP  0 // Set temperature when fan should start
#define EFANTIME  1 // duration in minutes how long the fan should spin
#define EFANSMAX  2 // max rpm in percent
#define EFANTMAX  3 // duration how long it took the fan to max rpm

// ==[ CONFIG: OLED-Display over i2c ]==
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH  128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET    4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define OLED_I2C      0x3C // i2c-address of display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==[ CONFIG: Timer für Non-Blocking-Code ]==
unsigned long nbcPreviousMillis = 0; // Hält den letzten "Veröffentlichungs"-Zeitstempel
const long nbcInterval = 2000; // Interval in Millisekunden (1.000 Millisekunden = 1 Sekunde)

// ==[ CONFIG: EIGENE VARIABLEN ]==
#define LEDINT  13 // Interne LED an "D13"
#define TMENU   6 // Taster Menü
#define TPLUS   5 // Taster Plus
#define TMINUS  3 // Taster Minus; Pin 4 geht nicht
#define TASTERENTPRELLUNG 200 // Wert in Millisekunden
int posMenu = 0;      // Zwischenspeicher für Menü-Position
float fltHumi = 0.0;  // Zwischenspeicehr für Luftfeuchte
float fltTemp = 0.0;  // Zwischenspeicher für Temperatur
int eFanTemp = 0;     // Zwischenspeicher für EEPROM-Wert; Temperatur einstellen, wann Lüfter anspringt
int eFanTime = 0;     // Zwischenspeicher für EEPROM-Wert; Zeit, wie lange Lüfter läuft (Minuten)
int eFanSMax = 0;     // Zwischenspeicher für EEPROM-Wert; Maximale Drehzahl (Prozent)
int eFanTMax = 0;     // Zwischenspeicher für EEPROM-Wert; Zeit, wie lange Lüfter braucht, um auf volle Drehzahl zu kommen (Minuten)

// ==[ SETUP-FUNCTION ]==
void setup() {
  // EEPROM-Werte auslesen
  eFanTemp = EEPROM.read(EFANTEMP);
  eFanTime = EEPROM.read(EFANTIME);
  eFanSMax = EEPROM.read(EFANSMAX);
  eFanTMax = EEPROM.read(EFANTMAX);
  
  if (eFanTemp == 255) {
    eFanTemp = 25;
    EEPROM.update(EFANTEMP, eFanTemp);
  }
  if (eFanTime == 255) {
    eFanTime = 30;
    EEPROM.update(EFANTIME, eFanTime);
  }
  if (eFanSMax == 255) {
    eFanSMax = 100;
    EEPROM.update(EFANSMAX, eFanSMax);
  }
  if (eFanTMax == 255) {
    eFanTMax = 5;
    EEPROM.update(EFANTMAX, eFanTMax);
  }

  // DHT22-Sensor starten
  dht.begin();

  // Pin-Richtung setzen
  pinMode(LEDINT, OUTPUT); // Interne LED aktivieren
  pinMode(TMENU, INPUT);
  pinMode(TPLUS, INPUT);
  pinMode(TMINUS, INPUT);

  // OLED starten
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  display.clearDisplay();
}

void loop() {
  // Non-blocking-Code; wird nur ausgeführt, wenn Intervall erreicht wurde
  unsigned long nbcCurrentMillis = millis(); // Aktuellen Timestamp holen
  if (nbcCurrentMillis - nbcPreviousMillis >= nbcInterval) {
    nbcPreviousMillis = nbcCurrentMillis;
    
    // Lese Sensor 1 (DHT22) ein:
    fltHumi = dht.readHumidity();
    fltTemp = dht.readTemperature();
  }

  // ----- Ab hier Echtzeit-Code -----

  // --[ Menü-Positionszähler ]--
  int intTMenu = 0;
  intTMenu = digitalRead(TMENU);
  if (intTMenu == 1) {
    posMenu += 1;
    // 0 = Generelle Anzeige
    // 1 = Temperatur einstellen, wann Lüfter anspringt
    // 2 = Zeit, wie lange Lüfter läuft (Minuten)
    // 3 = Maximale Drehzahl (Prozent)
    // 4 = Zeit, wie lange Lüfter braucht, um auf volle Drehzahl zu kommen
    if (posMenu > 4) { // Überlauf
      posMenu = 0;
    }

    // EEPROM aktualisieren beim Drücken der Menü-Taste
    EEPROM.update(EFANTEMP, eFanTemp);
    EEPROM.update(EFANTIME, eFanTime);
    EEPROM.update(EFANSMAX, eFanSMax);
    EEPROM.update(EFANTMAX, eFanTMax);
    
    delay(TASTERENTPRELLUNG);
  }

  // --[ Menü-Ausgabe ]--
  switch (posMenu) {
    case 0:
      menuMain();
      break;
    case 1:
      menuTemp();
      break;
    case 2:
      menuWerte();
      break;
    default:
      // if nothing else matches, do the default
      // default is optional
      break;
  }

  // --[ Temperatur einstellen ]--
  if (posMenu == 1) {
    int intTPlus = 0;
    intTPlus = digitalRead(TPLUS);
    int intTMinus = 0;
    intTMinus = digitalRead(TMINUS);

    if (intTPlus == 1) {
      eFanTemp += 1;
    }
    if (intTMinus == 1) {
      eFanTemp -= 1;
    }

    if (eFanTemp < 10) {
      eFanTemp = 10;
    }
    if (eFanTemp > 50) {
      eFanTemp = 50;
    }

    delay(TASTERENTPRELLUNG);
  }
}

void menuHeader() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  // Menü-Position ausgeben:
  display.setCursor(120, 0);
  display.print(posMenu);
}

void menuDrawHead(String strTitle) {
  display.setCursor(0, 0);
  display.print(strTitle); // Maximal 21 Zeichen und 3 Zeilen
  display.drawLine(0, 8, display.width()-1, 8, SSD1306_WHITE);
}

void menuMain () {
  menuHeader();
  menuDrawHead("FANCONTROL");
  display.setCursor(0, 13);
  display.print(F("Raumklima:"));
  display.setCursor(0, 25);
  display.print(String(fltTemp, 1) + " *C / " + String(fltHumi, 1) + " %");
  display.display();
}

void menuTemp () {
  menuHeader();
  menuDrawHead("Temp. einstellen");
  display.setCursor(0, 13); // x, y
  display.print("Akt. Wert: " + String(eFanTemp) + " *C");
  display.setCursor(0, 25); // x, y
  display.print("Temp.: 10 bis 50 *C");
  display.display();
}

void menuWerte () {
  menuHeader();
  menuDrawHead("Systemwerte");
  display.setCursor(0, 13);
  display.print("Temp:" + String(eFanTemp) + " / Time: " + String(eFanTime));
  display.setCursor(0, 25);
  display.print("SMax:" + String(eFanSMax) + " / TMax: " + String(eFanTMax));
  display.display();
}
