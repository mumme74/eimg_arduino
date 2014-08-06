/*
  Example program for using EImg to draw pictures on a tft screen
  
  license: public domain

*/

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <limits.h>
#include <stdint.h>
#include "images.h"
#include <eimg.h>

#define TFT_CS    10
#define TFT_DC    9
#define TFT_RESET 8




// initialize global display driver class
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RESET);
void drawFunc(int16_t x, int16_t y, uint16_t color){
  tft.drawPixel(x, y, color);
}


//typedef EImg16Bits<Adafruit_ST7735::drawPixel>::type EImg16;
//EImg16::setDrawFunction(&Adafruit_ST7735::drawPixel);

EImg16 down(down_png, &tft, &Adafruit_ST7735::drawPixel);
EImg16 carrot(carrot_png, &tft, &Adafruit_ST7735::drawPixel);
EImg16 house(house_png, &tft, &Adafruit_ST7735::drawPixel);
EImg16 lion(lion_png, &tft, &Adafruit_ST7735::drawPixel);
EImg16 car(car_png, &tft, &Adafruit_ST7735::drawPixel);
EImg16 storebro(storebro_png, &tft, &Adafruit_ST7735::drawPixel);
EImg16 vw(vw_png, &tft, &Adafruit_ST7735::drawPixel);
EImg16 survolt(survolt_png, &tft, &Adafruit_ST7735::drawPixel);




void setup() {
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  
  tft.setRotation(1);
  
  
  // clear display
  tft.fillScreen(ST7735_WHITE);
}

void loop() {
  tft.fillScreen(0xFFFF);
  down.draw(10, 10);
  delay(1000);
  carrot.draw(10,30);
  delay(1000);
  house.draw(32,0);
  delay(1000);
  lion.draw(32, 0);
  delay(1000);
  tft.fillScreen(0xFFFF);
  car.draw(32, 0);
  delay(1000);
  tft.fillScreen(0xFFFF);
  storebro.draw(15,0);
  delay(2000);
  
  uint32_t first = millis();
  tft.fillScreen(0xFFFF);
  vw.draw(0,0);
  Serial.write("render time (VW):"); Serial.print(millis() - first); Serial.write('\n');
  delay(1000);
  tft.fillScreen(0xFFFF);
  survolt.draw(0,0);
  delay(1000);
}

