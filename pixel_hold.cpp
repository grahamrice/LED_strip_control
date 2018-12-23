#include "pixel_hold.h"
#include <stdlib.h>

pixel_hold::pixel_hold(void){
 //don't need to do anything
 pixel_values = (uint32_t *)malloc(pixelCount);
}

uint32_t pixel_hold::pixel_get(uint8_t p){
  return pixel_values[p];
}

void pixel_hold::pixel_set(uint8_t p, uint32_t colour){
  pixel_values[p] = colour;
}

void pixel_hold::pixel_setall(uint32_t colour){
  for(int q = 0; q < pixelCount; q++) pixel_values[q] = colour;
}
