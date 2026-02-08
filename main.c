#include <stdio.h>
#include "pico/stdlib.h"

#include "extern/pico-scale/include/hx711_scale_adaptor.h"
#include "extern/pico-scale/include/scale.h"


int main() {
    setup_default_uart();

    // Set up multiple HX711 sensors
    printf("Configuring HX711\n");

    // 1. Set configuration
    hx711_multi_config_t hxmcfg;
    hx711_multi_get_default_config(&hxmcfg);
    hxmcfg.clock_pin = 9; //GPIO pin connected to each HX711 chip
    hxmcfg.data_pin_base = 14; //first GPIO pin used to connect to HX711 data pin
    hxmcfg.chips_len = 2; //how many HX711 chips connected

    //by default, the underlying PIO programs will run on pio0
    //if you need to change this, you can do:
    //hxmcfg.pio = pio1;

    // 2. Initialise
    hx711_multi_t hxm;
    hx711_multi_init(&hxm, &hxmcfg);

    // 3. Power up the HX711 chips and set gain on each chip
    hx711_multi_power_up(&hxm, hx711_gain_128);

    // 4. This step is optional. Only do this if you want to
    // change the gain AND save it to each HX711 chip
    //
    // hx711_multi_set_gain(&hxm, hx711_gain_64);
    // hx711_multi_power_down(&hxm);
    // hx711_wait_power_down();
    // hx711_multi_power_up(&hxm, hx711_gain_64);

    // 5. Wait for readings to settle
    hx711_wait_settle(hx711_gain_128);

    // 6. Read values
    int32_t arr[hxmcfg.chips_len];


    while(true)
    {
      // 6a. wait (block) until values are ready
      hx711_multi_get_values(&hxm, arr);

      // then print the value from each chip
      // the first value in the array is from the HX711
      // connected to the first configured data pin and
      // so on
      for(uint i = 0; i < hxmcfg.chips_len; ++i) {
	printf("hx711_multi_t chip %i: %li\n", i, arr[i]);
      }
    }

        
    return 0;
}



