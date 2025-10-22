// Minimal client for index.html — only functions used by the page

var api_base_url = "/api/v1/";

function http_request(type, path, keys, values, callback) {
  var str = api_base_url + path + "?";
  for (var i = 0; i < keys.length; i++) {
    str += keys[i] + "=" + encodeURIComponent(values[i]) + "&";
  }
  str = str.substring(0, str.length - 1);

  var xhr = new XMLHttpRequest();
  xhr.open(type, str, true);
  if (callback) xhr.onreadystatechange = callback;
  xhr.send();
}

function on_load() {
  // initialize periodic reads for dynamic values
  fetch_input_voltage();
  setInterval(fetch_input_voltage, 5000);
  // ensure default superhet mode visibility (default to single-IF)
  switch_superhet_mode('single');
}

function set_clocks() {
  var clk0_hz = parseInt(document.getElementById('clk0_hz').value) || 0;
  var clk1_hz = parseInt(document.getElementById('clk1_hz').value) || 0;
  var clk2_hz = parseInt(document.getElementById('clk2_hz').value) || 0;

  // Use dedicated clocks endpoint
  http_request('PUT', 'clocks', ['clk0', 'clk1', 'clk2'], [clk0_hz, clk1_hz, clk2_hz]);
}

function set_phase() {
  var phase0 = parseInt(document.getElementById('phase0').value) || 0;
  var phase1 = parseInt(document.getElementById('phase1').value) || 0;
  var phase2 = parseInt(document.getElementById('phase2').value) || 0;

  http_request('PUT', 'phase', ['phase0', 'phase1', 'phase2'], [phase0, phase1, phase2]);
}

function set_clock_state(clk, enable) {
  // Set the state of a specific clock (on/off)
  http_request('PUT', 'clock_toggle', ['clk', 'enable'], [clk, enable ? 1 : 0]);
}

function calculate_clocks() {
  // Read inputs (all in Hz)
  var dial = parseInt(document.getElementById('dial_hz').value) || 0;
  var if1 = parseInt(document.getElementById('if1_hz').value) || 0;
  var if2 = parseInt(document.getElementById('if2_hz').value) || 0;
  var sidetone = parseInt(document.getElementById('sidetone_hz').value) || 0;
  var usb = document.getElementById('usb_chk').checked;
  var upconvert = document.getElementById('upconvert_chk').checked;

  // Basic validation
  if (dial <= 0 || if1 <= 0 || if2 <= 0) {
    alert('Dial and IF must be positive numbers (Hz).');
    return;
  }


  // Dual-superhet mapping:
  // LO1 (CLK0) = Dial +/- IF1 (upconvert controls sign)
  var lo1 = upconvert ? (dial + if1) : (dial - if1);
  if (lo1 <= 0) {
    alert('Calculated LO1 is non-positive. Check Dial/IF1 and Upconvert settings.');
    return;
  }

  // LO2 (CLK1) = LO1 +/- IF2
  var lo2 = if1 - if2;
  if (lo2 <= 0) {
    alert('Calculated LO2 is non-positive. Check IF2 and Upconvert settings.');
    return;
  }

  // BFO (CLK2) = IF2 +/- Sidetone (absolute frequency around IF2)
  var bfo = usb ? (if2 + sidetone) : (if2 - sidetone);

  // Populate the clock fields (Hz)
  document.getElementById('clk0_hz').value = lo1;
  document.getElementById('clk1_hz').value = lo2;
  document.getElementById('clk2_hz').value = bfo;

  // Auto-apply the calculated clock frequencies to the device
  console.log('Auto-applying calculated clocks: ', lo1, lo2, bfo);
  set_clocks();
}

// Switch between dual-IF and single-IF superhet panels
function switch_superhet_mode(mode) {
  var dual = document.getElementById('superhet-dual');
  var single = document.getElementById('superhet-single');
  if (!dual || !single) return;
  if (mode === 'single') {
    dual.style.display = 'none';
    single.style.display = '';
  } else {
    dual.style.display = '';
    single.style.display = 'none';
  }
}

// New single-IF calculation: takes a single IF frequency and computes individual clocks
function calculate_clocks_single_if() {
  var dial = parseInt(document.getElementById('dial_single_hz').value) || 0;
  var ifreq = parseInt(document.getElementById('if_single_hz').value) || 0;
  var sidetone = parseInt(document.getElementById('sidetone_single_hz').value) || 0;
  var usb = document.getElementById('usb_single_chk').checked;
  var upconvert = document.getElementById('upconvert_single_chk').checked;

  if (dial <= 0 || ifreq <= 0) {
    alert('Dial and IF must be positive numbers (Hz).');
    return;
  }

  // For single-IF, compute clocks differently:
  // CLK0 = LO = Dial +/- IF (depends on upconvert)
  var lo = upconvert ? (dial + ifreq) : (dial - ifreq);
  if (lo <= 0) {
    alert('Calculated LO is non-positive. Check Dial/IF and Upconvert settings.');
    return;
  }

  // CLK1 = sidetone-anchored clock: use IF +/- sidetone (preserve USB/LSB choice)
  var clk1 = usb ? (ifreq + sidetone) : (ifreq - sidetone);

  var clk2 = dial;

  // Populate clock fields
  document.getElementById('clk0_hz').value = lo;
  document.getElementById('clk1_hz').value = clk1;
  document.getElementById('clk2_hz').value = clk2;

  console.log('Single-IF calculated clocks:', lo, clk1, clk2);
  set_clocks();
}

// Fetch and display input voltage from the device
function fetch_input_voltage() {
  // Use XHR directly to keep parity with http_request helper behavior
  var xhr = new XMLHttpRequest();
  xhr.open('GET', api_base_url + 'inputVoltage', true);
  xhr.onreadystatechange = function () {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        var val = parseFloat(xhr.responseText);
        if (!isNaN(val)) {
          document.getElementById('input_voltage').textContent = val.toFixed(2);
        } else {
          document.getElementById('input_voltage').textContent = '--';
        }
      } else {
        document.getElementById('input_voltage').textContent = '--';
      }
    }
  };
  xhr.send();
}

// Allow Enter to trigger set_freq when focus is in the freq field
document.addEventListener('DOMContentLoaded', function () {
  var freqInput = document.getElementById('freq_mhz');
  if (freqInput) {
    freqInput.addEventListener('keypress', function (event) {
      if (event.key === 'Enter') {
        event.preventDefault();
        var btn = document.getElementById('set_freq_btn');
        if (btn) btn.click();
      }
    });
  }
});
