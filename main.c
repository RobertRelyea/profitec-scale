#include <stdio.h>
#include "pico/stdlib.h"

#include "extern/pico-scale/include/hx711_scale_adaptor.h"
#include "extern/pico-scale/include/scale.h"


int main() {
    setup_default_uart();
    printf("Hello, world!\n");
    
    printf("Configuring HX711\n");
    // Set up single HX711 sensor

    hx711_config_t hxcfg;
    hx711_get_default_config(&hxcfg);

    hxcfg.clock_pin = 14;
    hxcfg.data_pin = 15;

    hx711_t hx;

    printf("Initializing HX711\n");
    hx711_init(&hx, &hxcfg);
    printf("Powering up HX711\n");
    hx711_power_up(&hx, hx711_gain_128);

    printf("Settling HX711\n");
    hx711_wait_settle(hx711_rate_80);

    int32_t val = 0;

    printf("Reading from HX711\n");
    while(true)
    {
      if(hx711_get_value_noblock(&hx, &val))
	{
	  printf("value: %li\n", val);
	}
    }
        
    return 0;
}



