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
  // placeholder kept for compatibility; page doesn't require any init
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
