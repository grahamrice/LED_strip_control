
#include <Adafruit_NeoPixel.h>

const int led = 2;      //for the strip!

#define pixelCount 96

Adafruit_NeoPixel strip = Adafruit_NeoPixel(pixelCount, led, NEO_GRB + NEO_KHZ800);

enum routine_t { rainbowR, rainbowChunksR, colourPulseR, colourChunksR, colourWipeR, colourFadeR, colourSlideR, solidColourR, offR};

enum colour_opt_t {oc_single, oc_double, oc_invert, oc_random};

routine_t currentRoutine = offR;

int partyState = 0;
int pulseCounter = 0;
uint16_t routineCounter = 0;

uint32_t solidColourHolder = 0x00000000;      //laziness
uint8_t red_p = 0, green_p = 0, blue_p = 0; //primary colours
uint8_t red_s = 0, green_s = 0, blue_s = 0; //secondary colours

uint8_t option_mode   = 0; //normal, strobe, theatrechase
uint8_t option_colour = 0; //single,double, invert, random
uint8_t option_slider = 0;

byte toggle = 0;

bool switched_off = True;
bool direction = False; //false count up, true count down
bool set_clear = False; //false for colour1, true for colour2

uint32_t random_hold; //hold a random value, future randoms will be psuedo-random bit ops from this
uint8_t random_counter; // when counter rolls over, random_hold will be updated with a new value

/*------------------------------- General functions--------------------------------------------------------*/

uint32_t limitBrightness(uint8_t rrr,uint8_t ggg,uint8_t bbb){
  if((rrr + ggg + bbb) > 575){
   return strip.Color(rrr * 0.75, ggg * 0.75, bbb * 0.75);
  }else{
    return strip.Color(rrr, ggg, bbb);
  }
}

uint32_t rotate_left(uint32_t input, int shifts) //24 bit rotate, don't shift more than 24
{
  uint32_t result = 0;
  result = input << shifts;
  result |= (input >> (24 - shifts));
  return result & 0xffffff;
}

void shuffle_colour(uint32_t * colour_in)
{
  uint32_t old = *colour_in;
  old += 0xa75d23;
  old = rotate_left(old,7);
  *colour_in = old ^ 0x8f61dc;
}

uint32_t get_random(){
   shuffle_colour(&random_hold);
   return random_hold & 0xffffff;
}

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

/***********************************LED strip functions*********************************************************/
uint8_t j = 0; //use routineCounter for small counter and j for direction changes


// Fill the dots one after the other with a color
void colourWipe(){ //uint8_t r, uint8_t g, uint8_t b, uint8_t wait, bool setDir, bool rev) {
 uint32_t colour1, colour2;

  switch(option_colour){
    case oc_single: colour1 = strip.Color(red_p, green_p, blue_p);
                    colour2 = 0;
                    break;
    case oc_double: colour1 = strip.Color(red_p, green_p, blue_p);
                    colour2 = strip.Color(red_s, green_s, blue_s);
                    break;
    case oc_invert: colour1 = strip.Color(red_p, green_p, blue_p);
                    colour2 = strip.Color(0xff - red_p, 0xff - green_p, 0xff - blue_p);
                    break;
    case oc_random: colour1 = random_hold;
                    colour2 = 0;
                    break;
  }

  if(routineCounter >=pixelCount){
      routineCounter = 0;
      j++;
      switch(j){ //perhaps move this into a function if the bools make sense for other routines.
        case 1: direction = false; //second colour L->R
                set_clear = true;
                break;
        case 2: direction = true; //first colour R->L
                set_clear = false;
                break;
        case 3: direction = true; //second colour R->L
                set_clear = true;
                break;
        case 4: direction = false; //first colour L->R
                set_clear = false;
                break;
        default: j = 0;
                direction = false;
                set_clear = false;
                break;
      }
   }
   strip.setPixelColor((direction) ? pixelCount - routineCounter - 1 : routineCounter, (set_clear)? colour2 : colour1);      //if reverse is true, start from last pixel
   strip.show();
    routineCounter++;
}

void colourSolid() {
  uint32_t oneBigColour;
  if(switched_off) oneBigColour = 0;
  else{
    switch(option_colour){
      case oc_single: oneBigColour = strip.Color(red_p, green_p, blue_p);
                      break;
      case oc_double: oneBigColour = strip.Color(red_s, green_s, blue_s);
                      break;
      case oc_invert: oneBigColour = strip.Color((red_p + red_s)/2, (green_p + green_s)/2, (blue_p + blue_s)/2);
                      break;
      case oc_random: oneBigColour = random_hold;
                      break;
    }
  }
  for(uint16_t i=0; i<pixelCount; i++) {
      strip.setPixelColor(i,oneBigColour);
    }
}



void rainbow() { //set slider to 1 for single colour, 384 for 1.5 rainbows across pixels - 400 for simplicity, will go up to 1.56 rainbows across
    uint16_t slider = option_slider << 2;
    if(j >=256){
        j = 0;
      }
      for(int i=0; i< strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel(((i * slider / strip.numPixels()) + j) & 255)); //halving to 128 spreads the full spectrum over unused pixels
      }
      j++;
  }

void rainbowChunks(){



}

void colourPulse(){ //uint8_t r, uint8_t g, uint8_t b, uint8_t wait){
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
}

void colourChunks(){



}

void colourFade(){



}

void colourSlide(){



}

/*------------------------------------------------------------------------*/

void apply_mode(uint8_t check){
  //use check to disable features for certain modes

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
  random_hold = random(0x1000000);
  strip.begin();
  strip.show();
}

/*-------------------------------get data--------------------------------------------*/

uint8_t check_slider(uint8_t input)
{
  if(input > 100) return 100;
  return input;
}

/* see bottom of file for data packet structure*/
void getData(int data_length){
    byte input[9]; // = {0,0,0,0};
    Serial.readBytes(&input[0],data_length);
    if(0x50 == (input[0] & 0xf0)){            //upper 4 bits == 0x5X
      currentRoutine = input[0] & 0x0f;
      red_p = input[1];
      green_p = input[2];
      blue_p = input[3];
      switched_off = false;
    }
    if((data_length == 9) && ((input[8] & 0x0f) == 0x0a)){  //last 4 bits == 0x0a
      option_slider = check_slider(input[4]);
      red_s = input[5];
      green_s = input[6];
      blue_s = input[7];
      option_colour = (input[8] >> 6) & 0x03;
      option_mode   = (input[8] >> 4) & 0x03;
    }
}


/***********************************main loop*****************************************/
void loop() {

  if(Serial.available() == 4){        //should be 4 bytes in original version
      getData(4);
   }else if(Serial.available() == 9){ //update to 9 byte packets for 2 colour picker
      getData(9);
   }else if(Serial.available() > 0){   //don't bother if there are no bytes in the buffer.
    delay(1);                       //give it another chance if mid-message
    if(Serial.available() == 4){
      getData(4);
    }else if(Serial.available() == 9){
      getData(9);
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
if !(switched_off){
   switch(currentRoutine){
    rainbowR:         rainbow();
                      apply_mode(0xff);
                      strip.show();
                      wait(30);
                      break;
    rainbowChunksR:   rainbowChunks();
                      apply_mode(0xff);
                      strip.show();
                      wait(100);
                      break;
    colourPulseR:     colourPulse();
                      apply_mode(0xff);
                      strip.show();
                      wait(100);
                      break;
    colourChunksR:    colourChunks();
                      apply_mode(0xff);
                      strip.show();
                      wait(100);
                      break;
    colourWipeR:      colourWipe();
                      apply_mode(0xff);
                      strip.show();
                      wait((option_slider > 10) ? option_slider : 10);
                      break;
    colourFadeR:      colourFade();
                      apply_mode(0xff);
                      strip.show();
                      wait((option_slider > 10) ? option_slider : 10);
                      break;
    colourSlideR:     colourSlide();
                      apply_mode(0xff);
                      strip.show();
                      wait(100);
                      break;
    solidColourR:     colourSolid();
                      apply_mode(0xff);
                      strip.show();
                      wait(100);
                      break;
    offR:             if(!switched_off){
                        switched_off = True;
                        colourSolid();
                        strip.show();
                      }
                      wait(100);
                      break;
    default:          currentRoutine = offR;
                      break;
   }
   if(0 == --random_counter) get_random(); //change random value
  }
}

/*---------------------------------------------
byte 0 - 0x5yyyy;  yyyy = 4 bit routine
byte 1 - red1
byte 2 - green1
byte 3 - blue1 - 4 byte mode will only include these bytes

byte 4 - slider value
byte 5 - red2
byte 6 - green2
byte 7 - blue2
byte 8 - 0xzzwwA; zz = colour option, ww = mode option



-----------------------------------------------*/
