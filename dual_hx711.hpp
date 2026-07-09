#pragma once

#include <cstdint>
#include "pico/stdlib.h"

/**
 * @brief Driver for two HX711 load cell amplifiers.
 */
class DualHX711 {
public:
    /**
     * @brief Constructor for DualHX711.
     * @param clock_pin0 The SCK pin for the first HX711 chip.
     * @param clock_pin1 The SCK pin for the second HX711 chip.
     * @param data_pin0 The DOUT pin for the first HX711 chip.
     * @param data_pin1 The DOUT pin for the second HX711 chip.
     */
    DualHX711(uint clock_pin0, uint clock_pin1, uint data_pin0, uint data_pin1);

    /**
     * @brief Initializes the GPIO pins and puts the scale into default gain (128).
     */
    void begin();

    /**
     * @brief Powers up both HX711 chips and configures the gain.
     * @param gain The gain to set (128, 64, or 32). Default is 128 (Channel A).
     */
    void power_up(uint8_t gain = 128);

    /**
     * @brief Powers down both HX711 chips (SCK high).
     */
    void power_down();

    /**
     * @brief Checks if both HX711 chips have finished conversions (both DOUT low).
     * @return true if data is ready to read on both chips.
     */
    bool is_ready();

    /**
     * @brief Reads raw 24-bit sign-extended values from both chips.
     * @param raw_val0 Output parameter for the first chip's raw value.
     * @param raw_val1 Output parameter for the second chip's raw value.
     * @param timeout_ms Timeout duration in milliseconds.
     * @return true if reading succeeded, false on timeout.
     */
    bool read(int32_t& raw_val0, int32_t& raw_val1, uint32_t timeout_ms = 150);

private:
    uint clk0;
    uint clk1;
    uint dat0;
    uint dat1;

    uint8_t current_gain;
    uint8_t pulses; // Number of pulses to send (25 for 128 gain, 26 for 32 gain, 27 for 64 gain)

    uint8_t gain_to_pulses(uint8_t gain);
};
