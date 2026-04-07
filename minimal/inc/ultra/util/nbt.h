// Timer struct definition
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>

typedef struct {
    uint64_t target_time_us;
    uint32_t duration_ms;
    uint64_t last_fire_time;

} non_blocking_timer;


// Function to initialize the non-blocking timer
void start_timer(non_blocking_timer *timer, uint32_t duration_milliseconds) {
    timer->duration_ms = duration_milliseconds;
    timer->target_time_us = time_us_64() + (uint64_t)duration_milliseconds * 1000;
}




// Function to check if the non-blocking timer has expired
int timer_expired(non_blocking_timer *timer) {
    uint64_t current_time_us = time_us_64();
    if (current_time_us >= timer->target_time_us) {
        return 1; // Timer expired
    }
    return 0; // Timer not expired
}

