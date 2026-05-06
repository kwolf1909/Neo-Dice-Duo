#include <FastLED.h>

//#define TINYWIREM
#define SOFTI2C

#ifdef TINYWIREM
#include <TinyWireM.h>
#endif
#ifdef SOFTI2C
#define SDA_PORT PORTB
#define SDA_PIN 3
#define SCL_PORT PORTB
#define SCL_PIN 4
#define I2C_FASTMODE 1
#include <SoftWire.h>
#endif

#include "AlphaDisplay.h"

#define DATA_PIN            2
#define BUTTON_PIN          0
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB
#define NUM_LEDS            24
#define BRIGHTNESS          64
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

#define SERIAL
//#define SERIALDEBUG

#define HUE_RED             0
#define HUE_ORANGE          32
#define HUE_YELLOW          64
#define HUE_GREEN           96
#define HUE_AQUA            128
#define HUE_BLUE            160
#define HUE_PURPLE          192
#define HUE_PINK            224

#define COLOR_DICE1         HUE_GREEN
#define COLOR_DICE2         HUE_AQUA
#define COLOR_DICE7         HUE_YELLOW
#define COLOR_RUNNING       HUE_PURPLE

bool     waitForFirstZero, toggle;
uint8_t  state, result1, result2, fade, pos1, pos2, sum, divider, dividerRandom, fadeCounter;
uint32_t currentTime, animationTime, zeroTime;

struct dicePattern {
  uint8_t startPos;
  uint16_t pattern;
};

enum { DICE_INIT = 1, DICE_WAIT, DICE_RUNNING, DICE_SLOWDOWN1, DICE_SLOWDOWN2, DICE_PRINT, DICE_TEST };

const dicePattern pattern[] = { { 5, 0b000001000000 }, { 4, 0b000010100000 }, { 3, 0b000101010000 },
                                { 2, 0b001010101000 }, { 1, 0b010101010100 }, { 0, 0b101010101010 } };

CRGB leds[NUM_LEDS];

AlphaDisplay alpha;

void setup() {
  delay(100);

#ifdef SERIAL
  Serial.begin(9600);
//Serial.println("Init...");
  delay(100);
#endif

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

#ifdef TINYWIREM
  TinyWireM.begin();
#else
  Wire.begin();
#endif

  alpha.init(DISPLAY_ADDRESS, DISPLAY_DIGITS, DISPLAY_BRIGHTNESS);
  alpha.print("IN IT");

  uint8_t hue = 0;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(hue, 255, 255);
    hue += 10;
    if (i) leds[i - 1] = 0;
    FastLED.show();
    delay(ANIMATION_DELAY * 2);
  }
  FastLED.clear();
  FastLED.show();

  alpha.clear();
  
  animationTime = millis();
  state = DICE_INIT;
}

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
        state = DICE_WAIT;
        break;

      case DICE_WAIT:
        if (readButton(BUTTON_PIN)) {
          state = DICE_RUNNING;
          zeroTime = millis() + ANIMATION_DELAY;
          waitForFirstZero = true;
          divider = 0;
        }
#ifdef SERIAL
        if (Serial.available() > 0) {
          uint8_t c = Serial.read();
          if (c == 'x') {
            state = DICE_RUNNING;
            zeroTime = millis() + ANIMATION_DELAY;
            waitForFirstZero = true;
            divider = 0;
          }
          if (c == 't') state = DICE_TEST;
        }
#endif

        // periodically call random function to get better randomness
        if (dividerRandom++ == 0) random(2,13);
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
              fadeToBlackBy(leds, NUM_LEDS, 32);
              printDice(result1, PATTERN_LENGTH, 0, COLOR_DICE1);
              printDice(result2, PATTERN_LENGTH, PATTERN_LENGTH, COLOR_DICE2);
              fade--;
            }
            else {
              fade = FADE_STEPS;
            }
            FastLED.show();
            
            if (divider++ == 0) {
              showVal(sum, toggle);
              toggle = !toggle;
            }
            if (divider >= DIVIDER_FLASH68) divider = 0;  
            break;

          case 7:
            if (fade) {
              fadeToBlackBy(leds, NUM_LEDS, 16);
              fade--;
            }
            else {
              printDice(result1, PATTERN_LENGTH, 0, COLOR_DICE7);
              printDice(result2, PATTERN_LENGTH, PATTERN_LENGTH, COLOR_DICE7);
              fade = FADE_STEPS;
            }
            FastLED.show();

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
        fadeToBlackBy(leds, NUM_LEDS, 32);
        pos1 = beatsin8(20, 0, NUM_LEDS - 1, zeroTime, 192);
        leds[pos1] = CHSV(COLOR_RUNNING, 255, 255);
        if (pos1 + PATTERN_LENGTH < NUM_LEDS) pos2 = pos1 + PATTERN_LENGTH;
        else pos2 = pos1 + PATTERN_LENGTH - NUM_LEDS;
        leds[pos2] = CHSV(COLOR_RUNNING, 255, 255);
        FastLED.show();
#ifdef SERIALDEBUG
        Serial.printf("%u, %u\n\r", pos1, pos2);
#endif
        if (pos1 == 0 && waitForFirstZero == false) {
          state = DICE_SLOWDOWN1;
          zeroTime = millis() + ANIMATION_DELAY;
          result1 = random(1, 7);
          result2 = random(1, 7);
        }
        if (divider++ == 0) showVal(random(2, 13), true);
        if (divider >= DIVIDER_RUNNING) divider = 0;

        if (pos1) waitForFirstZero = false;
        break;

      case DICE_SLOWDOWN1:
        fadeToBlackBy(leds, NUM_LEDS, 32);
        pos1 = beatsin8(20, 0, PATTERN_LENGTH, zeroTime, 192);
        leds[pos1] = CHSV(COLOR_DICE1, 255, 255);
        if (pos1 + PATTERN_LENGTH < NUM_LEDS) pos2 = pos1 + PATTERN_LENGTH;
        else pos2 = pos1 + PATTERN_LENGTH - NUM_LEDS;
        leds[pos2] = CHSV(COLOR_DICE2, 255, 255);

        printDice(result1, pos1, 0, COLOR_DICE1);
        printDice(result2, pos1, PATTERN_LENGTH, COLOR_DICE2);

        FastLED.show();
#ifdef SERIALDEBUG
        Serial.printf("%u, %u\n\r", pos1, pos2);
#endif
        if (divider++ == 0) showVal(random(2, 13), true);
        if (divider >= DIVIDER_SLOWDOWN1) divider = 0;

        if (pos1 == PATTERN_LENGTH - 1) {
          state = DICE_SLOWDOWN2;
          fadeCounter = 60;
        }
        break;

      case DICE_SLOWDOWN2:
        fadeToBlackBy(leds, NUM_LEDS, 16);
        printDice(result1, PATTERN_LENGTH, 0, COLOR_DICE1);
        printDice(result2, PATTERN_LENGTH, PATTERN_LENGTH, COLOR_DICE2);
        FastLED.show();

        if (divider++ == 0) showVal(random(2, 13), true);
        if (divider >= DIVIDER_SLOWDOWN2) divider = 0;

        if (fadeCounter-- == 0) {
          sum = result1 + result2;
          divider = 0;
          toggle = true;
          showVal(sum, true);
          state = DICE_WAIT;
        }
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
            if(result1 + result2 == x) numResult++;
          }
          numResult /= 10;
          
          if (x >= 10) disp[0] = '0' + x / 10;
          else disp[0] = ' ';
          disp[1] = '0' + x % 10;
          disp[2] = ' ';
          if(numResult >= 10) disp[3] = '0' + numResult / 10;
          else disp[3] = ' ';
          disp[4] = '0' + numResult % 10;
          alpha.print(disp);
          delay(1000);
        }
        state = DICE_WAIT;
        break;
    }
  }
}

void printDice(uint8_t val, uint8_t maxPos, uint8_t offset, uint8_t color) {
  uint16_t p;

  if (offset + PATTERN_LENGTH > NUM_LEDS) return;

  p = pattern[val - 1].pattern;

  // write dice pattern
  for (uint8_t pos = 0; pos < maxPos; pos++) {

    if ((p >> pos) & 0x0001)
      leds[offset + pos] = CHSV( color, 255, 255);
    //  else
    //    leds[offset + pos] = CHSV( 0, 0, 0);
  }
}
void addGlitter(fract8 chanceOfGlitter) {
  if (random8() < chanceOfGlitter) {
    leds[random16(NUM_LEDS)] += CRGB::White;
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
  bool val;
  
  // read pin by polling
  DDRB &= ~(1 << pin);
  delay(1);
  val = PINB & (1 << pin);
  DDRB |= 1 << pin;
  return !val;
}
