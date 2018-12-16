
#include <Adafruit_NeoPixel.h>
#include "pixel_hold.h"

const int led = 2;      //for the strip!

#define pixelCount 96

Adafruit_NeoPixel strip = Adafruit_NeoPixel(pixelCount, led, NEO_GRB + NEO_KHZ800);

pixel_hold holder = pixel_hold();

enum routine_t { rainbowR, rainbowChunksR, rainbowPulseR, rainbowCometR, colourCometR, colourPulseR, colourChunksR, colourWipeR, colourFadeR, colourSlideR, solidColourR, offR};

enum colour_opt_t {oc_single, oc_double, oc_invert, oc_random};

#define OM_NORMAL  0x1
#define OM_THEATRE 0x2
#define OM_STROBE  0x4
#define OM_ALL     0x7

routine_t currentRoutine = offR;

int partyState = 0;
int pulseCounter = 0;
uint16_t routineCounter = 0;
uint8_t theatre_counter = 0;

uint32_t solidColourHolder = 0x00000000;      //laziness
uint8_t red_p = 0, green_p = 0, blue_p = 0; //primary colours
uint8_t red_s = 0, green_s = 0, blue_s = 0; //secondary colours

uint8_t option_mode   = 0; //normal, strobe, theatrechase
uint8_t option_colour = 0; //single,double, invert, random
uint8_t option_slider = 0; //does not change when in strobe or theatre chase modes
uint8_t latest_slider = 0;  //most up to date slider for the strobe and theatre chase options

byte toggle = 0;

bool switched_off = true;
bool direction = false; //false count up, true count down
bool set_clear = false; //false for colour1, true for colour2

bool recalc_colour = true;

uint32_t random_hold; //hold a random value, future randoms will be psuedo-random bit ops from this
uint8_t random_counter; // when counter rolls over, random_hold will be updated with a new value

#define STROBE_OFF_T 23
#define STROBE_ON_T  37


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
   recalc_colour = true; //because random invert
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
 static uint32_t colour1, colour2;
  if (recalc_colour){
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
    recalc_colour = false;
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
   holder.pixel_set((direction) ? pixelCount - routineCounter - 1 : routineCounter, (set_clear)? colour2 : colour1);
   //strip.setPixelColor((direction) ? pixelCount - routineCounter - 1 : routineCounter, (set_clear)? colour2 : colour1);      //if reverse is true, start from last pixel
   //strip.show();
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
  holder.pixel_setall(oneBigColour);
  /*for(uint16_t i=0; i<pixelCount; i++) {
      strip.setPixelColor(i,oneBigColour);
    }*/
}



void rainbow() { //set slider to 1 for single colour, 384 for 1.5 rainbows across pixels - 400 for simplicity, will go up to 1.56 rainbows across
    uint16_t slider = option_slider << 2;
    if(j >=256){
        j = 0;
      }
      for(int i=0; i< strip.numPixels(); i++) {
        holder.pixel_set(i, Wheel(((i * slider / strip.numPixels()) + j) & 255));
        //strip.setPixelColor(i, Wheel(((i * slider / strip.numPixels()) + j) & 255)); //halving to 128 spreads the full spectrum over unused pixels
      }
      j++;
  }

void rainbowChunks(){
  uint16_t slider = option_slider >> 2; // slider controls width of chunks of the same colour
  uint16_t loRes = 0; //low resolution counter instead of relying on this thing to do integer divides
  if(j >=256){
      j = 0;
  }
  for(int i = 0; i < pixelCount; i++) {
    if (i - loRes > slider) loRes += slider;
    holder.pixel_set(i,Wheel(loRes+j));      //remove j from here but put into line above like i + j - loRes if colour changes slightly on each increment of j (this may be desirable)
    //strip.setPixelColor(i,Wheel(loRes+j));
  }
  j++;
}

void rainbowPulse(){ //single coloured pulse that fires across screen, cycles through 7 colours
  static int width = 1;
  static uint32_t rbow_colurs[8];
  if (recalc_colour) {
    width = 1 + ((50 + option_slider) >> 4);
    pulseCounter = 0 - 8 - width ;

    for(int i = 0; i < 255; i+=36){ //8 options
      rbow_colurs[i] = Wheel(i); //spread colours over pulse length
    }
  }

  if(pulseCounter >= 8 + width + pixelCount){
      pulseCounter = 0 - 8 - width ;
      routineCounter = 0;
      j++;
      if((j & 7) == 0) direction = !direction; //change direction when all colours have cycled
   }

   for(int i=0; i<pixelCount; i++){
     if((i >= pulseCounter) && (i < pulseCounter + 8 + width)){
       holder.pixel_set((direction) ?  pixelCount - i - 1 : i, rbow_colurs[ j & 0x7 ] );
     }
   }


}

#define MAX_WIDTH 18
void rainbowComet() { //comet of all rainbow colours in a short pulse (should look very mariokart rainbow road)
  static int width = 1;
  static uint32_t rbow_colurs[MAX_WIDTH];
  int j;
  if (recalc_colour) {
    width = 1 + ((50 + option_slider) >> 4);
    pulseCounter = 0 - 8 - width ;

    for(int i = 0; i < 8 + width; i++){
      rbow_colurs[i] = Wheel((i * 255)/8+width); //spread colours over pulse length
    }
  }

  if(pulseCounter >= 8 + width + pixelCount){
      pulseCounter = 0 - 8 - width ;
      routineCounter = 0;
      partyState++;
      direction = !direction;
   }

  j = 0;
  for(int i=0; i<pixelCount; i++){
    if((i >= pulseCounter) && (i < pulseCounter + 8 + width)){
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, rbow_colurs[j++] );
    }
  }

}

void colourPulse(){ //uint8_t r, uint8_t g, uint8_t b, uint8_t wait){
  static int width = 1;
  static uint8_t r_fade[4] = {0,0,0,0};
  static uint8_t g_fade[4] = {0,0,0,0};
  static uint8_t b_fade[4] = {0,0,0,0};
  static uint8_t colour1_r, colour1_g, colour1_b, colour2_r, colour2_g, colour2_b;
  uint8_t index = 0;
  if (recalc_colour) {
    width = 1 + (option_slider >> 4);
    switch(option_colour){
    case oc_double: colour1_r = red_p;
                    colour1_g = green_p;
                    colour1_b = blue_p;
                    colour2_r = red_s;
                    colour2_g = green_s;
                    colour2_b = blue_s;
                    break;
    case oc_invert: colour1_r = red_p;
                    colour1_g = green_p;
                    colour1_b = blue_p;
                    colour2_r = 0xff - red_p;
                    colour2_g = 0xff - green_p;
                    colour2_b = 0xff - blue_p;
                    break;
    case oc_random: colour1_r = (random_hold >> 16) & 0xff;
                    colour1_g = (random_hold >> 8) & 0xff;
                    colour1_b = (random_hold) & 0xff;
                    colour2_r = 0xff - colour1_r;
                    colour2_g = 0xff - colour1_g;
                    colour2_b = 0xff - colour1_b; //random and random invert, right??
                    break;
    //case oc_single: redundant line if default
    default:        colour1_r = red_p;
                    colour1_g = green_p;
                    colour1_b = blue_p;
                    colour2_r = 0;
                    colour2_g = 0;
                    colour2_b = 0;
                    break;
    }
    r_fade[0] = (uint8_t)((colour1_r * 0.8) + (colour2_r * 0.2));
    r_fade[1] = (uint8_t)((colour1_r * 0.6) + (colour2_r * 0.4));
    r_fade[2] = (uint8_t)((colour1_r * 0.4) + (colour2_r * 0.6));
    r_fade[3] = (uint8_t)((colour1_r * 0.2) + (colour2_r * 0.8));
    g_fade[0] = (uint8_t)((colour1_g * 0.8) + (colour2_g * 0.2));
    g_fade[1] = (uint8_t)((colour1_g * 0.6) + (colour2_g * 0.4));
    g_fade[2] = (uint8_t)((colour1_g * 0.4) + (colour2_g * 0.6));
    g_fade[3] = (uint8_t)((colour1_g * 0.2) + (colour2_g * 0.8));
    b_fade[0] = (uint8_t)((colour1_b * 0.8) + (colour2_b * 0.2));
    b_fade[1] = (uint8_t)((colour1_b * 0.6) + (colour2_b * 0.4));
    b_fade[2] = (uint8_t)((colour1_b * 0.4) + (colour2_b * 0.6));
    b_fade[3] = (uint8_t)((colour1_b * 0.2) + (colour2_b * 0.8));

    pulseCounter = 0 - 8 - width ;
  }

  //strip.clear();

  if(pulseCounter >= 8 + width + pixelCount){
      pulseCounter = 0 - 8 - width ;
      routineCounter = 0;
      partyState++;
      direction = !direction;
   }

   /*for(int i = 0-8-width; i < pixelCount + 8 + width; i++){*/
   for(int i=0; i<pixelCount; i++){
     if((pulseCounter <= i)&&(i < pulseCounter + 4)) { //"before" the pulse
        index = 3 - i - pulseCounter;
        holder.pixel_set((direction) ?  pixelCount - i - 1 : i, strip.Color(r_fade[index],g_fade[index],b_fade[index]) );
        //strip.setPixelColor((direction) ?  pixelCount - i - 1 : pulseCounter,r_fade[index],g_fade[index],b_fade[index]);
     }
     else if((pulseCounter + 4 + width <= i)&&(i < pulseCounter + 8 + width)) { //"after" the pulse
       index = i - pulseCounter - 4 - width;
       holder.pixel_set((direction) ?  pixelCount - i - 1 : i, strip.Color(r_fade[index],g_fade[index],b_fade[index]) );
       //strip.setPixelColor((direction) ?  pixelCount - i - 1 : pulseCounter,r_fade[index],g_fade[index],b_fade[index]);
   }
   else if((pulseCounter + 4 <= i)&&(i < pulseCounter + 4 + width)){ //during the pulse
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, strip.Color(colour2_r,colour1_g,colour1_b) );
      //strip.setPixelColor((direction) ?  pixelCount - i - 1 : i,colour2_r,colour1_g,colour1_b);
   }
   else{    //in the secondary colour space
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, strip.Color(colour2_r,colour2_g,colour2_b));
      //strip.setPixelColor((direction) ?  pixelCount - i - 1 : i,colour2_r,colour2_g,colour2_b);
   }
   }
    pulseCounter++;
}

void colourChunks(){
  holder.pixel_setall(0); //just do something for now


}

void colourFade(){
  static uint8_t colour1_r, colour1_g, colour1_b, colour2_r, colour2_g, colour2_b, width = 1;
  uint8_t fin_r,fin_g,fin_b;
  float prim, sec;
  uint8_t index = 0;
  if (recalc_colour) {
    width = 1 + (option_slider >> 4);
    switch(option_colour){
    case oc_double: colour1_r = red_p;
                    colour1_g = green_p;
                    colour1_b = blue_p;
                    colour2_r = red_s;
                    colour2_g = green_s;
                    colour2_b = blue_s;
                    break;
    case oc_invert: colour1_r = red_p;
                    colour1_g = green_p;
                    colour1_b = blue_p;
                    colour2_r = 0xff - red_p;
                    colour2_g = 0xff - green_p;
                    colour2_b = 0xff - blue_p;
                    break;
    case oc_random: colour1_r = (random_hold >> 16) & 0xff;
                    colour1_g = (random_hold >> 8) & 0xff;
                    colour1_b = (random_hold) & 0xff;
                    colour2_r = 0xff - colour1_r;
                    colour2_g = 0xff - colour1_g;
                    colour2_b = 0xff - colour1_b; //random and random invert, right??
                    break;
    //case oc_single: redundant line if default
    default:        colour1_r = red_p;
                    colour1_g = green_p;
                    colour1_b = blue_p;
                    colour2_r = 0;
                    colour2_g = 0;
                    colour2_b = 0;
                    break;
    }
  }
  if(j >=256){
      j = 0;
      direction = !direction;
  }

    if(direction) prim = (float)j/256;
    else          prim = (float)(256 - j)/256;
    sec = 1.0 - prim;
    fin_r = (colour1_r * prim) + (colour2_r * sec);
    fin_g = (colour1_g * prim) + (colour2_g * sec);
    fin_b = (colour1_b * prim) + (colour2_b * sec);
    for(int i = 0; i < pixelCount; i++) {
      holder.pixel_set(i, strip.Color(fin_r,fin_g,fin_b));
      //strip.setPixelColor(i,fin_r,fin_g,fin_b);
    }
    j++;
}

void colourSlide(){ //like a snail



}

void colourComet(){
  static uint8_t colour1_r, colour1_g, colour1_b, colour2_r, colour2_g, colour2_b, width = 1;
  uint32_t fin_1,fin_2;
  static uint32_t fade[8];
  int j;
  if (recalc_colour) {
    width = 1 + (option_slider >> 4);
    pulseCounter = 0 - 8 - width ;
    switch(option_colour){
    case oc_double: colour1_r = red_p;
                    colour1_g = green_p;
                    colour1_b = blue_p;
                    colour2_r = red_s;
                    colour2_g = green_s;
                    colour2_b = blue_s;
                    break;
    case oc_invert: colour1_r = red_p;
                    colour1_g = green_p;
                    colour1_b = blue_p;
                    colour2_r = 0xff - red_p;
                    colour2_g = 0xff - green_p;
                    colour2_b = 0xff - blue_p;
                    break;
    case oc_random: colour1_r = (random_hold >> 16) & 0xff;
                    colour1_g = (random_hold >> 8) & 0xff;
                    colour1_b = (random_hold) & 0xff;
                    colour2_r = 0xff - colour1_r;
                    colour2_g = 0xff - colour1_g;
                    colour2_b = 0xff - colour1_b; //random and random invert, right??
                    break;
    //case oc_single: redundant line if default
    default:        colour1_r = red_p;
                    colour1_g = green_p;
                    colour1_b = blue_p;
                    colour2_r = 0;
                    colour2_g = 0;
                    colour2_b = 0;
                    break;
    }

    fin_1 = strip.Color(colour1_r, colour1_g, colour1_b);
    fin_2 = strip.Color(colour2_r, colour2_g, colour2_b);

    for(int m = 0, n = 7; m < 8; m++, n--){
      fade[m] = strip.Color( ((colour1_r*n)>>3) + ((colour2_r*m)>>3) , ((colour1_g*n)>>3) + ((colour2_g*m)>>3), ((colour1_b*n)>>3) + ((colour2_b*m)>>3));
    }
  }

  if(pulseCounter >= 8 + width + pixelCount){
      pulseCounter = 0 - 8 - width ;
      routineCounter = 0;
      partyState++;
      direction = !direction;
   }

  j = 0;
  for(int i=0; i<pixelCount; i++){
    if((i >= pulseCounter + 8) && (i < pulseCounter + 8 + width)){  //within bulk of the comet
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, fin_1 );
    }else if((i >= pulseCounter) && (i < pulseCounter + 8)){ //within comet tail
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, fade[j] );
    }else{ //outside of comet, use second colour
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, fin_2 );
    }
  }



}

/*------------------------------------------------------------------------*/

void apply_theatre(){
  //will loop through once before returning
  uint8_t time_del = (latest_slider >> 1) + 25;
  if(theatre_counter > 2) theatre_counter = 0;
  for(int f = 0; f < pixelCount; f+=3) strip.setPixelColor(f+theatre_counter,holder.pixel_get(f+theatre_counter));
  strip.show();
  delay(time_del);
  for(int f = 0; f < pixelCount; f+=3) strip.setPixelColor(f+theatre_counter,0);
  theatre_counter++;
}

void apply_strobe(){
  uint16_t time_off = (1 + (latest_slider >> 3))  * STROBE_OFF_T; //0->100 becomes 1->13, * OFF time
  for(int f = 0; f < pixelCount; f++) strip.setPixelColor(f,holder.pixel_get(f));
  strip.setBrightness(0xf0);
  strip.show();
  delay(STROBE_ON_T);
  strip.setBrightness(0x01);
  strip.show();
  delay(time_off);

}

void apply_normal(){
  for(int f = 0; f < pixelCount; f++) strip.setPixelColor(f,holder.pixel_get(f));
}

void apply_mode(uint8_t check,int del){
  //use check to disable features for certain modes
  //delay is the delay under normal operation
  uint8_t thing = option_mode & check;
  switch(thing){
    OM_THEATRE: apply_theatre();
                break;
    OM_STROBE:  apply_strobe();
                break;
    OM_NORMAL:
    default:    apply_normal();
                strip.show();
                delay(del);
                break;
  }

}

//Theatre-style crawling lights.
/*void theaterChase(uint32_t c, uint8_t wait, bool reverse) {
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
}*/

//Theatre-style crawling lights with rainbow effect
/*void theaterChaseRainbow(uint8_t wait, bool reverse) {
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
}*/

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

void update_slider()
{
  if(option_mode == OM_NORMAL) option_slider = latest_slider; //update value when in normal mode
}

uint8_t check_mode(uint8_t input){
  uint8_t result = 0;
  switch(input){
    case 1: result = OM_THEATRE;
    case 2: result = OM_STROBE;
    default: result = OM_NORMAL;
  }
  return result;
}

/* see bottom of file for data packet structure*/
void getData(int data_length){
    byte input[9]; // = {0,0,0,0};
    Serial.readBytes(&input[0],data_length);
    if((0x50 == (input[0] & 0xf0))  && ((input[8] & 0x0f) == 0x0a)) {            //upper 4 bits == 0x5X, last 4 bits == 0x0a
      currentRoutine = input[0] & 0x0f;
      red_p = input[1];
      green_p = input[2];
      blue_p = input[3];
      switched_off = false;
      recalc_colour = true;
      red_s = input[5];
      green_s = input[6];
      blue_s = input[7];
      option_colour = (input[8] >> 6) & 0x03;
      option_mode   = check_mode((input[8] >> 4) & 0x03);
      latest_slider = check_slider(input[4]);
      update_slider();
    }
}


/***********************************main loop*****************************************/
void loop() {

  if(Serial.available() == 9){ //9 byte packets for 2 colour picker
      getData(9);
   }else if(Serial.available() > 0){   //don't bother if there are no bytes in the buffer.
    delay(1);                       //give it another chance if mid-message
    if(Serial.available() == 9){
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
if (!switched_off){
   switch(currentRoutine){
    rainbowR:         rainbow();
                      apply_mode(OM_ALL,30);
                      break;
    rainbowChunksR:   rainbowChunks();
                      apply_mode(OM_ALL,100);
                      break;
    rainbowPulseR:    rainbowPulse();
                      apply_mode(OM_ALL,100);
                      break;
    rainbowCometR:    rainbowComet();
                      apply_mode(OM_ALL,100);
                      break;
    colourCometR:     colourComet();
                      apply_mode(OM_ALL,100);
                      break;
    colourPulseR:     colourPulse();
                      apply_mode(OM_ALL,100);
                      break;
    colourChunksR:    colourChunks();
                      apply_mode(OM_ALL,100);
                      break;
    colourWipeR:      colourWipe();
                      apply_mode(OM_ALL,((option_slider > 10) ? option_slider : 10));
                      break;
    colourFadeR:      colourFade();
                      apply_mode(OM_ALL,((option_slider > 10) ? option_slider : 10));
                      break;
    colourSlideR:     colourSlide();
                      apply_mode(OM_ALL,100);
                      break;
    solidColourR:     colourSolid();
                      apply_mode(OM_ALL,100);
                      break;
    offR:             if(!switched_off){
                        switched_off = true;
                        colourSolid();
                        strip.show();
                      }
                      delay(100);
                      break;
    default:          currentRoutine = offR;
                      break;
   }
   if(0 == --random_counter) get_random(); //change random value
   pulseCounter++;
  }
}

/*---------------------------------------------
byte 0 - 0x5yyyy;  yyyy = 4 bit routine
byte 1 - slider value 0 -> 100
byte 2 - red1
byte 3 - green1
byte 4 - blue1
byte 5 - red2
byte 6 - green2
byte 7 - blue2
byte 8 - 0xzzwwA; zz = colour option, ww = mode option



-----------------------------------------------*/
