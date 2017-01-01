// Low power NeoPixel goggles example.
// Makes a nice blinky display with just a few LEDs on at any time.

#include <Adafruit_NeoPixel.h>


#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
#include <avr/power.h>
#endif

#define PIN 0

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(32, PIN);

uint8_t  mode   = 0, // Current animation effect
         offset = 0; // Position of spinny eyes

uint32_t prevTime;

uint32_t color_array[] = {pixels.Color(255, 0, 0), pixels.Color(255, 153, 0), pixels.Color(0, 255, 0),
                          pixels.Color(0, 255, 153), pixels.Color(0, 0, 255)
                         };

uint8_t color_index = 0;

void setup() {
#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif

  pixels.begin();
  pixels.setBrightness(85); // 1/3 brightness
  prevTime = millis();
}

void loop() {
  uint8_t  i;
  uint32_t t;

  uint32_t color = color_array[color_index];
  switch (mode) {

    case 0: // Random sparks - just one LED on at a time!
      i = random(32);
      pixels.setPixelColor(i, color);
      pixels.show();
      delay(10);
      pixels.setPixelColor(i, 0);
      break;

    case 1: // Spinny wheels (8 LEDs on at a time)
      for (i = 0; i < 16; i++) {
        uint32_t c = 0;
        if (((offset + i) & 7) < 2) c = color; // 4 pixels on...
        pixels.setPixelColor(   i, c); // First eye
        pixels.setPixelColor(31 - i, c); // Second eye (flipped)
      }
      pixels.show();
      offset++;
      delay(50);
      break;

/*
    case 2: // Random sparks of sequential color - just one LED on at a time!
      i = random(32);
      pixels.setPixelColor(i, color);
      pixels.show();
      delay(10);
      pixels.setPixelColor(i, 0);
      color_index++;
      if (color_index > sizeof(color_array)) color_index = 0;
      break;

    case 3:
      uint16_t j, k ;
      for (j = 0; j < 256; j++) {
        for (k = 0; k < 32; k++) {
          pixels.setPixelColor(k, Wheel((j + k) & 255));
        }
        pixels.show();
      }
      offset++;
      delay(50);
      break;
  }
*/
  t = millis();
  if ((t - prevTime) > 2000) {     // Every 2 seconds...
    mode++;                        // Next mode
    if (mode > 1) {                // End of modes?
      mode = 0;                    // Start modes over
      color_index++;
      if (color_index > sizeof(color_array)) color_index = 1;
    }
    for (i = 0; i < 32; i++) pixels.setPixelColor(i, 0);
    prevTime = t;
  }
}
/*
uint32_t Wheel(byte WheelPos) {
  if (WheelPos < 85) {
    return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
    WheelPos -= 170;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
*/
}
