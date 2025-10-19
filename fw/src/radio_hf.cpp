#include "globals.h"
#include "radio.h"
#include "radio_hf.h"
#include "file_system.h"

#include <Arduino.h>
#include <si5351.h>

Si5351 si5351;

// Default frequency for SI5351 outputs
uint64_t DEFAULT_CLK0_HZ = 10000000;
uint64_t DEFAULT_CLK1_HZ = 10000000;
uint64_t DEFAULT_CLK2_HZ = 10000000;

void hf_si5351_init();

void hf_init() {
  hf_si5351_init();
}

void hf_si5351_init() {
  uint64_t si5351_xtal_freq = fs_load_setting_long(HARDWARE_FILE, "xtal_freq_hz", 8000000, 32000000);
  // rough error checking. TODO: move magic numbers elsewhere
  if(si5351_xtal_freq < 25900000 || si5351_xtal_freq > 26100000) {
    si5351_xtal_freq = 26000000;
    Serial.println("Wrong config file, setting to 26.000MHz");
  }

  Wire.begin(CLOCK_SDA, CLOCK_SCL);

  Serial.print("[SI5351] Status: ");
  Serial.println(si5351.si5351_read(SI5351_DEVICE_STATUS));

  // TODO: there are currently spurs from fractional dividers. Eliminate these

  si5351.init(SI5351_CRYSTAL_LOAD_8PF , si5351_xtal_freq, 0);

  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);

  si5351.set_ms_source(SI5351_CLK0, SI5351_PLLA);
  si5351.set_ms_source(SI5351_CLK1, SI5351_PLLA);
  si5351.set_ms_source(SI5351_CLK2, SI5351_PLLB);

  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);

  // Load optional default clock frequencies from the hardware config file
  // Load optional default clock frequencies from preferences (no fallback)
  if (fs_setting_exists(PREFERENCE_FILE, "clk0_default")) {
    long v = fs_load_setting_long(PREFERENCE_FILE, "clk0_default", 0, 2000000000);
    DEFAULT_CLK0_HZ = (uint64_t) v;
    Serial.print("Loaded clk0_default from preferences: "); Serial.println(DEFAULT_CLK0_HZ);
  }

  if (fs_setting_exists(PREFERENCE_FILE, "clk1_default")) {
    long v = fs_load_setting_long(PREFERENCE_FILE, "clk1_default", 0, 2000000000);
    DEFAULT_CLK1_HZ = (uint64_t) v;
    Serial.print("Loaded clk1_default from preferences: "); Serial.println(DEFAULT_CLK1_HZ);
  }

  if (fs_setting_exists(PREFERENCE_FILE, "clk2_default")) {
    long v = fs_load_setting_long(PREFERENCE_FILE, "clk2_default", 0, 2000000000);
    DEFAULT_CLK2_HZ = (uint64_t) v;
    Serial.print("Loaded clk2_default from preferences: "); Serial.println(DEFAULT_CLK2_HZ);
  }

  // Set default frequency to parameterized values (from config or fallback)
  si5351.set_freq(DEFAULT_CLK0_HZ * 100, SI5351_CLK0);
  si5351.set_freq(DEFAULT_CLK1_HZ * 100, SI5351_CLK1);
  si5351.set_freq(DEFAULT_CLK2_HZ * 100, SI5351_CLK2);

  si5351.output_enable(SI5351_CLK0, 1);
  si5351.output_enable(SI5351_CLK1, 1);
  si5351.output_enable(SI5351_CLK2, 1);

  // set phase for each clock output
  si5351.set_phase(SI5351_CLK0, 0);
  si5351.set_phase(SI5351_CLK1, 0);
  si5351.set_phase(SI5351_CLK2, 0);

  si5351.pll_reset(SI5351_PLLA);
  si5351.pll_reset(SI5351_PLLB);
}