#include <pico/stdlib.h>
#include "hardware/adc.h"
#include "battery_monitor.h"



void battery_monitor_init(){
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(2);
}

void get_battery_percentage(uint8_t *percentage){
        const float vmin =   2.25;
        const float vmax =   3.0;
        const float conversion_factor = 3.3f / (1 << 12);

        uint16_t voltage = adc_read()+ADC_OFFSET;
        *percentage = (((voltage*conversion_factor) - vmin) / (vmax - vmin)) * 100;

    }

void get_battery_voltage(float *fvoltage){
        const float vmin =   2.25;
        const float vmax =   3.0;
        const float conversion_factor = 3.3f / (1 << 12);

        uint16_t voltage = adc_read()+ADC_OFFSET;
        *fvoltage = voltage*conversion_factor;

    }
