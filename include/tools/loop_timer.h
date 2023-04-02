#include "Arduino.h"

// overall loop freq. of the main loop, use this in all time dependent modules
#define FREQ_LOOP_CYCLE_HZ            20

extern uint32_t loop_timer;
extern uint64_t t_0;

/// \brief calculates loop frequency, only correct when 
///        loop_timer is updated every loop iteration
/// \return loop frequency
///
float loop_timer_get_loop_freq();

/// \brief measures loop cycle time and adds delay 
///        if necessary to meet predefined FREQ_LOOP_CYCLE_HZ
///
void loop_timer_check_cycle_freq();