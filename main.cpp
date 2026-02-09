#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"

#include "extern/pico-scale/include/scale.h"

#include "extern/displaylib/include/displaylib/ssd1306.hpp"


#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define SCREEN_SIZE (OLED_WIDTH * (OLED_HEIGHT) / 8)
#define OLED_CLK_PIN 21
#define OLED_DATA_PIN 20
uint8_t screen_buffer[SCREEN_SIZE];

const uint16_t I2C_Speed = 100;
const uint8_t I2C_Address = 0x3C;

SSD1306 myOLED(OLED_WIDTH, OLED_HEIGHT);

void SetupOLED()
{
  busy_wait_ms(500);

  myOLED.OLEDbegin(I2C_Address,i2c1,  I2C_Speed, OLED_DATA_PIN, OLED_CLK_PIN);
  busy_wait_ms(500);
  myOLED.OLEDSetBufferPtr(OLED_WIDTH, OLED_HEIGHT, screen_buffer);
  busy_wait_ms(500);
  myOLED.OLEDFillScreen(0xF0, 0);
  busy_wait_ms(500);
}



int main() {
    setup_default_uart();
    stdio_init_all();

    SetupOLED();

    myOLED.OLEDclearBuffer(); 
    myOLED.setFont(pFontDefault);
    myOLED.setCursor(5,5);
    myOLED.print("Hello World!");
    myOLED.OLEDupdate();  

    printf("Configuring Scale\n");

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
    hx711_wait_settle(hx711_rate_80);
    

    // 6. Read values
    int32_t arr[hxmcfg.chips_len];
    double arr_mass[hxmcfg.chips_len];

    int32_t zero_refs[2] = {14450, 56000};
    // Sensor readings for reference weight
    int32_t scale_refs[2] = {464000, 521900};
    double ref_weight = 418.6;


    while(true)
    {
      // 6a. wait (block) until values are ready
      hx711_multi_get_values(&hxm, arr);      

      // then print the value from each chip
      // the first value in the array is from the HX711
      // connected to the first configured data pin and
      // so on
      for(uint i = 0; i < hxmcfg.chips_len; ++i) {
	arr_mass[i] = (double)(arr[i] - zero_refs[i]) / (double)(scale_refs[i] - zero_refs[i]) * ref_weight;

	/* printf("hx711_multi_t chip %i: %li\n", i, arr[i]); */
      }

      printf("Weight 0: %f, Weight 1: %f, Avg: %f\n", arr_mass[0], arr_mass[1],
	     (arr_mass[0] + arr_mass[1]) / 2.0);
    }
}



