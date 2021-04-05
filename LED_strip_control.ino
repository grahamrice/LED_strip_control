#include <Base64.h>
#define URL_SAFE
#include <Adafruit_NeoPixel.h>
#include "pixel_hold.h"
#include <EEPROM.h>

#define STRIP_PIN 2
#define BT_STATUS_PIN 3
#define STATUS_PIN 4

#define pixelCount 96

Adafruit_NeoPixel strip = Adafruit_NeoPixel(pixelCount, STRIP_PIN, NEO_GRB + NEO_KHZ800);

pixel_hold holder = pixel_hold();

#define rainbowR 0
#define rainbowChunksR 1
#define rainbowPulseR 2
#define rainbowCometR 3
#define colourCometR 4
#define colourPulseR 5
#define colourChunksR 6
#define colourWipeR 7
#define colourFadeR 8
#define colourSlideR 9
#define solidColourR 0xA
#define offR 0xB

#define OC_SINGLE 0
#define OC_DOUBLE 1
#define OC_INVERT 2
#define OC_RANDOM 3

#define OM_NORMAL  0x1
#define OM_THEATRE 0x2
#define OM_STROBE  0x4
#define OM_MIRROR  0x8
#define OM_ALL     0xf

uint8_t currentRoutine = rainbowR;
uint8_t issued_command = 0;

int partyState = 0;
int pulseCounter = 0;
uint8_t theatre_counter = 0;

//these colours are those set by the user
uint8_t red_p = 0, green_p = 0, blue_p = 0; //primary colour
uint8_t red_s = 0, green_s = 0, blue_s = 0; //secondary colour

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

uint8_t pause_time = 0;

#define STROBE_OFF_T 23
#define STROBE_ON_T  37

//these global colours are used by functions to calculate fade colours etc.
 uint8_t colour1_r, colour1_g, colour1_b, colour2_r, colour2_g, colour2_b;

 uint8_t routineCounter = 0; //use j for small counter as a local var in functions
                             //routineCounter for direction changes and sequence control
#define MAX_WIDTH 32
#define MIN_WIDTH  16

const char dev_name[20] = "LEDSTRIP_TV";

uint8_t width = MIN_WIDTH;

int bt_device_number = -1;

boolean use_default;

void set_width(uint8_t w){
  width = w;
  if (width > MAX_WIDTH) width = MAX_WIDTH;
  if (width < MIN_WIDTH) width = MIN_WIDTH;
}

inline uint8_t get_width(){
  return width;
}
/*------------------------------*/
typedef struct 
{
  void * fun_ptr;
  short delay_period; //if -ve then use slider defaults
  uint8_t routine_code;
  uint8_t modes;
  
}routine_struct;

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
  old += 0x5a5a5a;
  old = rotate_left(old,7);
  *colour_in = old ^ 0xa5a5a5;
}

uint32_t get_random(){
   shuffle_colour(&random_hold);
   recalc_colour = true; //because random invert
   return random_hold & 0xffffff;
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(uint8_t WheelPos) {
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

uint32_t get_global_colour(int which){
  if (which == 1) return strip.Color(colour1_r,colour1_g,colour1_b);
  else return strip.Color(colour2_r,colour2_g,colour2_b);
}

void assign_colours(){
  switch(option_colour){
  case OC_DOUBLE: colour1_r = red_p;
                  colour1_g = green_p;
                  colour1_b = blue_p;
                  colour2_r = red_s;
                  colour2_g = green_s;
                  colour2_b = blue_s;
                  break;
  case OC_INVERT: colour1_r = red_p;
                  colour1_g = green_p;
                  colour1_b = blue_p;
                  colour2_r = 0xff - red_p;
                  colour2_g = 0xff - green_p;
                  colour2_b = 0xff - blue_p;
                  break;
  case OC_RANDOM: colour1_r = (random_hold >> 16) & 0xff;
                  colour1_g = (random_hold >> 8) & 0xff;
                  colour1_b = (random_hold) & 0xff;
                  colour2_r = 0xff - colour1_r;
                  colour2_g = 0xff - colour1_g;
                  colour2_b = 0xff - colour1_b; //random and random invert, right??
                  break;
  case OC_SINGLE:
  default:        colour1_r = red_p;
                  colour1_g = green_p;
                  colour1_b = blue_p;
                  colour2_r = 0;
                  colour2_g = 0;
                  colour2_b = 0;
                  break;
  }
}

/***********************************LED strip functions*********************************************************/

// Fill the dots one after the other with a color
void colourWipe(){ //uint8_t r, uint8_t g, uint8_t b, uint8_t wait, bool setDir, bool rev) {
   if (recalc_colour){
    assign_colours();
    recalc_colour = false;
  }

  if (routineCounter > 3) routineCounter = 0;

  if(pulseCounter >=pixelCount){
      pulseCounter = 0;
      if(routineCounter == 0){
        direction = false; //first colour L->R
        set_clear = false;
        routineCounter = 1;
      }else if(routineCounter == 1){
        direction = false; //second colour L->R
        set_clear = true;
        routineCounter = 2;
      }else if(routineCounter == 2){
        direction = true;
        set_clear = false;
        routineCounter = 3;
      }else if(routineCounter == 3){
        direction = true;
        set_clear = true;
        routineCounter = 0;
      }else{
        direction = false;
        set_clear = false;
        routineCounter = 0;
      }
      pause_time = 3;
  }
  if(pause_time == 0){
   for(int i = 0; i < pixelCount; i++){
     if (i >= pixelCount) holder.pixel_set((direction) ? pixelCount - pulseCounter - 1 : pulseCounter, (set_clear)? get_global_colour(2) : get_global_colour(1)); // this one
     else holder.pixel_set((direction) ? pixelCount - pulseCounter - 1 : pulseCounter, (set_clear)? get_global_colour(1) : get_global_colour(2)); // this one
   }
   //strip.setPixelColor((direction) ? pixelCount - routineCounter - 1 : routineCounter, (set_clear)? colour2 : colour1);      //if reverse is true, start from last pixel
   //strip.show();
  }

}

void colourSolid() {
  uint32_t oneBigColour;
  if(switched_off) oneBigColour = 0;
  else{
    switch(option_colour){ //this func can do its own thing
      case OC_DOUBLE: oneBigColour = strip.Color(red_s, green_s, blue_s);
                      break;
      case OC_INVERT: oneBigColour = strip.Color((red_p + red_s)/2, (green_p + green_s)/2, (blue_p + blue_s)/2);
                      break;
      case OC_RANDOM: oneBigColour = random_hold;
                      break;
      case OC_SINGLE:
      default:        oneBigColour = strip.Color(red_p, green_p, blue_p);
                      break;
      }
  }
  holder.pixel_setall(oneBigColour); // this one
  /*for(uint16_t i=0; i<pixelCount; i++) {
      strip.setPixelColor(i,oneBigColour);
    }*/
}

void routineOff(){
  uint32_t oneBigColour = 0;
  holder.pixel_setall(oneBigColour);
}


void rainbow() { //set slider to 1 for single colour, 384 for 1.5 rainbows across pixels - 400 for simplicity, will go up to 1.56 rainbows across
    uint16_t slider = 1 + (option_slider << 2);
    for(int i=0; i< strip.numPixels(); i++) {
      holder.pixel_set(i, Wheel(((i * slider / strip.numPixels()) + routineCounter) & 255));// // this one
      //strip.setPixelColor(i, Wheel(((i * slider / strip.numPixels()) + routineCounter) & 255)); //halving to 128 spreads the full spectrum over unused pixels
    }
    routineCounter++;
  }

void rainbowChunks(){
  uint8_t loRes = 0; //, shifter; //low resolution counter instead of relying on this thing to do integer divides

  if(recalc_colour){
    set_width(10 + option_slider >> 2); // slider controls width of chunks of the same colour
    recalc_colour = false;
  }
    
  for(int i = 0; i < pixelCount; i++) {
    loRes = routineCounter + (i * 3);
    loRes -= (loRes % get_width());
    holder.pixel_set(i,Wheel(loRes));  // this one     //remove j from here but put into line above like i + j - loRes if colour changes slightly on each increment of j (this may be desirable)
    //strip.setPixelColor(i,Wheel(loRes+routineCounter));
  }
  routineCounter+=3;
}

const uint32_t rbow_colours[8] = {0xff0000, 0x9f6000, 0x3fc000, 0x00de21, 0x007e81, 0x001ee1, 0x4200bd, 0xa2005d};

void rainbowPulse(){ //single coloured pulse that fires across screen, cycles through 7 colours
  
  if (recalc_colour) {
    set_width(0xA + (option_slider >> 2));
    //pulseCounter = 0 - get_width() ;

    recalc_colour = false;
  }

  if(pulseCounter >= get_width() + pixelCount){
      pulseCounter = 0 - get_width() ;
      routineCounter++;
      if((routineCounter & 7) == 0) direction = !direction; //change direction when all colours have cycled
      //pause_time = 10;
   }

   
   for(int i=0; i<pixelCount; i++){
     if((i >= pulseCounter) && (i < pulseCounter + get_width())){
       holder.pixel_set((direction) ?  pixelCount - i - 1 : i, rbow_colours[ routineCounter & 0x7 ] ); // this one
     }else{
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, 0x0 ); // this one
     }
   }


}

void rainbowComet() { //comet of all rainbow colours in a short pulse (should look very mariokart rainbow road)
  static uint32_t rbow_colurs[MAX_WIDTH];
  int j;
 
  if (recalc_colour) {
    set_width(0xA + (option_slider >> 2));
    
    //pulseCounter = 0 - get_width() ;

    for(int i = 0; i < get_width(); i++){
      rbow_colurs[i] = Wheel((i * 255)/get_width()); //spread colours over pulse length
    }
    recalc_colour = false;
  }

  if(pulseCounter >= get_width() + pixelCount){
      pulseCounter = 0 - get_width() ;
      partyState++;
      direction = !direction;
      pause_time = 3;
   }

  j = 0;
  for(int i=0; i<pixelCount; i++){
    if((i >= pulseCounter) && (i < pulseCounter + get_width())){
      if(j > MAX_WIDTH) j = 0;
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, rbow_colurs[j++] ); // this one
    }else{
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, 0 ); // this one
    }
  }

}

void colourPulse(){ //uint8_t r, uint8_t g, uint8_t b, uint8_t wait){
  
  static uint8_t r_fade[4] = {0,0,0,0};
  static uint8_t g_fade[4] = {0,0,0,0};
  static uint8_t b_fade[4] = {0,0,0,0};

  int index = 0;
 
  if (recalc_colour) {
    set_width(0xA + (option_slider >> 2));
    assign_colours();
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

    //pulseCounter = 0 - 8 - get_width() ;
    recalc_colour = false;
  }

  if(pulseCounter >= get_width() + 8 + pixelCount){ //additional 8 for the faded part
      pulseCounter = 0 - 8 - get_width() ;
      routineCounter = 0;
      partyState++;
      direction = !direction;
      pause_time = 3;
   }


   for(int i=0; i<pixelCount; i++){
     if((pulseCounter <= i)&&(i < (pulseCounter + 4))) { //"before" the pulse
        index = 3 - (i - pulseCounter);
        holder.pixel_set((direction) ?  pixelCount - i - 1 : i, strip.Color(r_fade[index],g_fade[index],b_fade[index]) ); // this one
        //strip.setPixelColor((direction) ?  pixelCount - i - 1 : pulseCounter,r_fade[index],g_fade[index],b_fade[index]);
     }else if(((pulseCounter + 4 + get_width()) <= i)&&(i < (pulseCounter + 8 + get_width()))) { //"after" the pulse
       index = (i - pulseCounter) - 4 - get_width();
       holder.pixel_set((direction) ?  pixelCount - i - 1 : i, strip.Color(r_fade[index],g_fade[index],b_fade[index]) ); // this one
       //strip.setPixelColor((direction) ?  pixelCount - i - 1 : pulseCounter,r_fade[index],g_fade[index],b_fade[index]);
   }else if(((pulseCounter + 4) <= i)&&(i < (pulseCounter + 4 + get_width()))){ //during the pulse
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, get_global_colour(1) ); // this one
      //strip.setPixelColor((direction) ?  pixelCount - i - 1 : i,colour2_r,colour1_g,colour1_b);
   }else{    //in the secondary colour space
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, get_global_colour(2)); // this one
      //strip.setPixelColor((direction) ?  pixelCount - i - 1 : i,colour2_r,colour2_g,colour2_b);
   }
   }
}

void colourChunks(){
  int j;
  bool k = false;
  
  if(recalc_colour){
    set_width(0xA + (option_slider >> 2));
    //pulseCounter = 0 - get_width() ;

    assign_colours();
    recalc_colour = false;
  }

  if(pulseCounter >= (get_width()<<1)) pulseCounter = 0; //pulse counter has gone past 2 widths, reset it

  if(pulseCounter >= get_width()){
    k = true; //pulseCounter has shifted us along at least 1 width, therefore start with second colour
    j = pulseCounter - get_width();
  }else{
    j = pulseCounter;
  }

  for(int i = 0; i < pixelCount; i++, j++){ //j is a running counter to determine if we have reached 1 width again, k will toggle each time that happens
    if(j >= get_width()){
      j = 0;
      k = !k;
    }
    if(k) holder.pixel_set((direction) ?  pixelCount - i - 1 : i, get_global_colour(2) ); // this one
    else holder.pixel_set((direction) ?  pixelCount - i - 1 : i, get_global_colour(1) ); // this one
  }
}

void colourFade(){
  uint8_t fin_r,fin_g,fin_b;
  float prim, sec;
  uint8_t index = 0;
  
  if (recalc_colour) {
    assign_colours();
    recalc_colour = false;
  }

    if(direction) prim = (float)routineCounter/256;
    else          prim = (float)(256 - routineCounter)/256;
    sec = 1.0 - prim;
    fin_r = (colour1_r * prim) + (colour2_r * sec);
    fin_g = (colour1_g * prim) + (colour2_g * sec);
    fin_b = (colour1_b * prim) + (colour2_b * sec);
    for(int i = 0; i < pixelCount; i++) {
      holder.pixel_set(i, strip.Color(fin_r,fin_g,fin_b)); // this one
      //strip.setPixelColor(i,fin_r,fin_g,fin_b);
    }
    routineCounter++;
    if(routineCounter == 0){
      direction = !direction;
    }
}

void colourSlide(){ //like a snail
  static uint8_t extend_lim = 3, extend = 0;
  static bool ext_con = true; //true to extend, False to contract
  const int min_len = 0xA;
  static int endCounter;
  
  if (recalc_colour) {
    extend_lim = 1 + (option_slider >> 2);
    assign_colours();
  }

  if(pulseCounter >= min_len + pixelCount){
      pulseCounter = 0 - min_len ;
      endCounter = pulseCounter + min_len;
      partyState++;
      direction = !direction;
      extend = 0; //extend represents the fron position, pulseCounter the rear
      ext_con = true;
      recalc_colour = false;
   }else{
      if (ext_con){
        extend++;
        endCounter++;
        if (extend >= extend_lim) ext_con = false;
      }else{
        extend--;   //else increment pulseCounter up one, but hold the end position in the same place by decrementing
        pulseCounter++;
        if (extend == 0) ext_con = true;
      }
   }
//   Serial.write(0xa5);
//   Serial.write(endCounter);
//   Serial.write(pulseCounter);
  for(int i=0; i<pixelCount; i++){
    if((i >= pulseCounter) && (i < endCounter)){  //within bulk of the snail
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, get_global_colour(1) ); // this one
    }else{ //outside of snail, use second colour
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, get_global_colour(2) ); // this one
    }
  }
  pause_time = 0x7f; //block the pulseCounter increment
}

void colourComet(){ //like colour pulse, but fade only on one side
  static uint32_t fade[8];
 
  
  if (recalc_colour) {
    set_width(0xA + (option_slider >> 2));
    //pulseCounter = 0 - get_width() ;
    assign_colours();
    recalc_colour = false;

    for(int m = 0, n = 7; m < 8; m++, n--){
      fade[n] = strip.Color( ((colour1_r*n)>>3) + ((colour2_r*m)>>3) , ((colour1_g*n)>>3) + ((colour2_g*m)>>3), ((colour1_b*n)>>3) + ((colour2_b*m)>>3));
    }
  }

  if(pulseCounter >= 8 + get_width() + pixelCount){  //additional 8 for the faded part
      pulseCounter = 0 - 8 - get_width() ;
      routineCounter = 0;
      partyState++;
      direction = !direction;
      pause_time = 3;
   }

 // j = 0;
  for(int i=0; i<pixelCount; i++){
    if((i >= pulseCounter + 8) && (i < pulseCounter + 8 + get_width())){  //within bulk of the comet
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, get_global_colour(1) ); // this one
    }else if((i >= pulseCounter) && (i < pulseCounter + 8)){ //within comet tail
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, fade[i-pulseCounter] ); // this one
    }else{ //outside of comet, use second colour
      holder.pixel_set((direction) ?  pixelCount - i - 1 : i, get_global_colour(2) ); // this one
    }
  }

}

/*------------------------------------------------------------------------*/
void apply_mirror(){
  int midpoint = pixelCount >> 1;
  for(int f = 0; f < midpoint; f++) //just take half of the pixels and set them mirrored in the middle
  {
    strip.setPixelColor(midpoint + f, holder.pixel_get(f));  
    strip.setPixelColor(midpoint - f - 1, holder.pixel_get(f));  
  }
}

void apply_theatre(){
  //will loop through once before returning
  uint8_t time_del = (latest_slider >> 1) + 25;
  if(theatre_counter > 2) theatre_counter = 0;
  for(int f = 0; f < pixelCount; f+=3) strip.setPixelColor(f+theatre_counter,holder.pixel_get(f+theatre_counter)); // this one
  strip.show();
  delay(time_del);
  for(int f = 0; f < pixelCount; f+=3) strip.setPixelColor(f+theatre_counter,0);
  theatre_counter++;
}

void apply_strobe(){
  uint16_t time_off = (1 + (latest_slider >> 3))  * STROBE_OFF_T; //0->100 becomes 1->13, * OFF time
  for(int f = 0; f < pixelCount; f++) strip.setPixelColor(f,holder.pixel_get(f)); // this one
  strip.show();
  delay(STROBE_ON_T);
  strip.fill(0,0,0); //clear all pixels
  strip.show();
  delay(time_off);

}

void apply_normal(){
  for(int f = 0; f < pixelCount; f++){
    strip.setPixelColor(f,holder.pixel_get(f));
  }
}

void apply_mode(uint8_t check,int del){
  //use check to disable features for certain modes
  //delay is the delay under normal operation
  uint16_t d = 100;
  if(del < 0) //if negative we can use the slider
  {
    d = 0 - del;
    if(option_slider > d) d = option_slider; //given delay is min delay
  }
  uint8_t thing = option_mode & check;
  switch(thing){
    case OM_THEATRE: strip.setBrightness(0xff);
                  apply_theatre();
                break;
    case OM_STROBE:  apply_strobe();
                break;
    case OM_MIRROR:  apply_mirror();
                break;
    case OM_NORMAL:
    default:    strip.setBrightness(0xff);
                apply_normal();
                //Serial.write(del);
                strip.show();
                delay(d);
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
   if(routineCounter >=256){
      routineCounter = 0;
      partyState++;
    }
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor((reverse) ? strip.numPixels() - i -q - 1 : i+q, Wheel( (i+routineCounter) % 255));    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor((reverse) ? strip.numPixels() - i -q - 1 : i+q, 0);        //turn every third pixel off
      }
    }
    routineCounter++;
  //}
}*/

void save_default(uint8_t * data, uint8_t count)
{
  uint8_t check = 0xff;
  for(int i = 0; i < count; i++)
  {
    EEPROM.write(i, data[i]);
    check ^= data[i];
  }
  EEPROM.write(count, check);
}

void restore_default()
{
   uint8_t data[10];
   uint8_t check = 0xff;
   for(int i = 0; i < 10; i++){
     data[i] = EEPROM.read(i);
     if(i != 9) check ^= data[i];
     else{
       if(check != data[i]) return; //check the sum
     }
   }

  red_p = data[1];
  green_p = data[2];
  blue_p = data[3];
  switched_off = false;
  recalc_colour = true;
  red_s = data[4];
  green_s = data[5];
  blue_s = data[6];
  option_colour = (data[7] >> 6) & 0x03;
  option_mode   = check_mode((data[7] >> 4) & 0x03);
  latest_slider = check_slider(data[0]);
  update_slider();
  currentRoutine = check_routine(data[8]); 
}

/*-------------Routine list------------------------------------------------------*/

#define ROUTINE_COUNT 15 //if more than 30 we're in trouble

routine_struct routine_list[ROUTINE_COUNT] = {
  {(void *)&colourWipe,-10,0x01,OM_ALL},
  {(void *)&colourSolid,-20,0x02,OM_ALL},
  {(void *)&rainbow,100,0x03,OM_ALL},
  {(void *)&rainbowChunks,100,0x04,OM_ALL},
  {(void *)&rainbowPulse,50,0x05,OM_ALL},
  {(void *)&rainbowComet,40,0x06,OM_ALL},
  {(void *)&colourPulse,40,0x07,OM_ALL},
  {(void *)&colourChunks,100,0x08,OM_ALL},
  {(void *)&colourFade,-10,0x09,OM_ALL},
  {(void *)&colourSlide,50,0x0A,OM_ALL},
  {(void *)&colourComet,40,0x0B,OM_ALL},
  {(void *)&routineOff,100,0x7f,OM_ALL},
  {NULL,0,0x80,0}, //send routine list
  {NULL,0,0x81,0}, //device name
  {NULL,0,0x82,0}, //save as default routine on connect
  //{NULL,0,0x81,0}; //fully shutdown
  //{NULL,0,0x82,0}; //button control etc
};

/*-------------------Bluetooth functions------------------*/

#define BUF_LENGTH 40
int check_bluetooth_name()
{
  char buf[BUF_LENGTH] = "AT+NAME";
  int avail = 0, br = 0, c = 0;
  Serial.println(buf);
  do{
    avail = Serial.available();
    if((avail > 0)&&((br+avail) <= BUF_LENGTH)) Serial.readBytes(&buf[br],avail);
    br += avail;
    if(br >= BUF_LENGTH) return -1;
    delay(5);
    c++;
  }while((avail != 0)&&(c<20));

  if(0 == strncmp(buf,"+NAME:",6))
  {
    if(0 == strncmp(&buf[6],dev_name,11)) return 0;
  }
  return 1;
}

int setup_bluetooth()
{
  if(check_bluetooth_disconnected())
  {
    if(0 == check_bluetooth_name()) return 0;
  }
  return 1;
}

boolean check_bluetooth_disconnected()
{
  return (digitalRead(BT_STATUS_PIN) == LOW);
}

boolean check_bluetooth_connected()
{
  return (digitalRead(BT_STATUS_PIN) == HIGH);
}

/*****************************************setup***************************************/


void setup() {
  // initialize digital pin 13 as an output.
  uint8_t byte_buf[3] = {0x31, 0x32, 0x33};
  pinMode(13, OUTPUT);
  pinMode(STATUS_PIN, OUTPUT);
  pinMode(BT_STATUS_PIN,INPUT); //read bluetooth status pin (1 = connected)
  digitalWrite(STATUS_PIN, HIGH );
  Serial.begin(115200);
  random_hold = random(0x1000000);
  toggle = 0;
  strip.begin();
  strip.show();
  //Serial.write(byte_buf,3);
  digitalWrite(13, HIGH );
  while(setup_bluetooth())
  {
    digitalWrite(13, LOW );
    delay(100);
    digitalWrite(13, HIGH );
    delay(900);
  }

  use_default = true; //if th ebt device connects, we will get the default data
}

//0x5a 0x03 0x00 0x55 0x66 0x77 0x88 0x99 0xaa 0x0a
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
  if(input == 1) result = OM_THEATRE;
  else if(input == 2) result = OM_STROBE;
  else if(input == 3) result = OM_MIRROR;
  else result = OM_NORMAL;
  return result;
}

uint8_t check_routine(uint8_t input)
{
  if((input & 0x80) == 0) return input;

  issued_command = input;

  return currentRoutine;
}

/* see bottom of file for data packet structure*/
void getData(int data_length){
    byte input[20]; // = {0,0,0,0};
    byte decoded[20];
    uint8_t response_length = 2;
    Serial.readBytes((char *)&input[0],data_length); // read the data

    if(check_bluetooth_disconnected()) return; //we don't care now
    
    if((input[data_length-2] != 13)&&(input[data_length-1] != 10)) return; //check it ends in CRLF

    base64_decode(decoded,input,data_length - 2);

    if('+' == decoded[0]) return;

    use_default = false;

    if(('i' == decoded[0])&&('n' == decoded[1])&&('i' == decoded[2])&&('t' == decoded[3])&&('_' == decoded[4]))
    {
      bt_device_number =  ((decoded[5] - 0x30) << 4) & 0xf0;
      bt_device_number += (decoded[6] - 0x30) & 0x0f;
    }
    else if((0x5A == decoded[0])  && (decoded[11] == 0xa5)) {            //first byte == 0x5A, last 4 bits == 0x0a
      currentRoutine = check_routine(decoded[1]);
      if(issued_command == 0)
      {
        red_p = decoded[3];
        green_p = decoded[4];
        blue_p = decoded[5];
        switched_off = false;
        recalc_colour = true;
        red_s = decoded[6];
        green_s = decoded[7];
        blue_s = decoded[8];
        option_colour = (decoded[9] >> 6) & 0x03;
        option_mode   = check_mode((decoded[9] >> 4) & 0x03);
        latest_slider = check_slider(decoded[2]);
        update_slider();
        response_length = 12;
      }
      decoded[0] ^= 0xff;
      decoded[1] ^= 0xff;
      if(issued_command == 0x80)
      {
        //response is 5A 8x <dev no.> <whatever else>
        issued_command = 0;
        decoded[2] = bt_device_number;
        get_routine_list(&decoded[3],&response_length);
        Serial.write( decoded ,response_length);
      }else if(issued_command == 0x81)
      {
        issued_command = 0;
        decoded[2] = bt_device_number;
        get_device_name(&decoded[3],&response_length);
        Serial.write( decoded ,response_length);
      }else if(issued_command == 0x82)
      {
       issued_command = 0;
       save_default(&decoded[2],9); 
      }      
    }
}

/*-------------Get Routine List-------------------*/
void get_routine_list(uint8_t * data, uint8_t * len)
{
  int r;
  for(r = 0; r < ROUTINE_COUNT; r++)
  {
    data[r] = routine_list[r].routine_code;    
  }
  *len += ROUTINE_COUNT;
}

void get_device_name(uint8_t * data, uint8_t * len)
{
  int c = 0;
  while(dev_name[c] != '\0'){
    data[c] = dev_name[c];
    c++;
  }
  *len += c;
}

/***********************************main loop*****************************************/
short delay_period = 100;
void loop() {

  if(Serial.available() == 18){ //12 byte packets for 2 colour picker becomes 16 bytes with base64 + CRLF
      getData(18);
   }else if(Serial.available() > 0){   //don't bother if there are no bytes in the buffer.
    delay(1);                       //give it another chance if mid-message
    if(Serial.available() == 18){
      getData(18);
    }else{
      byte x;
      while(Serial.available() > 0){
        x = Serial.read();            //flush the shit out
      }
    }
   }

   if(use_default && check_bluetooth_connected()){
     restore_default();
     use_default = false;
   }

   //blink LED? or flash/toggle LED everytime data is received or whatever so we know if it is crashing in this routine. Or is it getting stuck in while serial.available >0?
   //would an interrupt be better?
   if((++toggle & 0x04) == 0x04) digitalWrite(13, HIGH );
   else digitalWrite(13, LOW );

  if(issued_command != 0)
  {
    issued_command = 0;
    //do things
  }
  
  if (!switched_off)
  {
    void (*fun_ptr)() = NULL;
    delay_period = 100;
    uint8_t modes = 0;
    for(int r = 0; r < ROUTINE_COUNT; r++)
    {
      
      if(routine_list[r].routine_code == currentRoutine)
      {
        fun_ptr = routine_list[r].fun_ptr;
        delay_period = routine_list[r].delay_period;
        modes = routine_list[r].modes;
      }
    }
    if(fun_ptr != NULL)
    {
      (*fun_ptr)();
      apply_mode(modes,delay_period);
    }else{
      delay(50);
    }
    
  /*   switch(currentRoutine){
      case rainbowR:       rainbow();
                           apply_mode(OM_ALL,100);
                           break;
      case rainbowChunksR: rainbowChunks();
                           apply_mode(OM_ALL,100);
                           break;
      case rainbowPulseR:  rainbowPulse();
                           apply_mode(OM_ALL,50);
                           break;
      case rainbowCometR:  rainbowComet();
                           apply_mode(OM_ALL,40);
                           break;
      case colourCometR:   colourComet();
                           apply_mode(OM_ALL,40);
                           break;
      case colourPulseR:   colourPulse();
                           apply_mode(OM_ALL,40);
                           break;
      case colourChunksR:  colourChunks();
                           apply_mode(OM_ALL,100);
                           break;
      case colourWipeR:    colourWipe();
                           apply_mode(OM_ALL,((option_slider > 10) ? option_slider : 10));
                           break;
      case colourFadeR:    colourFade();
                           apply_mode(OM_ALL,((option_slider > 10) ? option_slider : 10));
                           break;
      case colourSlideR:   colourSlide();
                           apply_mode(OM_ALL,50);
                           break;
      case solidColourR:   colourSolid();
                           apply_mode(OM_ALL,((option_slider > 20) ? option_slider : 20));
                           break;
      case offR:           if(!switched_off){
                             switched_off = true;
                             colourSolid();
                             apply_mode(OM_NORMAL,100);
                           }
                           break;
      default:            currentRoutine = offR;
                          break;
     }*/
     //Serial.write(currentRoutine);
     if(0 == --random_counter) get_random(); //change random value
     if (pause_time == 0) pulseCounter++;
     else pause_time--;
  }
  if(switched_off) delay(delay_period);
}

/*---------------------------------------------
byte 0 - 0x5A;  
byte 1 - routine byte
byte 2 - slider value 0 -> 100
byte 3 - red1
byte 4 - green1
byte 5 - blue1
byte 6 - red2
byte 7 - green2
byte 8 - blue2
byte 9 - 0xzzwwA; zz = colour option, ww = mode option



-----------------------------------------------*/
