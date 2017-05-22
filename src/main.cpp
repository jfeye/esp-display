
#include <Arduino.h>
#include <SPI.h> // workaround :(

#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "send_progmem.h"
#include "html_index.h"

//#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#include "FastLED.h"
#include "Artnet.h"

#define BAUD_RATE 9600
#define EEPROM_SIZE 1024

#define DATA_PIN 2
#define NUM_LEDS 336
#define ROWS 14
#define COLS 24
#define MAX_CURRENT 7000

#define PORT 6454
#define MAX_CHANNELS 504
#define BUFF_SIZE 1024

#define MODE_PIN D1
uint8_t mode = 0;

CRGB leds[NUM_LEDS];
Artnet artnet;
uint8_t old_sequence = 0;
uint8_t old_universe = 2;

#define CASE_PIN D0
uint8_t case_closed = 0;

#define MIN_ANALOG_READ 0
#define MAX_ANALOG_READ 987
#define NOISE_THRESHOLD 3
#define MAX_GAMMA 5.0 // should be in [1.0, 8.0]
#define READ_GAMMA_EVERY 20

uint16_t gammaRead = 0;
float led_gamma = 1.0;
uint8_t lut[256];
float a = 0.0;
float b = 0.0;
uint8_t frameCnt = 0; // will be in [0, READ_GAMMA_EVERY-1], so lookup-table will be updated every READ_GAMMA_EVERY frames

ESP8266WebServer server(80);
uint8_t timeout = 100; // timeout for WiFi connection in 100ms, e.g. a value of 10 equals 1s

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data);
void dump(const uint8_t *buf, size_t buflen);
void updateLut();
void showIP(String ip, CRGB color, uint8_t reverse);
int scan_networks();
void serveIndex();
void handleGet();
void handleOther();
void getSSID(int idx, char buffer[]);
int getSSIDLength(int idx);
void getKey(int idx, char buffer[]);
int getKeyLength(int idx);

String scannedWiFis = "";
String storedWiFis = "";
String storedKeys = "";


/*
**************************************************
*  void setup()
**************************************************
*/

void setup() {
  Serial.begin(BAUD_RATE);
  EEPROM.begin(EEPROM_SIZE);

  int wifi_idx = scan_networks();

  pinMode(MODE_PIN, INPUT_PULLUP);
  mode = (digitalRead(MODE_PIN) == LOW ? 1 : 0);
  if (wifi_idx < 0) {
    mode = 1;
    Serial.println("No WiFi access data available.");
  }

  pinMode(CASE_PIN, INPUT_PULLUP);
  case_closed = (digitalRead(CASE_PIN) == LOW ? 1 : 0);

  delay(500);

  IPAddress my_ip;
  CRGB color = CRGB::Green;
  if (mode) {
    // Master mode
    Serial.println("Set to Master-mode");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("led-display", "led-display");

    Serial.println("SSID:     led-display");
    Serial.println("Password: led-display");
    my_ip = WiFi.softAPIP();
  } else {
    // Slave mode
    Serial.println("Set to Slave-mode");

    char ssid[getSSIDLength(wifi_idx)];
    getSSID(wifi_idx, ssid);

    char password[getKeyLength(wifi_idx)];
    getKey(wifi_idx, password);

    WiFi.begin(ssid, password);
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
      Serial.print(".");
      delay(100);
      timeout--;
    }
    if (timeout > 0) {
      Serial.print("\nConnected to WiFi ");
      Serial.println(ssid);
    } else {
      Serial.println("ERROR: Connection timeout!");
      color = CRGB::Red;
    }
    my_ip = WiFi.localIP();
  }

  server.on("/", serveIndex);
  server.on("/get", handleGet);
  server.onNotFound(handleOther);
  server.begin();

  Serial.print("Local IP is ");
  Serial.println(my_ip);

  a = ((MAX_GAMMA - 1.0) * (MAX_ANALOG_READ + MIN_ANALOG_READ)) / (MAX_ANALOG_READ - MIN_ANALOG_READ);
  b = (2.0 * (MAX_GAMMA - 1.0)) / (MAX_ANALOG_READ - MIN_ANALOG_READ);
  updateLut();

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_CURRENT);

  showIP(my_ip.toString(), color, case_closed);
  delay(5000);

  artnet.begin();
  // this will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);

  Serial.println("Ready.");
}


void getSSID(int idx, char buffer[]) {
  uint16_t eeprom_idx = 1;
  uint8_t s = 0;
  for (int i = 0; i < idx; i++) {
    s = EEPROM.read(eeprom_idx++);
    eeprom_idx += s;
    s = EEPROM.read(eeprom_idx++);
    eeprom_idx += s;
  }
  s = EEPROM.read(eeprom_idx++);
  for (int i = 0; i < s; i++) {
    buffer[i] = char(EEPROM.read(eeprom_idx++));
  }
}


int getSSIDLength(int idx) {
  uint16_t eeprom_idx = 1;
  uint8_t s = 0;
  for (int i = 0; i < idx; i++) {
    s = EEPROM.read(eeprom_idx++);
    eeprom_idx += s;
    s = EEPROM.read(eeprom_idx++);
    eeprom_idx += s;
  }
  s = EEPROM.read(eeprom_idx++);
  return (int)s;
}


void getKey(int idx, char buffer[]) {
  uint16_t eeprom_idx = 1;
  uint8_t p = 0;
  for (int i = 0; i < idx; i++) {
    p = EEPROM.read(eeprom_idx++);
    eeprom_idx += p;
    p = EEPROM.read(eeprom_idx++);
    eeprom_idx += p;
  }
  p = EEPROM.read(eeprom_idx++);
  eeprom_idx += p;
  p = EEPROM.read(eeprom_idx++);
  for (int i = 0; i < p; i++) {
    buffer[i] = char(EEPROM.read(eeprom_idx++));
  }
}


int getKeyLength(int idx) {
  uint16_t eeprom_idx = 1;
  uint8_t p = 0;
  for (int i = 0; i < idx; i++) {
    p = EEPROM.read(eeprom_idx++);
    eeprom_idx += p;
    p = EEPROM.read(eeprom_idx++);
    eeprom_idx += p;
  }
  p = EEPROM.read(eeprom_idx++);
  eeprom_idx += p;
  p = EEPROM.read(eeprom_idx++);
  return (int)p;
}


void serveIndex() {
  server.send(200, "text/html", FPSTR(html_index));
  sendProgmem(&server, html_index);
}


void handleGet() {
  Serial.println("GET request: ");
  for (int i = 0; i < server.args(); i++) {
    Serial.print("\t");
    Serial.print(server.argName(i));
    Serial.print("=");
    Serial.println(server.arg(i));
    if (server.argName(i) == "v") {
       if (server.arg(i) == "s") {
         server.send(200, "text/plain", scannedWiFis);
       } else if (server.arg(i) == "e") {
         server.send(200, "text/plain", storedWiFis);
       } else {
         server.send(404, "text/plain", "Invalid value: " + server.arg(i));
       }
    } else if (server.argName(i) == "s") {
      Serial.println(server.arg(i).length());
      server.send(200, "text/plain", server.arg(i));
    } else if (server.argName(i) == "p") {
      Serial.println(server.arg(i).length());
      server.send(200, "text/plain", server.arg(i));
    } else {
      server.send(404, "text/plain", "Invalid argument: " + server.argName(i));
    }
  }
}


void handleOther() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send ( 404, "text/plain", message );
}


int scan_networks() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No WiFi networks found.");
    scannedWiFis = "[]";
  } else {
    Serial.printf("%d WiFi networks found\n", n);
    scannedWiFis = "[";
    for (int i = 0; i < n; i++) {
      Serial.print(i+1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      scannedWiFis += "\"" + WiFi.SSID(i) + "\"";
      scannedWiFis += (i < n-1 ? "," : "");
      Serial.println(WiFi.encryptionType(i) == ENC_TYPE_NONE ? " " : "*");
    }
    scannedWiFis += "]";
  }

  uint16_t eeprom_idx = 1;
  uint8_t e = EEPROM.read(0);
  if (e >= 32) {
    e = 0;
    EEPROM.write(0, 0);
  }
  uint8_t s = 0;
  uint8_t p = 0;
  storedWiFis = "[";
  storedKeys = "[";
  if (e > 0) {
    for (int i = 0; i < e; i++) {
      storedWiFis += "\"";
      s = EEPROM.read(eeprom_idx++);
      for (int j = 0; j < s; j++) {
        storedWiFis += "" + char(EEPROM.read(eeprom_idx++));
      }
      storedWiFis += "\"";
      storedWiFis += (i < e-1 ? "," : "");

      storedKeys += "\"";
      p = EEPROM.read(eeprom_idx++);
      for (int j = 0; j < p; j++) {
        storedKeys += "" + char(EEPROM.read(eeprom_idx++));
      }
      storedKeys += "\"";
      storedKeys += (i < e-1 ? "," : "");
    }
  }
  storedWiFis += "]";
  storedKeys += "]";

  if (n == 0) return -1;
  else if (e == 0) return -2;
  else {
    eeprom_idx = 1;
    for (int i = 0; i < e; i++) {
      s = EEPROM.read(eeprom_idx++);
      for (int j = 0; j < n; j++) {
        for (int k = 0; k < s; k++) {
          if (char(EEPROM.read(eeprom_idx+k)) != WiFi.SSID(j).charAt(k)) {
            k = s + 1;
          }
          if (k == s - 1) {
            if (k == WiFi.SSID(j).length() - 1) {
              return i;
            }
          }
        }
      }
      eeprom_idx += s;
      p = EEPROM.read(eeprom_idx++);
      eeprom_idx += p;
    }
    return -3;
  }
}


void loop() {
  artnet.read();
  server.handleClient();
}


void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
  // 7 lines * 24 LEDs = 168 LEDs , 168 LEDs * 3 Channels = 504 Channels <= 512
  if (universe * MAX_CHANNELS/3 <= NUM_LEDS && old_sequence != sequence && old_universe != universe && length == MAX_CHANNELS) {
    /*
    // debug
    Serial.print("Universe: ");
    Serial.print(universe);
    Serial.print("\tLength: ");
    Serial.print(length);
    Serial.print("\tSequence: ");
    Serial.println(sequence);
    dump(data, length);
    */
    if (frameCnt == READ_GAMMA_EVERY-1) {
      updateLut();
      case_closed = (digitalRead(CASE_PIN) == LOW ? 1 : 0);
      frameCnt = 0;
    } else {
      frameCnt++;
    }

    uint16_t led_idx = 0;
    for (int i = 0; i < length; i = i+3) {
      led_idx = (uint16_t)(((universe-1)*MAX_CHANNELS + i)/3);
      if (case_closed) {
        if (led_idx >= NUM_LEDS/2) {
          led_idx -= ((uint16_t)(i/(COLS*3))) * (2*COLS) + COLS;
        } else { // 2
          led_idx += ( (ROWS/(3*NUM_LEDS/MAX_CHANNELS) - 1) - ((uint16_t)(i/(COLS*3))) ) * (2*COLS) + COLS;
        }
      }
      leds[led_idx] = CRGB(lut[data[i]], lut[data[i+1]], lut[data[i+2]]);
    }
    old_sequence = sequence;
    old_universe = universe;

    FastLED.show();
  } else {
    Serial.println("ooo SEQUENCE ORDER ERROR ooo");
    Serial.print("Universe: ");
    Serial.print(universe);
    Serial.print("\tLength: ");
    Serial.print(length);
    Serial.print("\tSequence: ");
    Serial.println(sequence);
  }
}


void dump(const uint8_t *buf, size_t buflen){
    Serial.printf("Data: \n");
    uint16_t t = 0;
    while(t < buflen) {
      Serial.printf("%02X %s", *buf++, t%3 == 2 ? "   " : " ") ;
      t++;
      if (t%18 == 0) Serial.printf("\n");
    }
    Serial.printf("\n");
}


void updateLut() {
  uint16_t old_gammaRead = gammaRead;
  gammaRead = analogRead(A0);
  if ((gammaRead > old_gammaRead + NOISE_THRESHOLD) || (gammaRead < old_gammaRead - NOISE_THRESHOLD)) {
    Serial.print("Updating LUT... ");
    if (gammaRead >= (MAX_ANALOG_READ + MIN_ANALOG_READ)/2.0) {
      led_gamma = gammaRead * b - a + 1.0;
    } else {
      led_gamma = 1 / (1.0 + a - b * gammaRead);
    }
    for (int i = 0; i < 256; i++) {
      lut[i] =  (uint8_t)(ceil(pow(i/255.0, led_gamma) * 255));
    }
    Serial.print("new gamma value is ");
    Serial.println(led_gamma);
  }
}


void showIP(String ip, CRGB color, uint8_t reverse) {
  uint8_t ip_extraSpaces[4] = {0, 0, 0, 0};

  for (uint16_t i=0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }

  char letters[10][15] = {
      // '0'
      {1, 1, 1,
       1, 0, 1,
       1, 0, 1,
       1, 0, 1,
       1, 1, 1},
      // '1'
      {0, 1, 0,
       1, 1, 0,
       0, 1, 0,
       0, 1, 0,
       1, 1, 1},
      // '2'
      {1, 1, 1,
       0, 0, 1,
       1, 1, 1,
       1, 0, 0,
       1, 1, 1},
      // '3'
      {1, 1, 1,
       0, 0, 1,
       1, 1, 1,
       0, 0, 1,
       1, 1, 1},
      // '4'
      {1, 0, 0,
       1, 0, 1,
       1, 1, 1,
       0, 0, 1,
       0, 0, 1},
      // '5'
      {1, 1, 1,
       1, 0, 0,
       1, 1, 1,
       0, 0, 1,
       1, 1, 1},
      // '6'
      {1, 1, 1,
       1, 0, 0,
       1, 1, 1,
       1, 0, 1,
       1, 1, 1},
      // '7'
      {1, 1, 1,
       0, 0, 1,
       0, 1, 0,
       0, 1, 0,
       0, 1, 0},
      // '8'
      {1, 1, 1,
       1, 0, 1,
       1, 1, 1,
       1, 0, 1,
       1, 1, 1},
      // '9'
      {1, 1, 1,
       1, 0, 1,
       1, 1, 1,
       0, 0, 1,
       1, 1, 1}
    };

    uint8_t l = ip.length();
    unsigned char chars[16];
    ip.getBytes(chars, 16);

    uint8_t cnt = 0;
    uint8_t dots = 0;
    for (uint8_t i = 0; i < l; i++) {
      if (chars[i] != '.') {
        cnt++;
      } else {
        ip_extraSpaces[dots] = 3 - cnt;
        cnt = 0;
        dots++;
      }
    }
    ip_extraSpaces[dots] = 3 - cnt;
    dots = 0;
    uint8_t sum = 0;
    for (uint8_t i=0; i < l; i++) {
      if (chars[i] == '.') {
        dots++;
      }
      if(chars[i] >= '0' && chars[i] <= '9') {
          sum = 0;
          for (uint8_t k = 0; k <= dots; k++) {
            sum += ip_extraSpaces[k];
          }
          uint16_t x = (i-dots+sum)*4 + dots - (dots > 1 ? 2 : 0);
          uint16_t y = x/COLS * 7;
          for(int j=0; j < 15; j++){
              if(letters[chars[i]-'0'][j]){
                uint16_t tx = x + j%3;
                uint16_t ty = y + j/3;
                uint16_t idx = ty*COLS;
                if ((ty%2 == 1) == reverse) idx += COLS - (tx%COLS) -1;
                else idx += tx%COLS;
                if (reverse) leds[idx] = color;
                else leds[NUM_LEDS-idx-1] = color;
              }
          }
      }
    }

    FastLED.show();
}
