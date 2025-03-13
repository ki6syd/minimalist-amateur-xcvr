#include "globals.h"
#include "power.h"
#include "radio.h"
#include "io.h"
#include "file_system.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define POWER_BUCK_FSW      510e3
#define VDIODE              0.6
#define VUSB_MAX            5.5

#define BIAS_CTRL_BITS      8
#define BIAS_CTRL_FREQ      100e3
#define BIAS_MAX_DUTY       0.9
#define BIAS_TOLERANCE      0.1

#define AGC_CTRL_BITS       8
#define AGC_CTRL_FREQ       100e3

#define TEMP_ABSOLUTE_0     273.15
#define TEMP_REFERENCE      (25 + TEMP_ABSOLUTE_0)

#define NUM_BIAS_OUTPUTS    2
#define BIAS_KP             1
#define BIAS_KD             -4
#define BIAS_DUTY_INITIAL   0.5


power_bias_channel_t bias_outputs[] = {BIAS_CHANNEL_0, BIAS_CHANNEL_1};
float pa_curr_error = 0, pa_last_duty = 0.25, pa_duty = 0.25, pa_integral = 0;
float pa_bias_target = 0;
float bias_duties[NUM_BIAS_OUTPUTS];
float agc_duty;

TaskHandle_t xAnalogSenseTaskHandle;
SemaphoreHandle_t xADCmutex;

uint32_t freq_buck = POWER_BUCK_FSW;
float input_volt = 0, pa_curr = 0, pa_volt = 0, pa_temp = 0;
float vbat_cell_low = 3.0;
uint16_t num_cell = 3;
uint32_t num_low_samples = 0;

void analog_sense_task(void *pvParameter);
void biasing_task(void *pvParameter);
void power_set_bias_duty(power_bias_channel_t channel, float duty);
void power_sweep_duty();
void power_set_agc_duty(float duty);

void power_init() {
  pinMode(ADC_MUX_CTRL_0, OUTPUT);
  pinMode(ADC_MUX_CTRL_1, OUTPUT);
  pinMode(PA_VDD_CTRL, OUTPUT);
  digitalWrite(PA_VDD_CTRL, LOW);   // VDD off

  // initialize mutex for ADC readings through the mux
  xADCmutex = xSemaphoreCreateMutex();
  if(xADCmutex == NULL) {
    Serial.println("Error creating mutex xADCmutex");
  }

  // load configuration from JSON file
  if(fs_setting_exists(PREFERENCE_FILE, "vbat_cell_low"))
    vbat_cell_low = fs_load_setting(PREFERENCE_FILE, "vbat_cell_low").toFloat();
  if(fs_setting_exists(PREFERENCE_FILE, "vbat_cell_low"))
    num_cell = fs_load_setting(PREFERENCE_FILE, "num_battery").toInt();

  // set up a PWM channel that drives buck converter sync pin
  // feature for the future: select this frequency based on the dial frequency
  ledcSetup(PWM_CHANNEL_SYNC, freq_buck, 2);
  ledcAttachPin(BUCK_SYNC, PWM_CHANNEL_SYNC);
  ledcWrite(PWM_CHANNEL_SYNC, 2);

  // set up BIAS_CTRL_0, _1 using different LEDC channel
  ledcSetup(PWM_CHANNEL_BIAS_0, BIAS_CTRL_FREQ, BIAS_CTRL_BITS);
  ledcAttachPin(BIAS_CTRL_0, PWM_CHANNEL_BIAS_0);
  ledcWrite(PWM_CHANNEL_BIAS_0, 1);

  ledcSetup(PWM_CHANNEL_BIAS_1, BIAS_CTRL_FREQ, BIAS_CTRL_BITS);
  ledcAttachPin(BIAS_CTRL_1, PWM_CHANNEL_BIAS_1);
  ledcWrite(PWM_CHANNEL_BIAS_1, 1);

  // set up AGC_CTRL using different LEDC channel
  ledcSetup(PWM_CHANNEL_AGC, BIAS_CTRL_FREQ, BIAS_CTRL_BITS);
  ledcAttachPin(AGC_CTRL, PWM_CHANNEL_AGC);
  ledcWrite(PWM_CHANNEL_AGC, 1);

  // set bias channels to zero
  for(uint16_t i = 0; i < NUM_BIAS_OUTPUTS; i++)
    power_set_bias_duty(bias_outputs[i], 0);

  // find gate bias point
  power_bias_to_current(BIAS_CURRENT_CW);

  // todo: run the bias sweep function and log data. use for lookup of starting point.

  // set AGC voltage
  power_agc_to_voltage(AGC_VOLT_RX);

  xTaskCreatePinnedToCore(
      analog_sense_task,
      "Analog Sensing",
      4096,
      NULL,
      TASK_PRIORITY_ADC, // priority
      &xAnalogSenseTaskHandle,
      TASK_CORE_ADC // core
  );
}

// TODO: force transition in radio module if battery power drops too low
void analog_sense_task(void *param) {
  while(true) {
    // update all ADC readings, only if mutex is available. Blocks until available.
    if(xSemaphoreTake(xADCmutex, portMAX_DELAY) == pdTRUE) {
      // read all ADC channels
      input_volt = power_adc_conversion(ADC_CHANNEL_VIN);
      pa_volt = power_adc_conversion(ADC_CHANNEL_PA_VDD);
      pa_curr = power_adc_conversion(ADC_CHANNEL_PA_IDD);
      pa_temp = power_adc_conversion(ADC_CHANNEL_PA_TEMP);

      // give back the mutex after reading
      xSemaphoreGive(xADCmutex);
    }


    // TODO: build temperature monitoring logic. Cut off at high temp, shift biasing with temp

    // TODO: move the below battery monitoring logic into a different task from ADC reads
    // figure out whether we're on USB power? Don't run this logic if voltage is very low
    if(input_volt + VDIODE > VUSB_MAX) {
      // increment counter if input voltage (corrected for diode drop) is too low
      if(input_volt + VDIODE < (num_cell * vbat_cell_low)) {
        num_low_samples++;
      }
      else {
        num_low_samples = 0;
        // comment in to allow TX without rebooting
        // radio_enable_tx(true);
      }

      // disallow TX if input voltage drops too low
      if(num_low_samples > 10) {
        radio_enable_tx(false);
        io_set_blink_mode(BLINK_ERROR);
        Serial.println("*****Input voltage has dropped below safe value*****");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

// updates buck converters to a new switching frequency [Hz]
// TODO: test this, implement logic to set it based on dial frequency
void power_update_freq(uint32_t new_freq) {
    if(new_freq < 1e6 && new_freq > 100e3) {
        freq_buck = new_freq;
        ledcChangeFrequency(PWM_CHANNEL_SYNC, freq_buck, 2);
    }
}

float power_adc_conversion(adc_channel_t channel) {
  if(channel == ADC_CHANNEL_VIN)
    return (float) analogRead(ADC_VDD) * ADC_MAX_VOLT / ADC_VDD_SCALE / ADC_FS_COUNTS;
  else {
    // set mux control pins
    switch(channel) {
      case ADC_CHANNEL_PA_VDD:
        digitalWrite(ADC_MUX_CTRL_0, LOW);
        digitalWrite(ADC_MUX_CTRL_1, HIGH);
        break;
      case ADC_CHANNEL_PA_IDD:
        digitalWrite(ADC_MUX_CTRL_0, HIGH);
        digitalWrite(ADC_MUX_CTRL_1, LOW);
        break;
      case ADC_CHANNEL_PA_TEMP:
        digitalWrite(ADC_MUX_CTRL_0, LOW);
        digitalWrite(ADC_MUX_CTRL_1, LOW);
        break;
    }

    // delay to allow settling
    // TODO: remove this delay if there was no change to the mux control. Would allow higher read rates
    vTaskDelay(pdMS_TO_TICKS(2));

    // scale and return
    switch(channel) {
      case ADC_CHANNEL_PA_VDD:
        return (float) analogRead(ADC_MUX_OUT) * ADC_MAX_VOLT / ADC_PA_VDD_SCALE / ADC_FS_COUNTS;
      case ADC_CHANNEL_PA_IDD:
        return ((float) analogRead(ADC_MUX_OUT) * ADC_MAX_VOLT / ADC_PA_IDD_SCALE / ADC_FS_COUNTS);
      case ADC_CHANNEL_PA_TEMP:
        float thermistor_voltage = (float) analogRead(ADC_MUX_OUT) * ADC_MAX_VOLT / ADC_FS_COUNTS;
        float thermistor_resistance =  ADC_PA_THERM_RES * (thermistor_voltage / (ADC_MAX_VOLT - thermistor_voltage));
        float temp_kelvin = (ADC_PA_THERM_BETA * TEMP_REFERENCE) / (ADC_PA_THERM_BETA + (TEMP_REFERENCE * log(thermistor_resistance / ADC_PA_THERM_RES)));

        // TODO: temperature calculation based on resistance and beta
        return temp_kelvin - TEMP_ABSOLUTE_0;
    }
  }

  return 0;
}

float power_get_input_volt() {
  return input_volt;
}

float power_get_pa_current() {
  return pa_curr;
}

float power_get_pa_volt() {
  return pa_volt;
}

float power_get_pa_temp() {
  return pa_temp;
}

void power_set_bias_duty(power_bias_channel_t channel, float duty) {
  if(duty < 0)
    duty = 0;
  if(duty > BIAS_MAX_DUTY)
    duty = BIAS_MAX_DUTY;

  uint32_t counts = (uint32_t) (duty * (float) (1 << BIAS_CTRL_BITS));

  if(channel == BIAS_CHANNEL_0) {
    ledcWrite(PWM_CHANNEL_BIAS_0, counts);
  }
  else if(channel == BIAS_CHANNEL_1) {
    ledcWrite(PWM_CHANNEL_BIAS_1, counts);
  }
}

// debug function - sweeps duty cycle for each channel and reads out current
void power_sweep_duty() {
  float measured_current = 0;

  // turn off all bias channels before beginning measurement process
  for(uint16_t i = 0; i < NUM_BIAS_OUTPUTS; i++)
    power_set_bias_duty(bias_outputs[i], 0);

  vTaskDelay(pdMS_TO_TICKS(10));
  digitalWrite(PA_VDD_CTRL, HIGH);
  vTaskDelay(pdMS_TO_TICKS(10));

  // lock the mutex for the duration of this function
  if(xSemaphoreTake(xADCmutex, portMAX_DELAY) != pdTRUE)
    return;

  // sweep each channel
  for(uint16_t i = 0; i < NUM_BIAS_OUTPUTS; i++) {
    Serial.print("biasing channel ");
    Serial.println(i);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    for(float duty = 0; duty < 1; duty += 0.01) {
      // set duty cycle, allow for settling
      bias_duties[i] = duty;
      power_set_bias_duty(bias_outputs[i], bias_duties[i]);
      vTaskDelay(pdMS_TO_TICKS(10));

      measured_current = power_adc_conversion(ADC_CHANNEL_PA_IDD);
      
      Serial.print("duty: ");
      Serial.print(bias_duties[i]);
      Serial.print("\tmeasured_current: ");
      Serial.println(measured_current);

      // stop running if current got too high 
      if(measured_current > BIAS_MAX_PER_CH)
        break;
    }
    
    // shut down PWM before moving to next one
    power_set_bias_duty(bias_outputs[i], 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    // todo: debug why this doesn't seem to actually bring current to zero
  }

  xSemaphoreGive(xADCmutex);

  digitalWrite(PA_VDD_CTRL, LOW);
}

// sweeps gate voltage until each half of the power amplifier draws total_current/2
// then leaves the amplifier at this bias point, no longer actively controls
// this function only returns when current is stable
void power_bias_to_current(float total_current) {
  // useful for debug
  // power_sweep_duty();

  // lock the mutex for the duration of this function
  if(xSemaphoreTake(xADCmutex, portMAX_DELAY) != pdTRUE)
    return;

  float measured_current = 0;

  // remember this setting
  pa_bias_target = total_current;

  // special case: nearly zero bias current. Don't actually want any duty cycle
  if(total_current < 0.005) {
    for(uint16_t i = 0; i < NUM_BIAS_OUTPUTS; i++) {
      bias_duties[i] = 0;
      power_set_bias_duty(bias_outputs[i], bias_duties[i]);
    }
    Serial.println("Biasing to zero current");
    return;
  }

  // turn on power amplifier, wait to allow it to settle
  digitalWrite(PA_VDD_CTRL, HIGH);
  vTaskDelay(pdMS_TO_TICKS(10));

  // check if TOTAL biasing is correct. can exit if it is already set properly
  measured_current = power_adc_conversion(ADC_CHANNEL_PA_IDD);
  
  // check error, return if in-spec
  if(abs(measured_current - total_current) < (BIAS_TOLERANCE * total_current)) {
    Serial.print("Bias current already in spec: ");
    Serial.println(measured_current);
    return;
  }

  // turn off all bias channels before beginning measurement process
  for(uint16_t i = 0; i < NUM_BIAS_OUTPUTS; i++)
    power_set_bias_duty(bias_outputs[i], 0);

  // initialize bias duty cycles to useful starting point
  for(uint16_t i = 0; i < NUM_BIAS_OUTPUTS; i++)
    bias_duties[i] = BIAS_DUTY_INITIAL;

  // find bias point for each channel
  float target_current = total_current / 2;
  
  for(uint16_t i = 0; i < NUM_BIAS_OUTPUTS; i++) {
    Serial.print("biasing channel ");
    Serial.println(i);
    vTaskDelay(pdMS_TO_TICKS(10));
    float error = 0, prev_error = 0;
    uint16_t cycle_counter = 0, correct_counter = 0, railed_counter = 0;
    do {
      // set duty cycle, allow for settling
      power_set_bias_duty(bias_outputs[i], bias_duties[i]);
      vTaskDelay(pdMS_TO_TICKS(3));

      // wait for mutex, then measure current
      measured_current = power_adc_conversion(ADC_CHANNEL_PA_IDD);
      
      // control error to zero with PID controller
      prev_error = error;
      error = target_current - measured_current;
      bias_duties[i] += error * BIAS_KP + (prev_error - error) * BIAS_KD;

      Serial.print("measured_current: ");
      Serial.print(measured_current);
      Serial.print("\terror: ");
      Serial.print(error);
      Serial.print("\tnew duty: ");
      Serial.println(bias_duties[i]);

      // possible case: no bias current at all (e.g. no power board). Fault after 10 iterations with extreme duty
      if(bias_duties[i] > 1 || bias_duties[i] < 0)
        railed_counter++;
      else
        railed_counter = 0;

      if(railed_counter > 10) {
        Serial.print("bias duty out of range for channel ");
        Serial.println(i);
        bias_duties[i] = 0;
        break;
      }
      
      // add to counter if bias is correct. Wait for 3 successive correct values.
      if(abs(error) < (BIAS_TOLERANCE * target_current))
        correct_counter++;
      else
        correct_counter = 0;

      // abort if current is not settling
      cycle_counter++;
      if(cycle_counter > 25) {
        Serial.println("Bias settling failed");
        bias_duties[i] = 0;
        break;
      }
    }
    while(correct_counter < 3);

    // shut down PWM before moving to next one
    power_set_bias_duty(bias_outputs[i], 0);
  }

  xSemaphoreGive(xADCmutex);

  // implement bias points we've found already
  Serial.print("Bias duty cycles: ");
  for(uint16_t i = 0; i < NUM_BIAS_OUTPUTS; i++) {
    power_set_bias_duty(bias_outputs[i], bias_duties[i]);
    Serial.print(bias_duties[i]);
    Serial.print("\t");
  }
  Serial.println();

  digitalWrite(PA_VDD_CTRL, LOW);
}

// set both channels to the specified bias voltage
void power_bias_to_voltage(float voltage) {  
  if(voltage < 0 || voltage > BIAS_MAX_VOLT * BIAS_VOLT_GAIN) {
    return;
  }

  // calculate duty cycle
  float duty = voltage / (BIAS_MAX_VOLT * BIAS_VOLT_GAIN);

  // apply duty cycle to each channel
  Serial.print("Bias duty cycles: ");
  for(uint16_t i = 0; i < NUM_BIAS_OUTPUTS; i++) {
    bias_duties[i] = duty;
    power_set_bias_duty(bias_outputs[i], duty);
    Serial.print(duty);
    Serial.print("\t");
  }
  Serial.println();

  // put bias control line in a deterministic state
  digitalWrite(PA_VDD_CTRL, LOW);
}

float power_get_bias_target() {
  return pa_bias_target; 
}

float power_get_bias_duty(power_bias_channel_t channel) {
  return bias_duties[channel];
}

float power_get_agc_duty() {
  return agc_duty;
}

void power_set_agc_duty(float duty) {
  if(duty < 0)
  duty = 0;
  if(duty > 1)
    duty = 1;

  uint32_t counts = (uint32_t) (duty * (float) (1 << AGC_CTRL_BITS));

  ledcWrite(PWM_CHANNEL_AGC, counts);
}

void power_agc_to_voltage(float voltage) {
  if(voltage < 0 || voltage > AGC_MAX_VOLT * AGC_VOLT_GAIN) {
    return;
  }

  // calculate duty cycle
  agc_duty = voltage / (AGC_MAX_VOLT * AGC_VOLT_GAIN);

  // apply duty cycle to AGC output
  Serial.print("AGC duty cycle: ");
  Serial.print(agc_duty);
  power_set_agc_duty(agc_duty);
  
  Serial.println();
}