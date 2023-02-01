// Colour Theremin
// Code for Arduino, using a 3.5" TFT and ultrasonic distance sensor
// by Donovan Aikman

// NOTES:
// This requires:
//     - Adafruit GFX library for graphics
//     - MCUFRIEND_kbv library for the screen
//     - SPI library to talk to the distance sensor
// These are all available in the standard Arduino IDE Library Manager
//
// You will need to update the default GFX library to the following values:
// #define TFTWIDTH   320
// #define TFTHEIGHT  480
// As the library was originally intended for a smaller Adafruit screen.

// About colour:
// Colour accuracy is only suggestive here as this is a prototype.
// The TFT's 16-bit colour is actually 5-6-5 bits for r, g, and b; but here we ignore g's extra bit for simplicity.
// Effectively, that's only 0-31 as values to cover 0-255 in RGB, so you could multiply by 8 for 32 steps.
// However, 31 * 8 = 248 so we are missing those extra 7 steps to 255.
// The RGBsmooth function smooths values out over the 31 steps, so that the prototype appears smoother than it is.

//////////////////////////////////////////////////////////////////////////
// Distance sensor details
#define TRIGGER 44
#define ECHO 45

//////////////////////////////////////////////////////////////////////////
//TFT required definitions to use MCUFRIEND_kbv and Adafruit GFX libraries
#define LCD_CS A3 
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
#define LCD_RESET A4

#include <SPI.h>         
#include "Adafruit_GFX.h"
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;

// Colour codes
#define	BLACK   0x0000
#define WHITE   0xFFFF
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0


//////////////////////////////////////////////////////////////////////////
// Global variables

const int refresh_rate = 250;
const int redPin       = 35;
const int greenPin     = 33;
const int bluePin      = 31;
const int dist_floor   = 200;
const int dist_scale   = 50;
int redBtnState        = 0;
int greenBtnState      = 0;
int blueBtnState       = 0;
int red, green, blue;
int r0, g0, b0;
word rgb;
char to_str[7];

//////////////////////////////////////////////////////////////////////////
// helper functions
//////////////////////////////////////////////////////////////////////////
// This is just handy for lining up our RGB values.
char* int_to_str(int16_t i) {
  sprintf(to_str, "%3d", i);
  return to_str;
}

// Used during setup for initial values.
// It is kept separate for the future addition of a colour reset button that could also call this.
void resetRGB(void) {
  red   = 31;
  green = 31;
  blue  = 31;
  r0    = red;
  g0    = green;
  b0    = blue;
  rgb   = RGBtoHEX(red, green, blue);
}

// Converts our 0-31 colour scale in the TFT's 5-6-5 bit colour format:
//                rrrrr0gggggbbbbb
// See note at the top about colour. More info on GFX colour format here:
// https://learn.adafruit.com/adafruit-gfx-graphics-library/coordinate-system-and-units
word RGBtoHEX(word r, word g, word b) {
  word rgbhex = ( (r << 11) | (g << 6) | b );
  return  rgbhex;
}

// This is a fix to prevent Arduino from printing hex values less than 16 without the leading zero.
// It assumes you have set up everything to print elsewhere.
void HEXPrint(int value){
  if (value < 16) tft.print("0");
  tft.print(value, HEX);
}

// This smooths that colour out over the 31 steps so that the prototype appears smoother than it is.
// See colour note at the top.
int RGBsmooth(int value){
  return (value * 8) + round(value * 7 / 31);
}

// These are the static elements of the UI we draw only once as the TFT frame buffer is slow.
void drawUI(void){
  tft.setRotation(1);
  tft.fillScreen(BLACK);
  tft.drawRect(80, 0, 320, 320, WHITE);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.setTextColor(RED);
  tft.print("Red =>");
  tft.setRotation(2);
  tft.setCursor(30, 410);
  tft.setTextColor(GREEN);
  tft.print("Green =>");
  tft.setRotation(0);
  tft.setCursor(110, 440);
  tft.setTextColor(BLUE);
  tft.print("(Blue <>)");
  tft.setRotation(1);

}

//////////////////////////////////////////////////////////////////////////
//Application processes
//////////////////////////////////////////////////////////////////////////

// Erases the previous square, draws the new one, and updates the numbers.
// The TFT only accepts a small bandwidth of drawing changes to screen values or it struggles to keep up.
void drawTFT(void){
  // Draw our square for r & g axis. Add a ^ v character for travel in the b axis.
  tft.fillRect((r0 * 9 + 85 ), (g0 * 9 + 5), 30, 30, BLACK);
  tft.fillRect((red * 9 + 85 ), (green * 9 + 5), 30, 30, rgb);
  if (blue > b0) {
    tft.drawChar((red * 9 + 100 ), (green * 9 + 5), 0x1E, ~rgb, rgb, 2);
  }
  if (blue < b0){
    tft.drawChar((red * 9 + 100 ), (green * 9 + 5), 0x1F, ~rgb, rgb, 2);    
  }

  // Write out our values.
  tft.setTextSize(1);
  tft.setCursor(10,160);
  tft.setTextColor(RED, BLACK);
  tft.print("#"); HEXPrint(RGBsmooth(red)); HEXPrint(RGBsmooth(green)); HEXPrint(RGBsmooth(blue));
  tft.setTextSize(2);
  tft.setCursor(10,180);
  tft.setTextColor(RED, BLACK);
  tft.print(int_to_str(RGBsmooth(red)));
  tft.setCursor(10,210);
  tft.setTextColor(GREEN, BLACK);
  tft.print(int_to_str(RGBsmooth(green)));
  tft.setCursor(10,240);
  tft.setTextColor(BLUE, BLACK);
  tft.print(int_to_str(RGBsmooth(blue)));
}

// Draws a static colour chip and displays its numbers.
// This will loop until released by a button press.
void paintChipMode(void){
  tft.fillScreen(rgb);

  tft.setTextSize(3);
  tft.setCursor(10,120);
  tft.setTextColor(RED, BLACK);
  tft.print("#"); HEXPrint(RGBsmooth(red)); HEXPrint(RGBsmooth(green)); HEXPrint(RGBsmooth(blue));
  tft.setTextSize(3);
  tft.setCursor(10,180);
  tft.setTextColor(RED, BLACK);
  tft.print(int_to_str(RGBsmooth(red)));
  tft.setCursor(10,210);
  tft.setTextColor(GREEN, BLACK);
  tft.print(int_to_str(RGBsmooth(green)));
  tft.setCursor(10,240);
  tft.setTextColor(BLUE, BLACK);
  tft.print(int_to_str(RGBsmooth(blue)));

  // Wait for a button to be pressed so we can return to the main loop.
  while (redBtnState != HIGH && blueBtnState != HIGH && greenBtnState != HIGH){
    redBtnState   = digitalRead(redPin);
    blueBtnState  = digitalRead(bluePin);
    greenBtnState = digitalRead(greenPin);
    delay(refresh_rate);
  }

  // Reset the screen to the colour select interface.
  drawUI();
}

// This gets the distance values and applies them sensibly to the colour requested by loop();
// Distance floor and scaling can be adjusted in the constants as desired if you need more or less room to hand wave.
// Floor represents the minimum distance to get above 0 and to discourage touch the sensor.
// Scale represents how squished the accepted range will be.
void colourSelectMode(int changingColour){
  // First, store previous colour values for erasing later.
  r0 = red;
  g0 = green;
  b0 = blue;

  // Get our distance value for colour.
  long distance, duration;
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER, LOW);
  duration = pulseIn(ECHO,HIGH);
  distance = (duration - dist_floor) / dist_scale;

  // This cleans up or discards outside values, as we don't want to go outside 0-31.
  // Extreme values (>100) can occur if a hand is too close, so just discard and return without change.
  if (distance > 100){
    return;
  } else if (distance < 0) {
    distance = 0;
  } else if (distance > 31) {
    distance = 31;
  }

  // Assign distance value to colour as requested.
  if (changingColour == 1) {
    red   = distance;
  } else if (changingColour == 2){
    green = distance;
  } else {
    blue  = distance;
  }

  // convert it to TFT's colour scale
  rgb = RGBtoHEX(red, green, blue);

  // go draw the dynamic elements of the TFT
  drawTFT();
}

//////////////////////////////////////////////////////////////////////////
//Arduino main functions
//////////////////////////////////////////////////////////////////////////

void setup(void) {
  // Serial setup.
  Serial.begin(9600);
  if (!Serial) delay(5000);
  Serial.println("Color Plate setup started.");

  // distance sensor setup
  pinMode(ECHO, INPUT);
  pinMode(TRIGGER, OUTPUT);
  digitalWrite(TRIGGER, LOW);

  // TFT setup.
  uint16_t ID = tft.readID();
  if (ID == 0xD3D3) ID = 0x9481; 
  tft.begin(ID);
  drawUI();

  // Button setup.
  pinMode(redPin, INPUT);
  pinMode(bluePin, INPUT);
  pinMode(greenPin, INPUT);

  // General setup.
  resetRGB();
  drawTFT();
  Serial.println("Color Plate setup complete.");
}

void loop(void) {
  // Go looking for button presses to know what colour to calibrate on
  // or lock into the paint chip mode.
  redBtnState   = digitalRead(redPin);
  blueBtnState  = digitalRead(bluePin);
  greenBtnState = digitalRead(greenPin);
  if (redBtnState == HIGH){
    colourSelectMode(1);
  } else if (greenBtnState == HIGH) {
    colourSelectMode(2);
  } else if (blueBtnState == HIGH) {
    colourSelectMode(3);
  } else {
    paintChipMode();
  }

  // Wait.
  delay(refresh_rate);
}    