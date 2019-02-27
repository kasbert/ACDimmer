
#ifndef ACDimmer_h
#define ACDimmer_h
#include "Arduino.h"

/*
 * ACDimmer uses Timer1 and Timer2
 * Pin 8 and any other
 */

class ACDimmer {
  public:
    // interruptPin = 8
    ACDimmer(uint8_t cmdPin = 2);
    void begin();
    uint8_t poll();

#define DIM_OFF 0
#define DIM_MAX 100
#define DIM_FORCE_ON 101

    void setDimming(uint8_t level);
    void setOn(); // Force on, no zero detect
    void setOff(); // Force off
    uint8_t isEdgeDetected();

    uint32_t getSec(); // 50Hz assumed
    uint8_t getPulseWidth(); // If > 0 , we have sync
};

#endif
