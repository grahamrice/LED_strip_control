
#include <Adafruit_NeoPixel.h>

const int led = 2;      //for the strip!

#define pixelCount 96

Adafruit_NeoPixel strip = Adafruit_NeoPixel(pixelCount, led, NEO_GRB + NEO_KHZ800);

enum routine_t { rainbowR, rainbowCycR, partyR, colourWipeR, solidColourR, colourPulseR, offR, inactiveR};   //inactive is essentially off but will not sleep. Used after awakening

routine_t currentRoutine = offR;

int partyState = 0;
int pulseCounter = 0;
uint8_t routineCounter = 0;


uint32_t solidColourHolder = 0x00000000;      //laziness
uint8_t red_p = 0, green_p = 0, blue_p = 0; //primary colours
uint8_t red_s = 0, green_s = 0, blue_s = 0; //secondary colours

uint8_t option_mode   = 0; //normal, strobe, theatrechase
uint8_t option_colour = 0; //single,double, invert, random

byte toggle = 0;

uint32_t limitBrightness(uint8_t rrr,uint8_t ggg,uint8_t bbb){
  if((rrr + ggg + bbb) > 575){
   return strip.Color(rrr * 0.75, ggg * 0.75, bbb * 0.75);
  }else{
    return strip.Color(rrr, ggg, bbb);
  }
}


/***********************************LED strip functions*********************************************************/
uint16_t j = 0;

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}


// Fill the dots one after the other with a color
void colorWipe(uint8_t r, uint8_t g, uint8_t b, uint8_t wait, bool setDir, bool rev) {
 uint32_t oneBigColour = limitBrightness(r, g, b);
  if(routineCounter >=pixelCount){
      routineCounter = 0;
      partyState++;
   }
   uint8_t reverse;
   uint8_t setClr;
   if(setDir){
     reverse = rev;
     setClr = 0;
   }else{
    reverse = partyState & 2;
    setClr = partyState & 1;                        //set on first part of routine, clear on second
   }
   strip.setPixelColor((reverse) ? pixelCount - routineCounter - 1 : routineCounter, (setClr)? 0 : oneBigColour);      //if reverse is true, start from last pixel
   strip.show();
    routineCounter++;

    delay(wait);
  //}
}

void colorSolid(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t oneBigColour = limitBrightness(r, g, b);
  for(uint16_t i=0; i<pixelCount; i++) {
      strip.setPixelColor(i,oneBigColour);
    }
    strip.show();
  }



void rainbow(uint8_t wait) {
  uint16_t i;

  //for(j=0; j<256; j++) {
  if(j >=256){
      j = 0;
    }
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i>>3)+j) & 255));
    }
    strip.show();
    j++;
    delay(wait);
  //}
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i;

  //for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
   if(j >=256*5){
      j = 0;
    }
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    j++;
    delay(wait);
  //}
}

void colorPulse(uint8_t r, uint8_t g, uint8_t b, uint8_t wait){
  strip.clear();
    if(pulseCounter >= 4 + pixelCount){
      pulseCounter = -4;
      routineCounter = 0;
      partyState++;
   }
   uint8_t reverse = partyState & 1;
   if(routineCounter < 5){
       routineCounter++;
    }else{
           if((pulseCounter >= 0)&&(pulseCounter < pixelCount)){
                strip.setPixelColor((reverse) ? pixelCount - pulseCounter - 1 : pulseCounter, r, g, b);        //set centre of pulse to max colour
           }
           if((pulseCounter + 1 < pixelCount)&&(pulseCounter + 1 >= 0)){
              strip.setPixelColor((reverse) ? pixelCount - pulseCounter - 2 : pulseCounter + 1, (uint8_t) r * 0.6, (uint8_t)g * 0.6, (uint8_t)b * 0.6);        //set centre of pulse to max colour
              }
           if((pulseCounter - 1 >= 0)&&(pulseCounter - 1 < pixelCount)){
              strip.setPixelColor((reverse) ? pixelCount - pulseCounter : pulseCounter - 1,  (uint8_t)r * 0.6, (uint8_t)g * 0.6, (uint8_t)b * 0.6);        //set centre of pulse to max colour
              }
           if((pulseCounter + 2 < pixelCount)&&(pulseCounter + 2 >= 0)){
              strip.setPixelColor((reverse) ? pixelCount - pulseCounter - 3 : pulseCounter + 2,(uint8_t) r * 0.4,(uint8_t) g * 0.4,(uint8_t) b * 0.4);        //set centre of pulse to max colour
              }
           if((pulseCounter - 2 >= 0)&&(pulseCounter - 2 < pixelCount)){
              strip.setPixelColor((reverse) ? pixelCount - pulseCounter + 1 : pulseCounter - 2,(uint8_t) r * 0.4,(uint8_t) g * 0.4, (uint8_t)b * 0.4);        //set centre of pulse to max colour
              }
           if((pulseCounter + 3 < pixelCount)&&(pulseCounter + 3 >= 0)){
              strip.setPixelColor((reverse) ? pixelCount - pulseCounter - 4 : pulseCounter + 3, (uint8_t) r * 0.2,(uint8_t) g * 0.2,(uint8_t) b * 0.2);        //set centre of pulse to max colour
              }
           if((pulseCounter - 3 >= 0)&&(pulseCounter - 3 < pixelCount)){
              strip.setPixelColor((reverse) ? pixelCount - pulseCounter + 2 : pulseCounter - 3, (uint8_t)r * 0.2, (uint8_t)g * 0.2, (uint8_t)b * 0.2);        //set centre of pulse to max colour
              }
              pulseCounter++;
    }
     strip.show();
     delay(wait);
}


//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait, bool reverse) {
  //for (int j=0; j<10; j++) {  //do 10 cycles of chasing
   if(j >=10){
      j = 0;
      partyState++;
    }
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor((reverse) ? strip.numPixels() - i -q - 1 : i+q, c);    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor((reverse) ? strip.numPixels() - i -q - 1 : i+q, 0);        //turn every third pixel off
      }
    }
    j++;
  //}
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait, bool reverse) {
  //for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
   if(j >=256){
      j = 0;
      partyState++;
    }
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor((reverse) ? strip.numPixels() - i -q - 1 : i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor((reverse) ? strip.numPixels() - i -q - 1 : i+q, 0);        //turn every third pixel off
      }
    }
    j++;
  //}
}

/*****************************************setup***************************************/


void setup() {
  // initialize digital pin 13 as an output.
  pinMode(13, OUTPUT);
  Serial.begin(115200);
  strip.begin();
  strip.show();
}

void getData(int data_length){
    byte input[8]; // = {0,0,0,0};
    Serial.readBytes(&input[0],data_length);
    if(0x50 == (input[0] & 0xf0)){            //upper 4 bits == 0x5X
      currentRoutine = input[0] & 0x0f;
      red_p = input[1];
      green_p = input[2];
      blue_p = input[3];
    }
    if((data_length == 8) && ((input[7] & 0x0f) == 0x0a)){  //last 4 bits == 0x0a
      red_s = input[4];
      green_s = input[5];
      blue_s = input[6];
      option_colour = (input[7] >> 6) & 0x03;
      option_mode   = (input[7] >> 4) & 0x03;
    }
}


/***********************************main loop*****************************************/
void loop() {

  if(Serial.available() == 4){        //should be 4 bytes in original version
      getData(4);
   }else if(Serial.available() == 8){ //update to 8 byte packets for 2 colour picker
      getData(8);
   }else if(Serial.available() > 0){   //don't bother if there are no bytes in the buffer.
    delay(1);                       //give it another chance if mid-message
    if(Serial.available() == 4){
      getData(4);
    }else if(Serial.available() == 8){
      getData(8);
    }else{
      byte x;
      while(Serial.available() > 0){
        x = Serial.read();            //flush the shit out
      }
    }
   }

   //blink LED? or flash/toggle LED everytime data is received or whatever so we know if it is crashing in this routine. Or is it getting stuck in while serial.available >0?
   //would an interrupt be better?
digitalWrite(13, ++toggle & 0x04);
//delay(100);
   switch(currentRoutine){
    case rainbowR:        rainbow(100);
                          break;
    case rainbowCycR:   rainbowCycle(100);
                          break;
    case partyR:      switch(partyState){
              case 0: colorWipe(0x9A, 0x33, 0x33, 10, true, false);
                  break;
              case 1: colorWipe(0x33, 0x9A, 0x33, 10,true,  false);
                  break;
              case 2: colorWipe(0x33, 0x33, 0x9A, 10,true,  false);
                  break;
              case 3: theaterChase(strip.Color(0x9A, 0x33, 0x33), 30, true);
                  break;
              case 4: theaterChase(strip.Color(0x9A, 0x9A, 0x33), 30, true);
                  break;
              case 5: theaterChase(strip.Color(0x33, 0x33, 0x9A), 30, true);
                  break;
              case 6: theaterChaseRainbow(40, true);
                  break;
              case 7: theaterChase(strip.Color(0x9A, 0x33, 0x33), 30, false);
                  break;
              case 8: theaterChase(strip.Color(0x9A, 0x9A, 0x33), 30, false);
                  break;
              case 9: theaterChase(strip.Color(0x33, 0x33, 0x9A), 30, false);
                  break;
              case 10:  colorWipe(0x9A, 0x33, 0x33, 10, true, true);
                  break;
              case 11: colorWipe(0x33, 0x9A, 0x33, 10, true, true);
                  break;
              case 12: colorWipe(0x33, 0x33, 0x9A, 10, true, true);
                  break;
              case 13:     partyState = 0;
                  break;
              default:  partyState = 0;
              break;
              }
              break;
    case colourPulseR:    colorPulse(red_p, green_p, blue_p,25);
                          break;
    case colourWipeR:     colorWipe(red_p, green_p, blue_p,25,  false, false);
                          break;
    case solidColourR:     colorSolid(red_p, green_p, blue_p);      //set the colour once then do nothing
                          currentRoutine = inactiveR;
                          break;
    case inactiveR:       delay(100);
                          break;
    case offR:            colorSolid(0, 0, 0);      //changed from 3 uint8_t to a single uint32_t
                          currentRoutine = inactiveR;
                          break;
    default:              currentRoutine = inactiveR;
                          break;
  }
}
