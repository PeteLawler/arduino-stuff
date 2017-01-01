/*
    Low power NeoPixel goggles example.
    Makes a nice blinky display
    with just a few LEDs on at any time.

    Base project
    https://learn.adafruit.com/kaleidoscope-eyes-neopixel-led-goggles-trinket-gemma?view=all
    Code also liberated from:
    https://learn.adafruit.com/trinket-sound-reactive-led-color-organ?view=all
    and other locations (forum posts, etc.)

    Hardware requirements:
    Adafruit Trinket or Gemma mini microcontroller (ATTiny85).
    Adafruit Electret Microphone Amplifier (ID: 1063)
    Several Neopixels, you can mix and match
    . Adafruit Flora RGB Smart Pixels (ID: 1260)
    . Adafruit NeoPixel Digital LED strip (ID: 1138)
    . Adafruit Neopixel Ring (ID: 1463)

    Software requirements:
    Adafruit NeoPixel library

    Connections:
    - 3v3 V to mic amp +
    - GND to mic amp -
    - Analog pin to microphone output (configurable below)
    - Digital pin to LED data input (configurable below)

    Written by Adafruit Industries and Peter Lawler <relwalretep@gmail.com>
    Distributed under the BSD license.
    This paragraph must be included in any redistribution.

*/

#include <Adafruit_NeoPixel.h>
#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
// To activate 16MHz, uncomment the following
// #include <avr/power.h>
#endif

#define MINMODE 2 // yuk (also, remember we count from 0)
#define MAXMODE 2 // yuk (also, remember we count from 0)

#define N_PIXELS  32  // Number of pixels you are using
#define N_PANELS   2 // Number of panels we're using
// for now, N_PIXELS / N_PANELS = PIXELS PER PANEL
#define BRIGHT    85  // Default brightness level 0-255
#define MIC_PIN    1  // Microphone is attached to Trinket GPIO #2/Gemma D2 (A1)
#define LED_PIN    0  // NeoPixel LED strand is connected to GPIO #0 / D0
#define DC_OFFSET  0  // DC offset in mic signal - if unusure, leave 0
#define NOISE     100  // Noise/hum/interference in mic signal
#define SAMPLES   60  // Length of buffer for dynamic level adjustment
#define TOP       (N_PIXELS +1) // Allow dot to go slightly off scale

// Comment out the next line if you do not want brightness control or have a Gemma
//#define POT_PIN    3  // if defined, a potentiometer is on GPIO #3 (A3, Trinket only)

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400mode  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

uint32_t color_array[] = {
  // red
  pixels.Color(255, 0, 0),
  // orange
  pixels.Color(255, 127, 0),
  // yellow
  pixels.Color(255, 255, 0),
  // green
  pixels.Color(0, 255, 0),
  // blue
  pixels.Color(0, 0, 255),
  // indigo
  pixels.Color(75, 0, 130),
  // violet
  pixels.Color(148, 0, 211)
};

byte
peak      = 0,      // Used for falling dot
dotCount  = 0,      // Frame counter for delaying dot-falling speed
volCount  = 0;      // Frame counter for storing past volume data

int
vol[SAMPLES],       // Collection of prior volume samples
    lvl       = 10,     // Current "dampened" audio level
    minLvlAvg = 0,      // For dynamic adjustment of graph low & high
    maxLvlAvg = 512;

uint8_t  mode   = MINMODE, // Current animation effect
         offset = 0, // Position of spinny eyes
         color_count = 7, // really actually 6 colours because color_array[0] is a thing
         color_index = 0,
         bright = BRIGHT;

uint32_t prevTime;

void setup() {
#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
  // to activate 16MHz, uncomment the following
  //  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif

  pixels.begin();
  // if POT_PIN is defined, we have a potentiometer on GPIO #3 on a Trinket
  //    (Gemma doesn't have this pin)

#ifdef POT_PIN
  bright = analogRead(POT_PIN);  // Read pin (0-255) (adjust potentiometer
  //   to give 0 to Vcc volts
#endif
  pixels.setBrightness(bright); // 1/3 brightness
  prevTime = millis();
}

void loop() {
  uint8_t  i;
  uint32_t t;

  uint32_t color = color_array[color_index];
  switch (mode) {

    case 0: // Random sparks - just one LED on at a time!
      i = random(N_PIXELS);
      pixels.setPixelColor(i, color);
      pixels.show();
      delay(10);
      pixels.setPixelColor(i, 0);
      break;

    case 1: // Spinny wheels (8 LEDs on at a time)
      for (i = 0; i < (N_PIXELS / 2); i++) {
        uint32_t c = 0;
        if (((offset + i) & 7) < 2) c = color; // 4 pixels on...
        pixels.setPixelColor(i, c); // First eye
        pixels.setPixelColor(31 - i, c); // Second eye (flipped)
      }
      pixels.show();
      offset++;
      delay(25);
      break;

    case 2: // Random sparks of sequential color - just one LED on at a time!
      i = random(N_PIXELS);
      pixels.setPixelColor(i, color);
      pixels.show();
      delay(10);
      pixels.setPixelColor(i, 0);
      color_index++;
      if (color_index > (color_count - 1)) color_index = 0;
      break;

    case 3: // full wheel of colour
      uint16_t j, k ;
      for (j = 0; j < 256; j++) {
        for (k = 0; k < N_PIXELS; k++) {
          pixels.setPixelColor(k, Wheel((j + k) & 255));
        }
        pixels.show();
      }
      delay(25);
      break;

    case 4: // From LED Color Organ
      uint8_t  i;
      uint16_t minLvl, maxLvl;
      int      n, height;
      n   = analogRead(MIC_PIN);                 // Raw reading from mic
      n   = abs(n - 512 - DC_OFFSET);            // Center on zero
      n   = (n <= NOISE) ? 0 : (n - NOISE);      // Remove noise/hum
      lvl = ((lvl * 7) + n) >> 3;    // "Dampened" reading (else looks twitchy)

      // Calculate bar height based on dynamic min/max levels (fixed point):
      height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

      if (height < 0L)       height = 0;     // Clip output
      else if (height > TOP) height = TOP;
      if (height > peak)     peak   = height; // Keep 'peak' dot at top

      // if POT_PIN is defined, we have a potentiometer on GPIO #3 on a Trinket
      //    (Gemma doesn't have this pin)
#ifdef POT_PIN
      bright = analogRead(POT_PIN);  // Read pin (0-255)
#endif
      pixels.setBrightness(bright);    // Set LED brightness
      // Color pixels based on rainbow gradient
      for (i = 0; i < (N_PIXELS / N_PANELS); i++) {
        if (i >= height)
        {
          pixels.setPixelColor(i,   0,   0, 0);
          pixels.setPixelColor(N_PIXELS / N_PANELS + i,   0,   0, 0);
        } else {
          pixels.setPixelColor(i, Wheel(map(i, 0, pixels.numPixels() - 1, 30, 150)));
          pixels.setPixelColor(N_PIXELS / N_PANELS + i, Wheel(map(i, 0, pixels.numPixels() - 1, 30, 150)));
        }
      }

      pixels.show(); // Update strip

      vol[volCount] = n;                      // Save sample for dynamic leveling
      if (++volCount >= SAMPLES) volCount = 0; // Advance/rollover sample counter

      // Get volume range of prior frames
      minLvl = maxLvl = vol[0];
      for (i = 1; i < SAMPLES; i++) {
        if (vol[i] < minLvl)      minLvl = vol[i];
        else if (vol[i] > maxLvl) maxLvl = vol[i];
      }
      // minLvl and maxLvl indicate the volume range over prior frames, used
      // for vertically scaling the output graph (so it looks interesting
      // regardless of volume level).  If they're too close together though
      // (e.g. at very low volume levels) the graph becomes super coarse
      // and 'jumpy'...so keep some minimum distance between them (this
      // also lets the graph go to zero when no sound is playing):
      if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
      minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6; // Dampen min/max levels
      maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6; // (fake rolling average)
      break;
  }


  // MAIN LOOP
  t = millis();
  /*  mode 0,1,2 delay = 2000 miliseconds
      mode 3 delay     = 10 * N_PIXELS milliseconds
      mode 4 delay     = 1750 milliseconds
      mode X delay     = 50 milliseconds
  */
  uint32_t looptime;
  if (mode < 3) {
    looptime = 2000;
  } else if (mode == 3) {
    looptime = 10 * N_PIXELS;
  } else if (mode == 4)  {
    looptime = 10000;
  } else  {
    looptime = 50;
  }
  if ((t - prevTime) > looptime) {     // Every 2 seconds...
    mode++;                        // Next mode
    if (mode > MAXMODE) {                // End of modes?
      mode = MINMODE;                    // Start modes over
      color_index++;
      // my kingdom for a sizeof
      // ... because counting from 0
      if (color_index > (color_count - 1)) color_index = 0;
    }
    for (i = 0; i < N_PIXELS; i++) pixels.setPixelColor(i, 0);
    prevTime = t;
  }
}

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
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
}

