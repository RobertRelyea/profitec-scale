#pragma once

#include "dual_hx711.hpp"
#include "pico/time.h"

class EspressoScale {
public:
    /**
     * @brief Constructor for EspressoScale.
     * @param clock_pin0 SCK pin for left load cell.
     * @param clock_pin1 SCK pin for right load cell.
     * @param data_pin0 DOUT pin for left load cell.
     * @param data_pin1 DOUT pin for right load cell.
     */
    EspressoScale(uint clock_pin0, uint clock_pin1, uint data_pin0, uint data_pin1);

    /**
     * @brief Initializes the scale and performs an initial tare.
     */
    void begin();

    /**
     * @brief Performs a tare operation to zero the scale (calculates offsets).
     * @param samples Number of samples to average for the offset.
     */
    void tare(int samples = 10);

    /**
     * @brief Sets the calibration factors (scale factors) for the two sensors.
     * @param factor0 Scale factor for sensor 0 (raw_units per gram).
     * @param factor1 Scale factor for sensor 1 (raw_units per gram).
     */
    void set_calibration(double factor0, double factor1);

    /**
     * @brief Reads from sensors and updates internal states (weight, flow, timer).
     * Should be called continuously in the main loop.
     * @return true if new data was successfully read and processed.
     */
    bool update();

    // Getters for weights
    double get_weight() const { return filtered_weight_total; }
    double get_weight0() const { return filtered_weight0; }
    double get_weight1() const { return filtered_weight1; }

    /**
     * @brief Returns center of gravity currently seen on the scale.
     * Will return 0.0 if a total weight less than 10g is observed.
     * @return Center of gravity between [0, 1]
     */
    double get_cg();

    // Getters for flow rate & timer
    double get_flow_rate() const { return current_flow_rate; }
    uint32_t get_brew_time_s() const;
    bool is_timer_running() const { return timer_running; }

    // Manual Timer Controls
    void start_timer();
    void stop_timer();
    void reset_timer();

    // Auto Brew Assist Settings
    void enable_brew_assist(bool enable) { brew_assist_enabled = enable; }
    bool is_brew_assist_enabled() const { return brew_assist_enabled; }

    /**
     * @brief Calibrates scale factors using multiple readings of a known reference weight
     * placed at different positions. Assumes tare has already been performed.
     * @param reference_weight_g Reference weight in grams.
     * @param raw0_readings Array of raw readings from sensor 0.
     * @param raw1_readings Array of raw readings from sensor 1.
     * @param num_readings Number of readings in the arrays.
     * @param new_factor0 Output parameter for the new scale factor 0.
     * @param new_factor1 Output parameter for the new scale factor 1.
     * @return true if calibration succeeded.
     */
    bool calibrate_sensors(double reference_weight_g, const int32_t* raw0_readings, const int32_t* raw1_readings, int num_readings, double& new_factor0, double& new_factor1);
    
    /**
     * @brief Reads a raw sample from both load cell sensors.
     * @param r0 Output parameter for sensor 0 raw reading.
     * @param r1 Output parameter for sensor 1 raw reading.
     * @return true if reading succeeded.
     */
    bool get_raw_sample(int32_t& r0, int32_t& r1);
    
    enum BrewState {
        BREW_STATE_IDLE,
        BREW_STATE_CUP_DETECTED,
        BREW_STATE_TARED,
        BREW_STATE_BREWING,
        BREW_STATE_FINISHED
    };

    BrewState get_brew_state() const { return brew_state; }
    const char* get_brew_state_string() const;

private:
    DualHX711 hx711;

    // Calibration data
    int32_t offset0;
    int32_t offset1;
    double scale_factor0;
    double scale_factor1;

    // Weight states
    double filtered_weight0;
    double filtered_weight1;
    double filtered_weight_total;

    // Noise filtering buffers (3-sample median filter)
    int32_t raw0_history[3];
    int32_t raw1_history[3];
    int history_idx;

    // Flow rate calculation states
    double prev_weight;
    absolute_time_t prev_flow_time;
    double current_flow_rate; // grams per second

    // Shot timer states
    absolute_time_t timer_start_time;
    absolute_time_t timer_stop_time;
    bool timer_running;

    // Auto Brew Assist states
    bool brew_assist_enabled;
    BrewState brew_state;
    absolute_time_t state_timer;
    double baseline_weight;

    void add_raw_sample(int32_t r0, int32_t r1);
    int32_t get_median(int32_t* buf);
    void update_brew_assist();
    void load_calibration();
    void save_calibration();
};
