
// updates the vfo frequency based on f_rf, f_bfo, and f_cw
// TODO: handle high side or low side injection on both VFO and BFO
// TODO: make sure we don't generate negative frequencies
uint64_t update_vfo(uint64_t f_rf, uint64_t f_bfo, uint64_t f_audio) {
  if(f_rf > f_if)
    f_vfo = f_rf + f_if;
  else
    f_vfo = f_if - f_rf;
    
  return f_vfo;
}


// update SI5351 clocks
// TODO: update this so it's possible to take in smaller steps, such as FT8 at 6.25Hz tone spacing
void set_clocks(uint64_t clk_0, uint64_t clk_1, uint64_t clk_2) {    
  Serial.println("[SI5351] Clocks updating:");
  Serial.print("\t");
  print_uint64_t(clk_0);
  Serial.print("\t");
  print_uint64_t(clk_1);
  Serial.print("\t");
  print_uint64_t(clk_2);
  Serial.println("\t");
  
  si5351.set_freq(clk_0 * 100, SI5351_CLK0);
  si5351.set_freq(clk_1 * 100, SI5351_CLK1);
  si5351.set_freq(clk_2 * 100, SI5351_CLK2);
}

// update f_rf with fine steps (0.01 Hz units). Used in digital modes where <1Hz accuracy is helpful
// todo: clean this up, merge functionality with above function
void set_clk2_fine(uint64_t clk_2) {
  Serial.print("[Si5351] CLK2 Fine Steps");
  Serial.print("\t");
  print_uint64_t(clk_2);

  si5351.set_freq(clk_2, SI5351_CLK2);
}
