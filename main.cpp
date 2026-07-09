#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "espresso_scale.hpp"
#include "extern/displaylib/include/displaylib/ssd1306.hpp"

#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define SCREEN_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)
#define OLED_CLK_PIN 21
#define OLED_DATA_PIN 20

#if PICO_RP2350
#define OLED_I2C_INST i2c0
#else
#define OLED_I2C_INST i2c1
#endif

#define PRESS_THRESHOLD 200.0

uint8_t screen_buffer[SCREEN_SIZE];

const uint16_t I2C_Speed = 400; // 400kHz Fast Mode for smoother display updates
const uint8_t I2C_Address = 0x3C;

SSD1306 myOLED(OLED_WIDTH, OLED_HEIGHT);
// Espresso scale instance: Clock 0 on GP9, Clock 1 on GP10, Data 0 on GP14, Data 1 on GP15
EspressoScale scale(9, 10, 14, 15);

void SetupOLED() {
    busy_wait_ms(200);
    myOLED.OLEDbegin(I2C_Address, OLED_I2C_INST, I2C_Speed, OLED_DATA_PIN, OLED_CLK_PIN);
    busy_wait_ms(100);
    myOLED.OLEDSetBufferPtr(OLED_WIDTH, OLED_HEIGHT, screen_buffer);
    myOLED.OLEDclearBuffer();
    myOLED.OLEDupdate();
}

void DrawSplashScreen() {
    myOLED.OLEDclearBuffer();
    
    // Draw a sleek frame
    myOLED.drawRect(0, 0, 128, 32, 1);
    myOLED.drawRect(2, 2, 124, 28, 1);
    
    // Title
    myOLED.setFont(pFontArialBold);
    myOLED.setCursor(20, 8);
    myOLED.print("Robert");
    
    myOLED.setFont(pFontPico);
    myOLED.setCursor(85, 18);
    myOLED.print("v1.0");

    myOLED.OLEDupdate();
    busy_wait_ms(1500);

    // Show taring state
    myOLED.OLEDclearBuffer();
    myOLED.drawRect(0, 0, 128, 32, 1);
    myOLED.setFont(pFontDefault);
    myOLED.setCursor(15, 12);
    myOLED.print("Calibrating...");
    myOLED.OLEDupdate();
}

void UpdateDisplay() {
  //myOLED.OLEDclearBuffer();

    // 1. Draw a vertical divider between the info panel and the weight
    //myOLED.drawFastVLine(54, 0, 32, 1);

    // 2. Draw the Shot Timer (Left Panel, Top Row)
    // Clock Icon
    myOLED.drawCircle(6, 6, 4, 1);
    myOLED.drawLine(6, 6, 6, 4, 1);
    myOLED.drawLine(6, 6, 8, 6, 1);
    
    char time_str[16];
    uint32_t total_s = scale.get_brew_time_s();
    uint32_t min = total_s / 60;
    uint32_t sec = total_s % 60;
    sprintf(time_str, "%d:%02d", (int)min, (int)sec);
    
    myOLED.setFont(pFontDefault);
    myOLED.setCursor(16, 2);
    myOLED.print(time_str);

    // 3. Draw the Flow Rate (Left Panel, Middle Row)
    // Water Drop Icon
    myOLED.drawTriangle(6, 12, 3, 16, 9, 16, 1);
    myOLED.fillCircle(6, 16, 2, 1);
    
    char flow_str[16];
    sprintf(flow_str, "%.1f g/s", scale.get_flow_rate());
    myOLED.setFont(pFontDefault);
    myOLED.setCursor(16, 12);
    myOLED.print(flow_str);

    // 4. Draw the Brew Assist State (Left Panel, Bottom Row)
    if (scale.is_brew_assist_enabled()) {
      const char* state_str = scale.get_brew_state_string();
      // Centered inside the left-bottom panel
      myOLED.setFont(pFontDefault);
      myOLED.setCursor(6, 23);
      myOLED.print("[");
      myOLED.print((char*)state_str);
      myOLED.print("]");
    }

    // 5. Draw the Weight Unit (Small 'g' at the bottom right)
    myOLED.setFont(pFontDefault);
    myOLED.setCursor(120, 22);
    myOLED.print("g");

    // 6. Draw the Weight (Right Panel, Large Font)
    char weight_str[16];
    double current_w = scale.get_weight();
    
    // Format: show 1 decimal place. Under 0, format -0.0 as 0.0 to avoid annoying jitter.
    if (current_w > -0.05 && current_w < 0.05) {
        current_w = 0.0;
    }
    sprintf(weight_str, "%.1f", current_w);
    
    int len = strlen(weight_str);
    int char_width = 16; // pFontArialRound character width
    
    // Right-align relative to x=118 (leaving room for the 'g')
    int x_start = 118 - (len * char_width);
    if (x_start < 56) {
        x_start = 56; // Prevent overflowing into the divider line
    }
    
    myOLED.setFont(pFontArialRound);
    myOLED.setCursor(x_start, 4);
    myOLED.print(weight_str);

    //myOLED.OLEDupdate();
}

static double elapsed_sec(absolute_time_t start, absolute_time_t end) {
    return (double)absolute_time_diff_us(start, end) / 1000000.0;
}

/* Displays the current center of gravity on the OLED display
 *
 */
int draw_cursor() {

  double cg = scale.get_cg();
  double weight = scale.get_weight();
  
  int cg_line_position = std::max(int(cg * OLED_WIDTH), 0);
  myOLED.drawFastVLine(cg_line_position, 0, 32, 1);

  myOLED.drawTriangle(cg_line_position - 2, 0,
		      cg_line_position + 2, 0,
		      cg_line_position, 4, 1);


  return cg_line_position;
}

size_t DisplayMenuLoop(std::string prompt, std::vector<std::string> options, double inactivity_timeout) {
    // Loop while we have activity
    absolute_time_t inactivity_start = get_absolute_time();

    while (elapsed_sec(inactivity_start, get_absolute_time()) < inactivity_timeout) {
      	// Clear screen
	myOLED.OLEDclearBuffer();
	
        // Read sensor values and update scale logic
        bool success = scale.update();
        double timeout_progress = elapsed_sec(inactivity_start, get_absolute_time()) / inactivity_timeout;

	// Draw prompt
	myOLED.setFont(pFontDefault);
	myOLED.setCursor(0, 10);
	myOLED.print(prompt);
	
	// Draw timeout bar
	myOLED.drawFastHLine(0, 0, timeout_progress * OLED_WIDTH, 1);

	// Draw menu options
	size_t num_options = options.size();
	int option_spacing = OLED_WIDTH / (num_options + 1);
	
	for (size_t option_idx = 0; option_idx < options.size(); ++option_idx) {
	  int option_x = option_spacing * (option_idx + 1);
	  int option_y = OLED_HEIGHT * 0.75;
	  myOLED.setFont(pFontDefault);
	  myOLED.setCursor(option_x, option_y);
	  myOLED.print(options[option_idx]);
	}

	// Draw cursor
	int cursor_x = draw_cursor();

	// Get current input
	double current_weight = scale.get_weight();

	// Determine selected option
	size_t selected_option = (size_t)(cursor_x / (OLED_WIDTH / num_options));
	if (current_weight >= 10.0){
	  // Reset inactivity timer if we receive an input
	  inactivity_start = get_absolute_time();
	  myOLED.fillCircle((selected_option+1) * option_spacing + ((options[selected_option].size() * 6) / 2),
			    OLED_HEIGHT * 0.75, current_weight / 10, 2);

	  if (current_weight >= PRESS_THRESHOLD) {
	    printf("Selected %s\n", options[selected_option].c_str());
	    return selected_option;
	  }
	}

	// Update OLED
	myOLED.OLEDupdate();	
    }
    return 0;
}

void RunCalibration() {
    printf("\n=== Load Cell Multi-Point Calibration ===\n");
    printf("1. Remove all weight from the scale platform.\n");
    printf("   Press 'd' when the platform is empty...\n");
    
    while (true) {
        int c = getchar_timeout_us(100000);
        if (c != PICO_ERROR_TIMEOUT && (c == 'd' || c == 'D')) {
            break;
        }
    }
    
    printf("Recording zero offsets... Please wait...\n");
    scale.tare(30);
    printf("Zero offsets recorded.\n");
    
    printf("\n2. Place a known reference weight on the scale.\n");
    printf("   Enter the reference weight in grams (e.g. 200.0) followed by Enter:\n");
    
    char weight_str[32];
    int idx = 0;
    while (true) {
        int c = getchar_timeout_us(100000);
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\r' || c == '\n') {
                if (idx > 0) {
                    weight_str[idx] = '\0';
                    break;
                }
            } else if (c == 8 || c == 127) { // Backspace
                if (idx > 0) {
                    idx--;
                    printf("\b \b");
                }
            } else if (idx < 30) {
                weight_str[idx++] = (char)c;
                putchar(c);
            }
        }
    }
    
    double ref_weight = atof(weight_str);
    if (ref_weight <= 0.0) {
        printf("\nError: Invalid weight entered: %f. Calibration aborted.\n", ref_weight);
        return;
    }
    printf("\nReference weight set to: %.2f g\n", ref_weight);

    int32_t raw0_readings[5];
    int32_t raw1_readings[5];

    printf("\n3. We will now take 5 calibration readings at different positions.\n");
    printf("   For each step, place the weight at the specified location on the scale.\n");

    const char* positions[5] = {
        "Front-Left corner",
        "Back-Left corner",
        "Front-Right corner",
        "Back-Right corner",
        "Center of the platform"
    };

    for (int i = 0; i < 5; ++i) {
        printf("\n--> Position %d of 5: Place the weight on the %s.\n", i + 1, positions[i]);
        printf("    Press 'd' when the weight is in position and stable...\n");

        while (true) {
            int c = getchar_timeout_us(100000);
            if (c != PICO_ERROR_TIMEOUT && (c == 'd' || c == 'D')) {
                break;
            }
        }

        printf("    Reading sensors... ");
        int32_t r0_sum = 0;
        int32_t r1_sum = 0;
        int count = 0;
        for (int s = 0; s < 20 && count < 10; ++s) {
            int32_t r0, r1;
            if (scale.get_raw_sample(r0, r1)) {
                r0_sum += r0;
                r1_sum += r1;
                count++;
            }
            busy_wait_ms(15);
        }

        if (count < 8) {
            printf("FAILED. Not enough stable samples (got %d/10). Retrying this position...\n", count);
            i--; // Repeat this step
            continue;
        }

        raw0_readings[i] = r0_sum / count;
        raw1_readings[i] = r1_sum / count;
        printf("DONE (avg raw0: %ld, raw1: %ld)\n", (long)raw0_readings[i], (long)raw1_readings[i]);
    }

    printf("\nCalculating new calibration values... Please wait...\n");
    
    double new_factor0 = 0;
    double new_factor1 = 0;
    if (scale.calibrate_sensors(ref_weight, raw0_readings, raw1_readings, 5, new_factor0, new_factor1)) {
        printf("\n>>> Calibration SUCCESS <<<\n");
        printf("New scale factors computed (accounts for sensor variations):\n");
        printf("  Scale Factor 0 (Left Sensor):  %.2f\n", new_factor0);
        printf("  Scale Factor 1 (Right Sensor): %.2f\n", new_factor1);
        printf("\nTo make these changes permanent, update lines 13 in espresso_scale.cpp:\n");
        printf("  scale_factor0(%.2f), scale_factor1(%.2f)\n\n", new_factor0, new_factor1);
    } else {
        printf("\nError: Calibration failed.\n");
        printf("This can happen if the weight was not shifted between positions (too similar readings),\n");
        printf("or if the computed scale factors were mathematically invalid (e.g. negative).\n");
        printf("Please restart calibration and ensure you shift the weight as requested.\n\n");
    }
}

void ProcessSerialCommands() {
    int c = getchar_timeout_us(0);
    if (c != PICO_ERROR_TIMEOUT) {
        char cmd = (char)c;
        printf("Received command: %c\n", cmd);
        switch (cmd) {
            case 't':
            case 'T':
                printf("Taring scale...\n");
                scale.tare(10);
                printf("Tare complete.\n");
                break;
            case 's':
            case 'S':
                if (scale.is_timer_running()) {
                    scale.stop_timer();
                    printf("Timer stopped.\n");
                } else {
                    scale.start_timer();
                    printf("Timer started.\n");
                }
                break;
            case 'r':
            case 'R':
                scale.reset_timer();
                printf("Timer reset.\n");
                break;
            case 'a':
            case 'A':
                scale.enable_brew_assist(!scale.is_brew_assist_enabled());
                printf("Auto Brew Assist toggled. Enabled: %d\n", scale.is_brew_assist_enabled());
                break;
            case 'c':
            case 'C':
                RunCalibration();
                break;
            default:
                printf("Unknown command: %c. Available commands: T (tare), S (start/stop timer), R (reset timer), A (toggle brew assist), C (calibrate load cells)\n", cmd);
                break;
        }
    }
}

enum ScaleState {
  BOOT,
  NORMAL,
  LEFT_PRESSED,
  LEFT_RELEASED,
  RIGHT_PRESSED,
  RIGHT_RELEASED,
  MODE_SELECT,
  TARE,
  SETTINGS,
};

ScaleState state{ScaleState(BOOT)};

/** @brief Determine if a virtual button has been pressed.
 * A virtual button is pressed if the detected total weight is greater
 * than PRESS_THRESHOLD and the scale cg is in the bottom or top 10%.
 */
void DetectVirtualButtons() {
  double cg = scale.get_cg();
  // Check if button is pressed, set the scale state accordingly
  if (scale.get_weight() >= PRESS_THRESHOLD) {
    // Check left virtual button
    if (cg < 0.1) {
      state = ScaleState(LEFT_PRESSED);
    }
    else if (cg > 0.9) {
      state = ScaleState(RIGHT_PRESSED);
    }
  }

  // Check if we have released a pressed button
  if (std::abs(cg - 0.5) < 0.2) {
    if (state == ScaleState(LEFT_PRESSED))
      state = ScaleState(LEFT_RELEASED);
    else if (state == ScaleState(RIGHT_PRESSED))
      state = ScaleState(RIGHT_RELEASED);
  }

}

int main() {
    setup_default_uart();
    stdio_init_all();

    // Wait for serial terminal connection to prevent missing prints
    busy_wait_ms(2000);
    printf("\n=== Profitec Scale Booting ===\n");

    SetupOLED();
    DrawSplashScreen();

    printf("Initializing Espresso Scale...\n");
    scale.begin();
    printf("Scale initialized.\n");

    state = ScaleState(NORMAL);

    uint32_t last_display_update = 0;

    while (true) {
        // Read sensor values and update scale logic
        bool success = scale.update();

        // Process any incoming USB/UART control characters
        ProcessSerialCommands();

	// Detect virtual button presses
	DetectVirtualButtons();

        // Update display at ~15Hz (approx every 66ms) for fluid UI updates
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_display_update >= 1) {
            myOLED.OLEDclearBuffer();

	    switch(state) {
	      case ScaleState(NORMAL): {
		UpdateDisplay();
		break;
	      }
	    case ScaleState(LEFT_PRESSED): {
	        myOLED.setFont(pFontDefault);
		myOLED.setCursor(0, 10);
		myOLED.print("Brew Assist");
		break;
	    }
	    case ScaleState(LEFT_RELEASED): {
	      state = ScaleState(MODE_SELECT);
	      break;
	    }
	      // Mode select: select scale options including brew assist.
	      // Returns to normal state after selections are made.
	      case ScaleState(MODE_SELECT): {
		std::vector<std::string> menu_options{"no", "yes"};
		size_t ret = DisplayMenuLoop("Brew Assist?", menu_options, 5);
		scale.enable_brew_assist(ret == 1);
		state = ScaleState(NORMAL);
		break;
	      }
	    case ScaleState(RIGHT_PRESSED): {
	        myOLED.setFont(pFontDefault);
		myOLED.setCursor(0, 10);
		myOLED.print("Tare");
		break;
	    }
	    case ScaleState(RIGHT_RELEASED): {
	      sleep_ms(1000);
	      state = ScaleState(TARE);
	      break;
	    }
	      case ScaleState(TARE): {
		myOLED.setFont(pFontDefault);
		myOLED.setCursor(0, 10);
		myOLED.print("Taring...");
		scale.tare();
		state = ScaleState(NORMAL);
		break;
	      }
	    }
	    

	    myOLED.OLEDupdate();
            last_display_update = now;

	    // Output debug info to USB serial
            if (success) {
	      printf("CG: %.2f\n", scale.get_cg());
                // printf("Weight: %.2f g | Flow: %.2f g/s | Timer: %d s | State: %s\n",
                //        scale.get_weight(), scale.get_flow_rate(), 
                //        (int)scale.get_brew_time_s(), scale.get_brew_state_string());
            }
        }

        // Tiny delay to yield execution
        sleep_ms(2);
    }
}
