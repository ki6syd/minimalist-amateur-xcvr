#define SWEEP_SETTLING_TIME 10
#define JSON_DOC_SIZE       8192
#define SELF_TEST_VOL       2
#define BPF_SWEEP_SPREAD    4000000
#define BPF_SWEEP_NUM_PTS   50
#define XTAL_SWEEP_SPREAD   9000
#define XTAL_SWEEP_NUM_PTS  50
#define AUDIO_SWEEP_SPREAD  3000
#define AUDIO_SWEEP_NUM_PTS 100
#define SUPPR_SWEEP_SPREAD  4000
#define SUPPR_SWEEP_NUM_PTS 3

void handle_selftest(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) { 
  if(request_type == HTTP_POST) {
    // look for required parameters in the message
    if(!request->hasParam("testName")) {
      Serial.println("[DEBUG] testName not sent");
      request->send(400, "text/plain", "testName not sent");
      return;
    }

    String test_type = request->getParam("testName")->value();
    String response;

    if(test_type == "BPF")
      response = bpf_sweep();
    if(test_type == "XTAL")
      response = xtal_sweep();
    if(test_type == "AUDIO")
      response = audio_sweep();
    if(test_type == "SUPPR")
      response = suppr_sweep();
    if(test_type == "XMIT")
      response = xmit_test();
    
    request->send(201, "application/json", response);
  }
}

void handle_raw_samples(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) { 

  if(request_type == HTTP_POST) {
    DynamicJsonDocument doc(JSON_DOC_SIZE);
    JsonArray data = doc.createNestedArray("data");
    String response;
    uint16_t num_samples = 100;
    // set sample interval at 200us, ADC read itself takes about 100
    uint16_t sample_interval = 200;
    uint64_t last_sample_time = micros();
    float readings[num_samples];
    float sum = 0, avg = 0, rms = 0;

    gpio_write(OUTPUT_ADC_SEL, OUTPUT_SEL_AUDIO);

    Serial.println("Beginning sampling");

    for(uint16_t i=0; i<num_samples; i++) {
      gpio_write(OUTPUT_GREEN_LED, OUTPUT_ON);
      // delay to get correct sample rate (roughly)
      while(micros() - last_sample_time < sample_interval);
      
      // sample
      readings[i] = ADC_SCALING_AUDIO * analogRead(A0);
      gpio_write(OUTPUT_GREEN_LED, OUTPUT_OFF);
      // update time
      last_sample_time = micros();
    }

    Serial.println("Done sampling");

    // averaging loop
    for(uint16_t i=0; i<num_samples; i++) {
      sum += readings[i];
    }
    avg = sum / num_samples;

    // rms loop. note that this subtracts out the average
    for(uint16_t i=0; i<num_samples; i++) {
      readings[i] -= avg;
      rms += readings[i] * readings[i];
    }
    rms = sqrt(rms/num_samples);

    Serial.print("Average: ");
    Serial.println(avg);
    Serial.print("RMS: ");
    Serial.print(rms*1000);
    Serial.println("mV");


    // package and send data
    for(uint8_t i=0; i<num_samples; i++) {
      JsonObject data_0 = data.createNestedObject();
      data_0["x"] = i * sample_interval;
      data_0["y"] = readings[i];
    }

    serializeJson(doc, response);
    request->send(201, "application/json", response);
  }
}

// BPF sweep: hold if and audio frequencies constant, but adjust the first LO and RF frequency
// returns serialized JSON 
String bpf_sweep() {
  DynamicJsonDocument doc(JSON_DOC_SIZE);
  JsonArray data = doc.createNestedArray("data");
  String response;

  // set up hardware for a sweep
  if(tx_rx_mode != MODE_RX)
    return "";
    
  gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_SSB);
  gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
  // update_volume(SELF_TEST_VOL);
  si5351.output_enable(SI5351_CLK2, 1);

  // define sweep start/stop
  uint64_t f_rf_orig = f_rf;
  uint64_t f_rf_min = f_rf - BPF_SWEEP_SPREAD/2;
  uint64_t f_rf_max = f_rf + BPF_SWEEP_SPREAD/2;
  uint64_t step_size = BPF_SWEEP_SPREAD / BPF_SWEEP_NUM_PTS;
  uint16_t i = 0;

  // sweep and measure. hold BFO and audio frequency fixed
  for(f_rf=f_rf_min; f_rf < f_rf_max; f_rf += step_size) {
    f_vfo = update_vfo(f_rf, f_bfo, f_audio);
    set_clocks(f_bfo, f_vfo, f_rf);     
    // delay for settling, also blink light
    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
    my_delay(SWEEP_SETTLING_TIME);
    update_smeter();
    gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
    my_delay(SWEEP_SETTLING_TIME);

    Serial.print("RF: ");
    print_uint64_t(f_rf);
    Serial.print("VFO: ");
    print_uint64_t(f_vfo);
    Serial.print("BFO: ");
    print_uint64_t(f_bfo);
    Serial.println();

    JsonObject data_0 = data.createNestedObject();
    data_0["x"] = f_rf/1000;
    data_0["y"] = last_smeter;
    i++;
    
    // watchdog may not get fed in this long function
    ESP.wdtFeed();
  }
        
  // return clocks to original value
  f_rf = f_rf_orig;
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  set_clocks(f_bfo, f_vfo, f_rf);
  si5351.output_enable(SI5351_CLK2, 0);
  
  gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
  Serial.println("Done sweeping BPF");

  serializeJson(doc, response);
  return response;
}


// Crystal sweep: hold rf and audio frequencies constant, but adjust the IF 
// returns serialized JSON 
String xtal_sweep() {
  DynamicJsonDocument doc(JSON_DOC_SIZE);
  JsonArray data = doc.createNestedArray("data");
  String response;

  // set up hardware for a sweep
  if(tx_rx_mode != MODE_RX)
    return "";
  gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_SSB);
  gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
  update_volume(SELF_TEST_VOL);
  si5351.output_enable(SI5351_CLK2, 1);

  // define sweep start/stop
  uint64_t f_if_orig = f_if;
  uint64_t f_if_min = f_if - XTAL_SWEEP_SPREAD/2;
  uint64_t f_if_max = f_if + XTAL_SWEEP_SPREAD/2;
  uint16_t step_size = XTAL_SWEEP_SPREAD / XTAL_SWEEP_NUM_PTS;
  uint16_t i = 0;

  // sweep and measure. hold BFO and audio frequency fixed
  for(f_if=f_if_min; f_if < f_if_max; f_if += step_size) {
    f_bfo = f_if + f_audio;
    f_vfo = update_vfo(f_rf, f_bfo, f_audio);
    set_clocks(f_bfo, f_vfo, f_rf);     
    // delay for settling, also blink light
    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
    my_delay(SWEEP_SETTLING_TIME);
    update_smeter();
    gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
    my_delay(SWEEP_SETTLING_TIME);

    Serial.print("VFO: ");
    print_uint64_t(f_vfo);
    Serial.print("BFO: ");
    print_uint64_t(f_bfo);
    Serial.println();

    JsonObject data_0 = data.createNestedObject();
    data_0["x"] = (int64_t) f_if - (int64_t) f_if_orig;
    data_0["y"] = last_smeter;
    i++;
    
    // watchdog may not get fed in this long function
    ESP.wdtFeed();
  }
        
  // return clocks to original value
  f_if = f_if_orig;
  f_bfo = f_if + f_audio;
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  set_clocks(f_bfo, f_vfo, f_rf);
  si5351.output_enable(SI5351_CLK2, 0);
  gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
  Serial.println("Done sweeping xtal");

  serializeJson(doc, response);
  return response;
}


// audio sweep: hold rf and if frequencies constant, but adjust the bfo
// returns serialized JSON 
String audio_sweep() {
  DynamicJsonDocument doc(JSON_DOC_SIZE);
  JsonArray data = doc.createNestedArray("data");
  String response;

  // set up hardware for a sweep
  if(tx_rx_mode != MODE_RX)
    return "";
  gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_CW); 
  gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
  update_volume(SELF_TEST_VOL);
  si5351.output_enable(SI5351_CLK2, 1);
  

  // define sweep start/stop
  uint64_t f_bfo_orig = f_bfo;
  uint64_t f_bfo_min = f_bfo - f_audio - AUDIO_SWEEP_SPREAD/2;
  uint64_t f_bfo_max = f_bfo - f_audio + AUDIO_SWEEP_SPREAD/2;
  uint16_t step_size = AUDIO_SWEEP_SPREAD / AUDIO_SWEEP_NUM_PTS;
  uint16_t i = 0;

  // sweep and measure. hold VFO, IF, and audio frequency fixed
  for(f_bfo=f_bfo_min; f_bfo < f_bfo_max; f_bfo += step_size) {
    set_clocks(f_bfo, f_vfo, f_rf);     
    // delay for settling, also blink light
    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
    my_delay(SWEEP_SETTLING_TIME);
    update_smeter();
    gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
    my_delay(SWEEP_SETTLING_TIME);

    Serial.print("VFO: ");
    print_uint64_t(f_vfo);
    Serial.print("BFO: ");
    print_uint64_t(f_bfo);
    Serial.println();

    JsonObject data_0 = data.createNestedObject();
    data_0["x"] = (int64_t) f_bfo - (int64_t) f_bfo_orig;
    data_0["y"] = last_smeter;
    i++;
    
    // watchdog may not get fed in this long function
    ESP.wdtFeed();
  }
        
  // return clocks to original value
  f_bfo = f_bfo_orig;
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  set_clocks(f_bfo, f_vfo, f_rf);
  si5351.output_enable(SI5351_CLK2, 0);
  gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
  Serial.println("Done sweeping audio");

  serializeJson(doc, response);
  return response;
}




// sideband suppression sweep: sweep RF while leaving RX clocks fixed. Tests unwanted sideband rejection
// returns serialized JSON 
String suppr_sweep() {
  DynamicJsonDocument doc(JSON_DOC_SIZE);
  JsonArray data = doc.createNestedArray("data");
  String response;
  float meter_readings[SUPPR_SWEEP_NUM_PTS];

  // set up hardware for a sweep
  if(tx_rx_mode != MODE_RX)
    return "";
  gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_SSB); 
  gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
  // extra low volume for this test
  update_volume(1);
  si5351.output_enable(SI5351_CLK2, 1);
  
  // define sweep start/stop
  uint64_t f_rf_nom = f_rf;
  uint64_t f_rf_list[] = {f_rf - 2*f_audio, f_rf_nom, f_rf + 2*f_audio};
 
  // sweep and measure
  for(uint8_t i=0; i < SUPPR_SWEEP_NUM_PTS; i++) {
    f_rf = f_rf_list[i];
    set_clocks(f_bfo, f_vfo, f_rf);     
    // delay for settling, also blink light
    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
    my_delay(500);
    update_smeter();
    gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
    my_delay(500);

    Serial.print("RF: ");
    print_uint64_t(f_rf);
    Serial.print("VFO: ");
    print_uint64_t(f_vfo);
    Serial.print("BFO: ");
    print_uint64_t(f_bfo);
    Serial.println();

    JsonObject data_0 = data.createNestedObject();
    data_0["x"] = (int64_t) f_rf - (int64_t) f_rf_nom;
    data_0["y"] = last_smeter;
    
    // watchdog may not get fed in this long function
    ESP.wdtFeed();
  }
        
  // return clocks to original value
  f_rf = f_rf_nom;
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  set_clocks(f_bfo, f_vfo, f_rf);
  si5351.output_enable(SI5351_CLK2, 0);
  gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
  Serial.println("Done sweeping for suppression test");

  Serial.println(response);
  serializeJson(doc, response);
  return response;
}

// test routine for power amplifier
String xmit_test() {
  key_on();
  my_delay(10000);
  key_off();

  return "";
}
