
#include <Arduino.h>
#include <SPI.h> // workaround :(

//#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#include "FastLED.h"
#include "Artnet.h"

//const char ssid[] = "led_receiver";
//const char password[] = "led_receiver123";
//const char ssid[] = "PlanetExpress";
//const char password[] = "dc30f313ea88fa7cf231684b43ff71a2";
const char ssid[] = "ISP Dipl 2.4";
const char password[] = "MartinLeucker";

#define BAUD_RATE 115200
#define DATA_PIN 4
#define NUM_LEDS 336
#define MAX_CURRENT 7000
#define PORT 6454
#define MAX_CHANNELS 504
#define BUFF_SIZE 1024

CRGB leds[NUM_LEDS];
Artnet artnet;
uint8_t old_sequence = 0;

#define MIN_ANALOG_READ 0
#define MAX_ANALOG_READ 987
#define NOISE_THRESHOLD 2
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


void setup() {
  Serial.begin(BAUD_RATE);
  delay(10);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.print("\nConnected to WiFi ");
  Serial.println(ssid);
  Serial.print("Local IP is ");
  Serial.println(WiFi.localIP());

  /*
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid,key);

  Serial.println(ssid);
  Serial.println(key);

  Serial.print("\nMy IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println();
  */

  a = ((MAX_GAMMA - 1.0) * (MAX_ANALOG_READ + MIN_ANALOG_READ)) / (MAX_ANALOG_READ - MIN_ANALOG_READ);
  b = (2.0 * (MAX_GAMMA - 1.0)) / (MAX_ANALOG_READ - MIN_ANALOG_READ);
  updateLut();

  artnet.begin();
  // this will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_CURRENT);

  Serial.println("Ready.");
}

void loop() {
  artnet.read();
}


void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
  // 7 lines * 24 LEDs = 168 LEDs , 168 LEDs * 3 Channels = 504 Channels <= 512
  if (universe * MAX_CHANNELS/3 <= NUM_LEDS && old_sequence != sequence && length == MAX_CHANNELS) {
    /*
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
    old_sequence = sequence;
    //Serial.printf("\n");
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
    Serial.print("new gamma value is ");
    Serial.println(led_gamma);
    for (int i = 0; i < 256; i++) {
      lut[i] =  (uint8_t)(ceil(pow(i/255.0, led_gamma) * 255));
    }
  }
}
