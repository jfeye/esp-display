
#include <Arduino.h>
#include <SPI.h> // workaround :(

#include "FastLED.h"
#include "Artnet.h"

const char ssid[] = "led_receiver";
const char key[] = "led_receiver123";

#define DATA_PIN 4
#define NUM_LEDS 336
#define MAX_CURRENT 7000
#define PORT 6454
#define BUFF_SIZE 1024

uint8_t udp_buff[BUFF_SIZE];
CRGB leds[NUM_LEDS];
Artnet artnet;
WiFiUDP udp;

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data);
void dump(const uint8_t *buf, size_t buflen);

void setup() {

  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid,key);

  Serial.println(ssid);
  Serial.println(key);

  Serial.print("\nMy IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println();


  //artnet.begin();

  // this will be called for each packet received
  //artnet.setArtDmxCallback(onDmxFrame);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5,MAX_CURRENT);
  FastLED.setBrightness(64);

  for(int i=0; i<NUM_LEDS; i++) leds[i] = CHSV(i*5%255,255,255);
  FastLED.show();

  udp.begin(PORT);
  Serial.println("Ready.");
}

void loop() {
  int sz;
  if( (sz = udp.parsePacket()) > 0){
    udp.read(udp_buff, sizeof(udp_buff));
    dump(udp_buff, sz);
  }
}


void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data){
  Serial.println("u:"+String(universe));
  Serial.println("l:"+String(length));
  Serial.println("s:"+String(sequence));
  dump(data,length);
  if (universe > 7 || universe <1){
    Serial.println("u:" + String(universe));
    return;
  }
  for (int i = 0; i < (length/3) && (((universe-1)*24)+i)<NUM_LEDS; i++){
    leds[((universe-1)*24)+i] = CRGB(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
  }
  FastLED.show();
  Serial.println("Frame.");
}

void dump(const uint8_t *buf, size_t buflen){
    Serial.printf("Data: ");
    uint8_t t = buflen%16;
    while(buflen--){
      if (buflen%16 == t) Serial.printf("\n");
      Serial.printf("%02X%s", *buf++, (buflen > 0) ? " " : "");
    }
    Serial.printf("\n");

}
