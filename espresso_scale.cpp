#include "espresso_scale.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "hardware/flash.h"
#include "hardware/sync.h"

// Reserved sector near the end of a standard 2MB flash space
#define FLASH_TARGET_OFFSET (2048 * 1024 - 4096)

struct ScaleConfig {
    uint32_t magic; // 0x5CADE002
    double scale_factor0;
    double scale_factor1;
    int32_t offset0;
    int32_t offset1;
};


// Helper to calculate absolute time differences in seconds
double elapsed_sec(absolute_time_t start, absolute_time_t end) {
    return (double)absolute_time_diff_us(start, end) / 1000000.0;
}

EspressoScale::EspressoScale(uint clock_pin0, uint clock_pin1, uint data_pin0, uint data_pin1)
    : hx711(clock_pin0, clock_pin1, data_pin0, data_pin1),
      offset0(14450), offset1(56000), // Default values from main.cpp
      scale_factor0(1073.93), scale_factor1(1113.00),
      filtered_weight0(0.0), filtered_weight1(0.0), filtered_weight_total(0.0),
      history_idx(0), prev_weight(0.0), current_flow_rate(0.0),
      timer_running(false), brew_assist_enabled(true), brew_state(BREW_STATE_IDLE) {
    
    for (int i = 0; i < 3; ++i) {
        raw0_history[i] = 0;
        raw1_history[i] = 0;
    }
    prev_flow_time = get_absolute_time();
    timer_start_time = get_absolute_time();
    timer_stop_time = get_absolute_time();
    state_timer = get_absolute_time();
}

void EspressoScale::begin() {
    hx711.begin();
    load_calibration();
    tare(10);
    prev_weight = get_weight();
    prev_flow_time = get_absolute_time();
}

void EspressoScale::tare(int samples) {
    int32_t sum0 = 0;
    int32_t sum1 = 0;
    int count = 0;
    int32_t r0, r1;

    // Flush a few readings first
    for (int i = 0; i < 3; ++i) {
        hx711.read(r0, r1, 150);
        sleep_ms(10);
    }

    // Accumulate samples
    for (int i = 0; i < samples || count < samples / 2;) {
        if (hx711.read(r0, r1, 150)) {
            sum0 += r0;
            sum1 += r1;
            count++;
        }
        sleep_ms(10);
        i++;
        if (i > samples * 2) break; // Avoid infinite loop if disconnected
    }

    if (count > 0) {
        offset0 = sum0 / count;
        offset1 = sum1 / count;
    }

    // Reset filtering buffers
    for (int i = 0; i < 3; ++i) {
        raw0_history[i] = offset0;
        raw1_history[i] = offset1;
    }
    history_idx = 0;

    filtered_weight0 = 0.0;
    filtered_weight1 = 0.0;
    filtered_weight_total = 0.0;
    current_flow_rate = 0.0;
    prev_weight = 0.0;
    prev_flow_time = get_absolute_time();
}

void EspressoScale::set_calibration(double factor0, double factor1) {
    scale_factor0 = factor0;
    scale_factor1 = factor1;
}

void EspressoScale::add_raw_sample(int32_t r0, int32_t r1) {
    raw0_history[history_idx] = r0;
    raw1_history[history_idx] = r1;
    history_idx = (history_idx + 1) % 3;
}

int32_t EspressoScale::get_median(int32_t* buf) {
    int32_t temp[3] = {buf[0], buf[1], buf[2]};
    std::sort(temp, temp + 3);
    return temp[1];
}

double EspressoScale::get_cg() {
    double raw_weight0 = (raw0_history[0] - offset0) / scale_factor0;
    double raw_weight1 = (raw1_history[0] - offset1) / scale_factor1;

    double total_weight = raw_weight0 + raw_weight1;
    if (total_weight > 10.0)
        return raw_weight1 / total_weight;
    return 0.5;
}

bool EspressoScale::update() {
    int32_t r0, r1;
    if (!hx711.read(r0, r1, 150)) {
        return false; // Failed to read from sensors
    }

    add_raw_sample(r0, r1);

    int32_t median0 = get_median(raw0_history);
    int32_t median1 = get_median(raw1_history);

    // Calculate physical weights (subtracted by zero offset, divided by scale factor)
    double w0 = (double)(median0 - offset0) / scale_factor0;
    double w1 = (double)(median1 - offset1) / scale_factor1;
    double total_w = w0 + w1;

    // Apply exponential moving average (EMA) filter to weights
    // Alpha of 0.3 matches high responsiveness with low noise
    filtered_weight0 = 0.3 * w0 + 0.7 * filtered_weight0;
    filtered_weight1 = 0.3 * w1 + 0.7 * filtered_weight1;
    filtered_weight_total = 0.3 * total_w + 0.7 * filtered_weight_total;

    // Calculate flow rate (g/s)
    absolute_time_t now = get_absolute_time();
    double dt = elapsed_sec(prev_flow_time, now);

    if (dt >= 0.1) { // Update flow rate every 100ms
        double delta_w = filtered_weight_total - prev_weight;
        double raw_flow = delta_w / dt;

        // Apply EMA filter to flow rate to smooth out reading fluctuations
        current_flow_rate = 0.15 * raw_flow + 0.85 * current_flow_rate;
        if (current_flow_rate < 0.05 && current_flow_rate > -0.05) {
            current_flow_rate = 0.0; // Clean display threshold
        }

        prev_weight = filtered_weight_total;
        prev_flow_time = now;
    }

    // Run Brew Assist state machine if enabled
    if (brew_assist_enabled) {
        update_brew_assist();
    }

    return true;
}

uint32_t EspressoScale::get_brew_time_s() const {
    if (!timer_running) {
        return (uint32_t)(absolute_time_diff_us(timer_start_time, timer_stop_time) / 1000000);
    }
    return (uint32_t)(absolute_time_diff_us(timer_start_time, get_absolute_time()) / 1000000);
}

void EspressoScale::start_timer() {
    if (!timer_running) {
        timer_start_time = get_absolute_time();
        timer_running = true;
    }
}

void EspressoScale::stop_timer() {
    if (timer_running) {
        timer_stop_time = get_absolute_time();
        timer_running = false;
    }
}

void EspressoScale::reset_timer() {
    timer_running = false;
    timer_start_time = get_absolute_time();
    timer_stop_time = timer_start_time;
}

void EspressoScale::update_brew_assist() {
    absolute_time_t now = get_absolute_time();
    double current_w = filtered_weight_total;

    switch (brew_state) {
        case BREW_STATE_IDLE:
            // Detect cup placed (weight increases by more than 10 grams)
            if (current_w > 10.0) {
                brew_state = BREW_STATE_CUP_DETECTED;
                state_timer = now;
                baseline_weight = current_w;
            }
            break;

        case BREW_STATE_CUP_DETECTED:
            // Check if weight is stable (varies by less than 0.3g over 1.2s)
            if (std::abs(current_w - baseline_weight) > 0.4) {
                // Not stable yet, reset baseline
                baseline_weight = current_w;
                state_timer = now;
            } else if (elapsed_sec(state_timer, now) >= 1.2) {
                // Stable for 1.2s, auto-tare
                tare(8);
                brew_state = BREW_STATE_TARED;
                reset_timer();
                state_timer = now;
            }

            // Cup removed before taring
            if (current_w < 5.0) {
                brew_state = BREW_STATE_IDLE;
            }
            break;

        case BREW_STATE_TARED:
            // Waiting for flow (weight increases by > 0.4g relative to tared zero)
            if (current_w > 0.4) {
                start_timer();
                brew_state = BREW_STATE_BREWING;
                state_timer = now;
            }

            // If cup is removed (weight goes significantly negative)
            if (current_w < -10.0) {
                brew_state = BREW_STATE_IDLE;
                tare(5);
            }
            break;

        case BREW_STATE_BREWING:
            // Brewing in progress
            // Detect end of brew (flow rate drops close to 0 for 2.5 seconds)
            if (current_flow_rate >= 0.15) {
                state_timer = now; // Flowing, reset timeout
            } else if (elapsed_sec(state_timer, now) >= 2.5 && current_w > 5.0) {
                stop_timer();
                brew_state = BREW_STATE_FINISHED;
            }

            // If cup is removed suddenly
            if (current_w < -10.0) {
                stop_timer();
                brew_state = BREW_STATE_IDLE;
                tare(5);
            }
            break;

        case BREW_STATE_FINISHED:
            // Show result. Reset if cup is removed (weight drops close to 0)
            // Note: Since we tared with the cup, removing the cup makes weight negative (approx -baseline_weight).
            // So if weight is significantly negative, it means the cup was removed.
            if (current_w < -5.0) {
                reset_timer();
                brew_state = BREW_STATE_IDLE;
                tare(5);
            }
            break;
    }
}

const char* EspressoScale::get_brew_state_string() const {
    switch (brew_state) {
        case BREW_STATE_IDLE:          return "IDLE";
        case BREW_STATE_CUP_DETECTED:  return "DETECT";
        case BREW_STATE_TARED:         return "READY";
        case BREW_STATE_BREWING:       return "FLOW";
        case BREW_STATE_FINISHED:      return "DONE";
        default:                       return "UNKNOWN";
    }
}

bool EspressoScale::calibrate_sensors(double reference_weight_g, const int32_t* raw0_readings, const int32_t* raw1_readings, int num_readings, double& new_factor0, double& new_factor1) {
    if (num_readings < 2) {
        return false; // Least squares needs at least 2 points
    }

    double S_AA = 0.0;
    double S_AB = 0.0;
    double S_BB = 0.0;
    double S_AW = 0.0;
    double S_BW = 0.0;

    for (int i = 0; i < num_readings; ++i) {
        double A = (double)(raw0_readings[i] - offset0);
        double B = (double)(raw1_readings[i] - offset1);
        
        S_AA += A * A;
        S_AB += A * B;
        S_BB += B * B;
        S_AW += A * reference_weight_g;
        S_BW += B * reference_weight_g;
    }

    // Solve the 2x2 system of normal equations:
    // S_AA * x + S_AB * y = S_AW
    // S_AB * x + S_BB * y = S_BW
    // where x = 1/scale_factor0 and y = 1/scale_factor1
    
    double D = S_AA * S_BB - S_AB * S_AB;
    
    // Check if the system is collinear (user didn't shift the weight enough)
    // The determinant D will be close to 0 if the ratios A_i / B_i are nearly constant.
    if (D < 1e-7 * S_AA * S_BB) {
        return false; 
    }

    double x = (S_AW * S_BB - S_BW * S_AB) / D;
    double y = (S_AA * S_BW - S_AW * S_AB) / D;

    // Scale factors must be positive and non-zero
    if (x <= 0.0 || y <= 0.0) {
        return false;
    }

    scale_factor0 = 1.0 / x;
    scale_factor1 = 1.0 / y;

    save_calibration();

    new_factor0 = scale_factor0;
    new_factor1 = scale_factor1;
    return true;
}

bool EspressoScale::get_raw_sample(int32_t& r0, int32_t& r1) {
    return hx711.read(r0, r1, 150);
}

void EspressoScale::load_calibration() {
    const ScaleConfig* config = (const ScaleConfig*)(XIP_BASE + FLASH_TARGET_OFFSET);
    if (config->magic == 0x5CADE002) {
        scale_factor0 = config->scale_factor0;
        scale_factor1 = config->scale_factor1;
        offset0 = config->offset0;
        offset1 = config->offset1;
        printf("Loaded calibration from Flash: SF0=%.2f, SF1=%.2f, Off0=%ld, Off1=%ld\n",
               scale_factor0, scale_factor1, (long)offset0, (long)offset1);
    } else {
        printf("No valid calibration found in Flash (magic: 0x%08lX). Using defaults.\n", (long unsigned int)config->magic);
    }
}

void EspressoScale::save_calibration() {
    ScaleConfig config;
    config.magic = 0x5CADE002;
    config.scale_factor0 = scale_factor0;
    config.scale_factor1 = scale_factor1;
    config.offset0 = offset0;
    config.offset1 = offset1;

    uint8_t buf[FLASH_PAGE_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &config, sizeof(config));

    printf("Saving calibration to Flash: SF0=%.2f, SF1=%.2f, Off0=%ld, Off1=%ld... ",
           scale_factor0, scale_factor1, (long)offset0, (long)offset1);

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, buf, FLASH_PAGE_SIZE);
    restore_interrupts(ints);

    printf("DONE.\n");
}


