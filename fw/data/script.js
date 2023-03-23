var freq_a = 7.060;
var freq_b = 14.060;
var active_vfo = "a";
var band_1 = 7;
var band_2 = 14;
var interval = 0;
var keyer_rpt_delay = 25000;    // TODO - this is currently the total time between enqueues. Change to time btwn calls.
var repeat_flag = 0;
var vfo_digits = 5;
var keyer_mem_1 = "CQ SOTA DE KI6SYD K";
var keyer_mem_2 = "KI6SYD";
var keyer_mem_3 = "S2S";


function send_command(param, val) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/"+param+"?value="+val, true);
  xhr.send();

  console.log(param, ': ', val)
}


function update_af(val) {
  document.getElementById('af_gain').value = val; //update current slider value
  send_command("vol", val)
}


function update_keyer(val) {
  document.getElementById('keyer_speed_wpm').value = val; //update current slider value
  send_command("speed", val)
}


function band(val) {
  freq = parseFloat(document.getElementById('freq_mhz').value);
  if(Math.floor(freq) != val) {
    document.getElementById('freq_mhz').value = val
  }

  set_freq()
}


function set_freq() {
  if(active_vfo == "a") {
    freq_a = parseFloat(document.getElementById('freq_mhz').value);
    document.getElementById('freq_mhz').value = freq_a.toFixed(vfo_digits);
    send_command("freq", freq_a)
  }
  else if(active_vfo == "b") {
    freq_b = parseFloat(document.getElementById('freq_mhz').value); 
    document.getElementById('freq_mhz').value = freq_b.toFixed(vfo_digits);
    send_command("freq", freq_b)
  }

  console.log('set')
}

function set_clocks() {
  clk0 = parseFloat(document.getElementById('clk0').value);
  clk1 = parseFloat(document.getElementById('clk1').value);
  clk2 = parseFloat(document.getElementById('clk2').value);

  clock_string = clk0 + "_" + clk1 + "_" + clk2

  send_command("clk", clock_string)

  console.log('setting clocks')
  console.log(clock_string)
}


function swap_vfo() {
  if(active_vfo == "a") { 
    active_vfo = "b";
    document.getElementById('vfo').value = 'VFO B';
    document.getElementById('freq_mhz').value = freq_b.toFixed(vfo_digits);
  }
  else if(active_vfo == "b") { 
    active_vfo = "a";
    document.getElementById('vfo').value = 'VFO A';
    document.getElementById('freq_mhz').value = freq_a.toFixed(vfo_digits);
  }

  set_freq();
}


function memory(num) {
  if(repeat_flag == 0) {
    if(num == 1)
      send_command("enqueue", keyer_mem_1)
    if(num == 2)
      send_command("enqueue", keyer_mem_2)
    if(num == 3)
      send_command("enqueue", keyer_mem_3)
  }
  else {
    if(num == 1) {
      send_command("enqueue", keyer_mem_1)
      interval = setInterval(function() { send_command("enqueue", keyer_mem_1);}, keyer_rpt_delay); 
    }
    if(num == 2) {
      send_command("enqueue", keyer_mem_2)
      interval = setInterval(function() { send_command("enqueue", keyer_mem_2);}, keyer_rpt_delay); 
    }
    if(num == 3) {
      send_command("enqueue", keyer_mem_3)
      interval = setInterval(function() { send_command("enqueue", keyer_mem_3);}, keyer_rpt_delay); 
    }
  }
}


function incr_decr(num) {
  console.log(num)
  document.getElementById('freq_mhz').value = parseFloat(document.getElementById('freq_mhz').value) + num

  set_freq()
}


function repeat() {
  if(repeat_flag == 0) {
    repeat_flag = 1;
    document.getElementById("rpt_button").style.backgroundColor='#888888';
  }
  else {
    repeat_flag = 0;
    document.getElementById("rpt_button").style.backgroundColor='#4d4c46';
    clearInterval(interval);
  }
  console.log(repeat_flag);
}

function bw(val) {
  send_command("bandwidth", val)
}

function special(val) {
  send_command("special", val)
}


function read_freeform() {
  var cur_char = document.getElementById('freeform').value
  // replace space with _, it goes into URL
  cur_char = cur_char.replace(" ", "_")

  document.getElementById('freeform').value = ""
  send_command("enqueue", cur_char)
}



function get_s_meter() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('s_meter').value = this.responseText;
    }
  };
  xhttp.open("GET", "s", true);
  xhttp.send();
}


function get_v_bat() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('v_bat').value = this.responseText;
    }
  };
  xhttp.open("GET", "vbat", true);
  xhttp.send();
}

function get_debug() {
  // debug 1
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('dbg_1').value = this.responseText;
    }
  };
  xhttp.open("GET", "dbg1", true);
  xhttp.send();

  // debug 2
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('dbg_2').value = this.responseText;
    }
  };
  xhttp.open("GET", "dbg2", true);
  xhttp.send();
}

// add listener, and a function for enqueueing/deleting characters
document.getElementById('freeform').addEventListener('input', read_freeform);

// add listener to VFO text area, so enter press is equivalent to "SET" button
document.getElementById('freq_mhz').addEventListener('keypress', function(event) {
  // If the user presses the "Enter" key on the keyboard
  if (event.key === 'Enter') {
    // Cancel the default action, if needed
    event.preventDefault();
    // Trigger the button element with a click
    document.getElementById('set_freq_btn').click();
  }
});

// refresh every 2 seconds
setInterval(function() { get_s_meter();}, 1000); 
setInterval(function() { get_v_bat();}, 1000); 
setInterval(function() { get_debug();}, 500); 