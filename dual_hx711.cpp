#include "dual_hx711.hpp"
#include "hardware/sync.h"

DualHX711::DualHX711(uint clock_pin0, uint clock_pin1, uint data_pin0, uint data_pin1)
    : clk0(clock_pin0), clk1(clock_pin1), dat0(data_pin0), dat1(data_pin1), current_gain(128), pulses(25) {}

void DualHX711::begin() {
    gpio_init(clk0);
    gpio_set_dir(clk0, GPIO_OUT);
    gpio_put(clk0, 0);

    gpio_init(clk1);
    gpio_set_dir(clk1, GPIO_OUT);
    gpio_put(clk1, 0);

    gpio_init(dat0);
    gpio_set_dir(dat0, GPIO_IN);
    gpio_pull_up(dat0);

    gpio_init(dat1);
    gpio_set_dir(dat1, GPIO_IN);
    gpio_pull_up(dat1);

    power_up(current_gain);
}

uint8_t DualHX711::gain_to_pulses(uint8_t gain) {
    switch (gain) {
        case 128: return 25; // Channel A, Gain 128
        case 32:  return 26; // Channel B, Gain 32
        case 64:  return 27; // Channel A, Gain 64
        default:  return 25; // Fallback
    }
}

void DualHX711::power_up(uint8_t gain) {
    gpio_put(clk0, 0);
    gpio_put(clk1, 0);
    current_gain = gain;
    pulses = gain_to_pulses(gain);
    sleep_ms(1); // Wait for HX711 to power up and settle
}

void DualHX711::power_down() {
    gpio_put(clk0, 1);
    gpio_put(clk1, 1);
    sleep_us(65); // Hold clock high for >60us to enter power down
}

bool DualHX711::is_ready() {
    // Both chips must be ready (DOUT pins low)
    return (gpio_get(dat0) == 0) && (gpio_get(dat1) == 0);
}

bool DualHX711::read(int32_t& raw_val0, int32_t& raw_val1, uint32_t timeout_ms) {
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    
    // Wait for both DOUT pins to go low (data ready)
    while (!is_ready()) {
        if (to_ms_since_boot(get_absolute_time()) - start_time > timeout_ms) {
            return false; // Timeout
        }
        sleep_us(100);
    }

    int32_t raw0 = 0;
    int32_t raw1 = 0;

    // Disable interrupts during timing-sensitive clock generation to prevent the 
    // clock high state from being stretched beyond 50us (which triggers power down).
    uint32_t flags = save_and_disable_interrupts();

    for (int i = 0; i < 24; ++i) {
        gpio_put(clk0, 1);
        gpio_put(clk1, 1);
        sleep_us(1); // Keep high for 1us (HX711 needs min 0.2us, max 50us)

        // Read data bits
        raw0 = (raw0 << 1) | gpio_get(dat0);
        raw1 = (raw1 << 1) | gpio_get(dat1);

        gpio_put(clk0, 0);
        gpio_put(clk1, 0);
        sleep_us(1); // Keep low for 1us (HX711 needs min 0.2us)
    }

    // Set gain for next reading by sending additional clock pulses
    for (int i = 24; i < pulses; ++i) {
        gpio_put(clk0, 1);
        gpio_put(clk1, 1);
        sleep_us(1);
        gpio_put(clk0, 0);
        gpio_put(clk1, 0);
        sleep_us(1);
    }

    restore_interrupts(flags);

    // HX711 outputs 24-bit values. We need to sign-extend to 32-bit.
    if (raw0 & 0x800000) {
        raw0 |= 0xFF000000;
    }
    if (raw1 & 0x800000) {
        raw1 |= 0xFF000000;
    }

    raw_val0 = raw0;
    raw_val1 = raw1;
    return true;
}
