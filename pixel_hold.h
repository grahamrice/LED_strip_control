#ifndef PIXEL_HOLD
#define PIXEL_HOLD

#ifndef pixelCount
#define pixelCount 96
#endif

#include "stdint.h"

class pixel_hold {
  private:
    uint32_t pixel_values[pixelCount];  //write class to hold the pixel current value in the same way the strip does when you cba
  public:
    pixel_hold();
    uint32_t pixel_get(uint8_t p);
    void pixel_set(uint8_t p, uint32_t colour);
    void pixel_setall(uint32_t colour);
};

#endif
