#include <Wire.h>
#include <tinyNeoPixel.h>
//#define SEG14
#include "AlphaDisplay.h"

//#define SERIALDEBUG

#if defined (__AVR_ATtiny85__)
#define BUTTON_PIN          3
#define DATA_PIN            4
#endif
#if defined (__AVR_ATtiny814__) || defined (__AVR_ATtiny1614__)
#define BUTTON_PIN          3
#define DATA_PIN            PIN_PA4
#define VIO_PIN             2
#endif
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB
#define NUM_LEDS            24
#define BRIGHTNESS          32
#define FRAMES_PER_SECOND   50
#define FADE_STEPS          32
#define PATTERN_LENGTH      12
#define NUM_TEST            1000
#define DISPLAY_ADDRESS     0x70
#define DISPLAY_DIGITS      5
#define DISPLAY_BRIGHTNESS  8
#define ANIMATION_DELAY     25
#define DIVIDER_RUNNING     2
#define DIVIDER_SLOWDOWN1   4
#define DIVIDER_SLOWDOWN2   8
#define DIVIDER_FLASH68     20
#define DIVIDER_FLASH7      10
#define DIVIDER_RANDOM      20

bool     waitForFirstZero, toggle;
uint8_t  state, result1, result2, fade, pos1, pos2, sum, divider, dividerRandom, fadeCounter;
uint16_t hue;
uint32_t currentTime, animationTime, zeroTime;

struct dicePattern {
  uint8_t startPos;
  uint16_t pattern;
};

enum { DICE_INIT = 1, DICE_WAIT, DICE_RUNNING, DICE_SLOWDOWN1, DICE_SLOWDOWN2, DICE_PRINT, DICE_CONFETTI, DICE_TEST };

const dicePattern pattern[] = { { 5, 0b000001000000 }, { 4, 0b000010100000 }, { 3, 0b000101010000 },
  { 2, 0b001010101000 }, { 1, 0b010101010100 }, { 0, 0b101010101010 }
};

// A 256-byte pre-calculated 8-bit sine wave lookup table (0 to 255)
// This matches FastLED's internal sin8 lookup for high performance.
const uint8_t PROGMEM sin8_lut[] = {
  128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170, 173,
  176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211, 213, 215,
  218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 238, 240, 241, 243, 244,
  245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255,
  255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246,
  245, 244, 243, 241, 240, 238, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220,
  218, 215, 213, 211, 208, 206, 203, 201, 198, 196, 193, 190, 188, 185, 182, 179,
  176, 173, 170, 167, 165, 162, 158, 155, 152, 149, 146, 143, 140, 137, 134, 131,
  128, 124, 121, 118, 115, 112, 109, 106, 103, 100, 97,  93,  90,  88,  85,  82,
  79,  76,  73,  70,  67,  65,  62,  59,  57,  54,  52,  49,  47,  44,  42,  40,
  37,  35,  33,  31,  29,  27,  25,  23,  21,  20,  18,  17,  15,  14,  12,  11,
  10,  9,   7,   6,   5,   5,   4,   3,   2,   2,   1,   1,   1,   0,   0,   0,
  0,   0,   0,   0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,
  10,  11,  12,  14,  15,  17,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,
  37,  40,  42,  44,  47,  49,  52,  54,  57,  59,  62,  65,  67,  70,  73,  76,
  79,  82,  85,  88,  90,  93,  97,  100, 103, 106, 109, 112, 115, 118, 121, 124
};

uint8_t beatsin8(uint16_t, uint8_t, uint8_t, uint32_t, uint8_t);
void fadeToBlack(uint8_t);

AlphaDisplay alpha;

tinyNeoPixel ring(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

const uint32_t colorRed = ring.Color(255, 0, 0);
const uint32_t colorOrange = ring.Color(255, 165, 0);
const uint32_t colorYellow = ring.Color(255, 255, 0);
const uint32_t colorGreen = ring.Color(0, 255, 0);
const uint32_t colorCyan = ring.Color(0, 255, 255);
const uint32_t colorBlue = ring.Color(0, 0, 255);
const uint32_t colorMagenta = ring.Color(255, 0, 255);

//---------------------------------------------------------------------------------

void setup() {
  delay(100);

#ifdef SERIALDEBUG
  // use pins PA1(TX) and PA2(RX)
  Serial.swap(1);
  Serial.begin(115200);
  Serial.println("Init...");
  delay(100);
#endif

#if defined (TINYWIREM)
  TinyWireM.begin();
#elif defined (TINYI2C)
  TinyI2C.init();
#else
  Wire.begin();
#endif

  // set IO-level for alphanumeric display
#if (__AVR_ATtiny814__) || defined(__AVR_ATtiny1614__)
  PORTB.DIRSET = 1 << VIO_PIN;
  PORTB.OUTSET = 1 << VIO_PIN;
#endif

  delay(100);
  alpha.init(DISPLAY_ADDRESS, DISPLAY_DIGITS, DISPLAY_BRIGHTNESS);
  alpha.print("IN IT");

  // button input pin with pullup enabled
#if (__AVR_ATtiny814__) || defined (__AVR_ATtiny1614__)
  PORTA.DIRCLR = 1 << BUTTON_PIN;
  PORTA.PIN3CTRL = PORT_PULLUPEN_bm;
#endif
#if defined (__AVR_ATtiny85__)
  DDRB &= ~(1 << BUTTON_PIN);
  PORTB |= 1 << BUTTON_PIN;
#endif

  // NeoPixel LEDs
  ring.begin();
  ring.setBrightness(BRIGHTNESS);
  ring.clear();
  ring.show();

  hue = 0;
  uint32_t color;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    color = ring.ColorHSV(hue, 255, 255);
    ring.clear();
    ring.setPixelColor(i, color);
    hue += 32768 / NUM_LEDS;
    ring.show();
    delay(ANIMATION_DELAY * 2);
  }
  ring.clear();
  ring.show();

  alpha.clear();

  hue = 0;
  animationTime = millis();
  state = DICE_INIT;
}

//---------------------------------------------------------------------------------

void loop()
{
  currentTime = millis();
  if (currentTime - animationTime > ANIMATION_DELAY) {
    animationTime = currentTime;

    switch (state) {
      case DICE_INIT:
        randomSeed(123456);
        result1 = 0;
        result2 = 0;
        dividerRandom = 0;
        state = DICE_CONFETTI;
#ifdef SERIALDEBUG
        Serial.println("State: Confetti");
#endif
        break;

      case DICE_WAIT:
        if (readButton(BUTTON_PIN)) {
#ifdef SERIALDEBUG
          Serial.println("State: Running");
#endif
          state = DICE_RUNNING;
          zeroTime = millis();
          waitForFirstZero = true;
          divider = 0;
        }
#ifdef SERIALDEBUG
        if (Serial.available() > 0) {
          uint8_t c = Serial.read();
          if (c == 'x') {
            Serial.println("State: Running");
            state = DICE_RUNNING;
            zeroTime = millis();
            waitForFirstZero = true;
            divider = 0;
          }
          if (c == 't') state = DICE_TEST;
        }
#endif
        // periodically call random function to get better randomness
        if (dividerRandom++ == 0) random(2, 13);
        if (dividerRandom >= DIVIDER_RANDOM) dividerRandom = 0;

        // animation
        if (!result1 || !result2) break;

        switch (sum) {
          case 2:
          case 3:
          case 4:
          case 5:
            break;

          case 6:
          case 8:
            if (fade) {
              addGlitter(30);
              fadeToBlack(16);
              printDice(result1, PATTERN_LENGTH, 0, colorCyan);
              printDice(result2, PATTERN_LENGTH, PATTERN_LENGTH, colorGreen);
              fade--;
            }
            else {
              fade = FADE_STEPS;
            }
            ring.show();

            if (divider++ == 0) {
              showVal(sum, toggle);
              toggle = !toggle;
            }
            if (divider >= DIVIDER_FLASH68) divider = 0;
            break;

          case 7:
            if (fade) {
              fadeToBlack(16);
              fade--;
            }
            else {
              printDice(result1, PATTERN_LENGTH, 0, colorYellow);
              printDice(result2, PATTERN_LENGTH, PATTERN_LENGTH, colorYellow);
              fade = FADE_STEPS;
            }
            ring.show();

            if (divider++ == 0) {
              showVal(sum, toggle);
              toggle = !toggle;
            }
            if (divider >= DIVIDER_FLASH7) divider = 0;
            break;

          case 9:
          case 10:
          case 11:
          case 12:
            break;

          default:
            break;
        }
        break;

      case DICE_RUNNING:
        fadeToBlack(64);
        pos1 = beatsin8(20, 0, NUM_LEDS - 1, zeroTime, 192);
        pos2 = (pos1 + PATTERN_LENGTH) % NUM_LEDS;
        ring.setPixelColor(pos1, colorMagenta);
        ring.setPixelColor(pos2, colorMagenta);
        ring.show();

        if (divider++ == 0) showVal(random(2, 13), true);
        if (divider >= DIVIDER_RUNNING) divider = 0;
        if (pos1) waitForFirstZero = false;

        if (pos1 == 0 && waitForFirstZero == false) {
#ifdef SERIALDEBUG
          Serial.println("State: Slowdown1");
#endif
          state = DICE_SLOWDOWN1;
          zeroTime = millis();
          result1 = random(1, 7);
          result2 = random(1, 7);
        }
        break;

      case DICE_SLOWDOWN1:
        fadeToBlack(4);
        pos1 = beatsin8(20, 0, PATTERN_LENGTH, zeroTime, 192);
        pos2 = (pos1 + PATTERN_LENGTH) % NUM_LEDS;
        ring.setPixelColor(pos1, colorCyan);
        ring.setPixelColor(pos2, colorGreen);

        printDice(result1, pos1, 0, colorCyan);
        printDice(result2, pos1, PATTERN_LENGTH, colorGreen);

        ring.show();

        if (divider == 0) showVal(random(2, 13), true);
        if (divider++ >= DIVIDER_SLOWDOWN1) divider = 0;

        if (pos1 == PATTERN_LENGTH - 1) {
#ifdef SERIALDEBUG
          Serial.println("State: Slowdown2");
#endif
          state = DICE_SLOWDOWN2;
          fadeCounter = 60;
        }
        break;

      case DICE_SLOWDOWN2:
        fadeToBlack(4);
        printDice(result1, PATTERN_LENGTH, 0, colorCyan);
        printDice(result2, PATTERN_LENGTH, PATTERN_LENGTH, colorGreen);
        ring.show();

        if (divider == 0) showVal(random(2, 13), true);
        if (divider++ >= DIVIDER_SLOWDOWN2) divider = 0;

        if (--fadeCounter == 0) {
          sum = result1 + result2;
          divider = 0;
          toggle = true;
          showVal(sum, true);
#ifdef SERIALDEBUG
          Serial.println("State: Wait");
#endif
          state = DICE_WAIT;
        }
        break;

      case DICE_CONFETTI:
        confetti(hue++);

        if (readButton(BUTTON_PIN)) {
#ifdef SERIALDEBUG
          Serial.println("State: Wait");
#endif
          state = DICE_WAIT;
          ring.clear();
        }
        ring.show();
        break;

      case DICE_TEST:
#ifdef SERIALDEBUG
        Serial.println("Starting test...");
#endif
        char disp[DISPLAY_DIGITS + 1];
        uint8_t numResult;
        for (uint8_t x = 2; x <= 12; x++) {
          numResult = 0;
          for (uint16_t i = 0; i < NUM_TEST; i++) {
            result1 = random(1, 7);
            result2 = random(1, 7);
            if (result1 + result2 == x) numResult++;
          }
          numResult /= 10;

          if (x >= 10) disp[0] = '0' + x / 10;
          else disp[0] = ' ';
          disp[1] = '0' + x % 10;
          if (numResult >= 10) disp[2] = '0' + numResult / 10;
          else disp[2] = ' ';
          disp[3] = '0' + numResult % 10;
          alpha.print(disp);
          delay(1000);
        }
        state = DICE_WAIT;
        break;
    }
  }
}

//---------------------------------------------------------------------------------

void printDice(uint8_t val, uint8_t maxPos, uint8_t offset, uint32_t color) {
  uint16_t p;

  if (offset + PATTERN_LENGTH > NUM_LEDS) return;

  p = pattern[val - 1].pattern;

  // write dice pattern
  for (uint8_t pos = 0; pos < maxPos; pos++) {
    if ((p >> pos) & 0x0001) ring.setPixelColor(offset + pos, color);
  }
}

void showVal(uint8_t val, bool on) {
  char disp[DISPLAY_DIGITS + 1];

  if (on) {
    if (val >= 10) disp[0] = '0' + val / 10;
    else disp[0] = ' ';
    disp[1] = '0' + val % 10;
    disp[2] = ' ';
    disp[3] = ' ';
    disp[4] = ' ';
    disp[5] = 0;
    alpha.print(disp);
  }
  else
    alpha.clear();
}

bool readButton(uint8_t pin) {

#if defined(__AVR_ATtiny1614__)
  return PORTA.IN & (1 << BUTTON_PIN) ? false : true;
#endif
#if defined (__AVR_ATtiny85__)
  return PINB & (1 << BUTTON_PIN) ? false : true;
#endif
  return false;
}

uint8_t beatsin8(uint16_t bpm, uint8_t low, uint8_t high, uint32_t time_offset, uint8_t phase_offset) {
  // 60.000 ms pro Minute / BPM = Dauer eines vollen Taktes in ms
  uint32_t ms_per_beat = 60000L / bpm;

  // Aktuelle Zeit plus den gewünschten Zeit-Versatz berechnen
  uint32_t adjusted_time = millis() - time_offset;

  // Position innerhalb des aktuellen Taktes ermitteln (Modulo-Überlauf)
  uint32_t pos = adjusted_time % ms_per_beat;

  // Zeit-Position sauber auf den Indexbereich 0-255 skalieren
  uint8_t base_index = (pos * 255) / ms_per_beat;

  // Phasenversatz hinzufügen (8-Bit Integer läuft automatisch bei 255 sauber auf 0 über)
  uint8_t final_index = base_index + phase_offset;

  // Den echten Sinuswert (0 bis 255) aus dem Flash-Speicher auslesen
  uint8_t wave = pgm_read_byte(&(sin8_lut[final_index]));

  // Welle auf das gewünschte Ziel-Fenster [low, high] skalieren
  uint16_t range = high - low;

#ifdef SERIALDEBUG2
  char buf[80];
  sprintf(buf, "beatsin8: pos: %lu, base_index: %u, final_index: %u, wave: %u", pos, base_index, final_index, wave);
  Serial.println(buf);
#endif

  return low + ((wave * range) / 255);
}

void fadeToBlack(uint8_t fadeValue) {
  for (uint8_t i = 0; i < ring.numPixels(); i++) {
    uint32_t c = ring.getPixelColor(i);
    uint16_t r = (c >> 16) & 0xFF;
    uint16_t g = (c >> 8) & 0xFF;
    uint16_t b = c & 0xFF;

    // Scale down colors (255 - fadeValue) / 256
    // If fadeValue is 64, it multiplies by 191/256 (~0.75)
    r = (r * (255 - fadeValue)) >> 8;
    g = (g * (255 - fadeValue)) >> 8;
    b = (b * (255 - fadeValue)) >> 8;

#ifdef SERIALDEBUG3
  char buf[80];
  sprintf(buf, "fadeToBlack: R: %u, G: %u, B: %u", r, g, b);
  Serial.println(buf);
#endif

    ring.setPixelColor(i, ring.Color(r, g, b));
  }
}

void addGlitter(uint8_t chanceOfGlitter) {
  if (random(256) < chanceOfGlitter) {
    int pos = random(ring.numPixels());
    uint32_t current = ring.getPixelColor(pos);

    // Extract RGB components using bitwise math
    uint8_t r = (current >> 16) & 0xFF;
    uint8_t g = (current >> 8) & 0xFF;
    uint8_t b = current & 0xFF;

    // Add white with saturation cap at 255
    r = (r + 255 > 255) ? 255 : r + 255; // effectively sets to white max
    // Or to actually mix/add: r = qadd8(r, 255) if using FastLED lib8tion,
    // but with pure standard Arduino integer math:
    r = (uint16_t)r + 255 > 255 ? 255 : r + 255; // simplified below:

    // Cleaner additive white assignment:
    ring.setPixelColor(pos, ring.Color(min(255, r + 255), min(255, g + 255), min(255, b + 255)));
  }
}

void confetti(uint16_t baseHue) {
  // Fade all existing pixels down by multiplying RGB by 245/256 (~4% fade per frame)
  for (uint16_t i = 0; i < ring.numPixels(); i++) {
    uint32_t c = ring.getPixelColor(i);
    if (c != 0) {
      uint8_t r = (c >> 16) & 0xFF;
      uint8_t g = (c >> 8) & 0xFF;
      uint8_t b = c & 0xFF;

      // Integer math multiplication & shift (fade to black)
      r = (r * 245) >> 8;
      g = (g * 245) >> 8;
      b = (b * 245) >> 8;

      ring.setPixelColor(i, r, g, b);
    }
  }

  // Pick a random pixel and add a bright speckle of color
  uint16_t pos = random(ring.numPixels());

  // Combine base hue with a random offset (0-63 range out of 65535 HSV hue space)
  uint16_t randomHue = (baseHue + random(64)) * 1024;
  uint32_t color = ring.ColorHSV(randomHue, 200, 255);

  // Blend by adding or simply setting the new speckle
  ring.setPixelColor(pos, color);
}
