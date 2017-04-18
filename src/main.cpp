
#include <Arduino.h>
#include <SPI.h> // workaround :(

//#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#include "FastLED.h"
#include "Artnet.h"

const char ssid[] = "ISP Dipl 2.4";
const char password[] = "MartinLeucker";

#define BAUD_RATE 9600
#define DATA_PIN 2
#define NUM_LEDS 336
#define ROWS 14
#define COLS 24
#define MAX_CURRENT 7000
#define PORT 6454
#define MAX_CHANNELS 504
#define BUFF_SIZE 1024

#define MODE_PIN D1

CRGB leds[NUM_LEDS];
Artnet artnet;
uint8_t old_sequence = 0;
uint8_t old_universe = 2;
uint8_t mode = 0;

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

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data);
void dump(const uint8_t *buf, size_t buflen);
void updateLut();
void printIP(String ip, CRGB color, uint8_t reverse);


/*
**************************************************
*  void setup()
**************************************************
*/

void setup() {
  Serial.begin(BAUD_RATE);
  delay(500);
  Serial.println("Serial started");

  pinMode(MODE_PIN, INPUT_PULLUP);
  delay(250);
  mode = digitalRead(MODE_PIN);

  IPAddress my_ip;
  if (mode == LOW) {
    // Master mode
    Serial.println("Set to Master-mode");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("LED-Display", "led-display");

    Serial.println("SSID:     LED-Display");
    Serial.println("Password: led-display");
    my_ip = WiFi.softAPIP();
  } else {
    // Slave mode
    Serial.println("Set to Slave-mode");

    WiFi.begin(ssid, password);
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(100);
    }
    Serial.print("\nConnected to WiFi ");
    Serial.println(ssid);
    my_ip = WiFi.localIP();
  }

  Serial.print("Local IP is ");
  Serial.println(my_ip);

  a = ((MAX_GAMMA - 1.0) * (MAX_ANALOG_READ + MIN_ANALOG_READ)) / (MAX_ANALOG_READ - MIN_ANALOG_READ);
  b = (2.0 * (MAX_GAMMA - 1.0)) / (MAX_ANALOG_READ - MIN_ANALOG_READ);
  updateLut();

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_CURRENT);

  printIP(my_ip.toString(), CRGB::Green, 0);
  delay(6000);

  artnet.begin();
  // this will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);

  Serial.println("Ready.");
}


void loop() {
  artnet.read();
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
      frameCnt = 0;
    } else {
      frameCnt++;
    }

    for (int i = 0; i < length; i = i+3) {
      leds[(uint16_t)(((universe-1)*MAX_CHANNELS + i)/3)] = CRGB(lut[data[i]], lut[data[i+1]], lut[data[i+2]]);
      //Serial.printf("%3d: %02X %02X %02X%s", ((universe-1)*MAX_CHANNELS + i)/3, data[i], data[i+1], data[i+2], (((universe-1)*MAX_CHANNELS + i)/3)%6 == 5 ? "\n" : "\t");
    }
    //Serial.printf("\n");
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


void printIP(String ip, CRGB color, uint8_t reverse) {
  uint8_t ip_extraChars[4] = {0, 0, 0, 0};

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
      {0, 0, 1,
       0, 0, 1,
       0, 0, 1,
       0, 0, 1,
       0, 0, 1},
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
      {1, 0, 1,
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
       1, 0, 0,
       1, 0, 0},
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
        ip_extraChars[dots] = 3 - cnt;
        cnt = 0;
        dots++;
      }
    }
    ip_extraChars[dots] = 3 - cnt;
    dots = 0;
    uint8_t sum = 0;
    for (uint8_t i=0; i < l; i++) {
      //Serial.println("c: " + String(chars[i]));
      if (chars[i] == '.') {
        dots++;
      }
      if(chars[i] >= '0' && chars[i] <= '9') {
          sum = 0;
          for (uint8_t k = 0; k <= dots; k++) {
            sum += ip_extraChars[k];
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
