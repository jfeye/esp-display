#
#include <Arduino.h>

#include "FastLED.h"


#define DATA_PIN 13
#define NUM_LEDS 336
#define MAX_CURRENT 7000

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5,MAX_CURRENT);
  FastLED.setBrightness(64);

  for(int i=0; i<NUM_LEDS; i++) leds[i] = CHSV(i*5%255,0,255);

  FastLED.show();
}

void loop() {
  delay(1);
  /*for(int i=0; i<NUM_LEDS/2; i++){
    leds[i] = CHSV((2*i*96/NUM_LEDS),255,255-((millis()/5)+(i*512/NUM_LEDS))%255);
    leds[NUM_LEDS-i-1] = CHSV((2*i*96/NUM_LEDS),255,255-((millis()/5)+(i*512/NUM_LEDS))%255);
  }*/
  for(int i=0; i<NUM_LEDS; i++) leds[i] = CHSV((millis()/10-i)%255,255,255);
  FastLED.show();
}
