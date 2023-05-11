var freq = 7.060;
// var interval = 0;
// var keyer_rpt_delay = 25000;    // TODO - this is currently the total time between enqueues. Change to time btwn calls.
// var repeat_flag = 0;
var vfo_digits = 5;
var keyer_mem_1 = "CQ SOTA DE KI6SYD K";
var keyer_mem_2 = "QRZ DE KI6SYD K"


function send_command(param, val) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/"+param+"?value="+val, true);
  xhr.send();

  console.log(param, ': ', val)
}

function set_freq() {
  // send an update
  freq = parseFloat(document.getElementById('freq_mhz').value);
  // document.getElementById('freq_mhz').value = freq.toFixed(vfo_digits);
  send_command("set_freq", freq)

  console.log('set')

  // request freq back as a way to update
  get_freq();
}

function get_freq() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var response = parseFloat(this.responseText)/1e6;
      console.log(response);
      document.getElementById('freq_mhz').value = response.toFixed(vfo_digits);
    }
  };
  xhttp.open("GET", "get_freq", true);
  xhttp.send();
}


function get_s_meter() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('s_meter').value = this.responseText;
    }
  };
  xhttp.open("GET", "get_s", true);
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
  xhttp.open("GET", "get_vbat", true);
  xhttp.send();
}

function get_debug() {
  // debug 1
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('dbg').value = this.responseText;
    }
  };
  xhttp.open("GET", "get_debug", true);
  xhttp.send();
}

function incr_vol() {
  // send command to increase
  send_command("incr_vol", "")

  // pull new volume
  get_volume();
}

function decr_vol() {
  // send command to increase
  send_command("decr_vol", "")

  // pull new volume
  get_volume();
}

function get_volume() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('af_gain').value = this.responseText;
    }
  };
  xhttp.open("GET", "get_vol", true);
  xhttp.send();
}

function read_freeform() {
  var cur_char = document.getElementById('freeform').value
  // replace space with _, it goes into URL
  cur_char = cur_char.replace(" ", "_")

  document.getElementById('freeform').value = ""
  send_command("enqueue", cur_char)
}

function incr_speed() {
  // send command to increase
  send_command("incr_speed", "")

  // pull new speed
  get_speed();
}

function decr_speed() {
  // send command to increase
  send_command("decr_speed", "")

  // pull new speed
  get_speed();
}

function get_speed() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('keyer_speed').value = this.responseText;
    }
  };
  xhttp.open("GET", "get_speed", true);
  xhttp.send();
}

function press_bw() {
  // send command to increase
  send_command("press_bw", "")

  // pull new bandwidth
  get_bw();
}

function get_bw() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('bw').value = this.responseText;
    }
  };
  xhttp.open("GET", "get_bw", true);
  xhttp.send();
}


function press_ant() {
  // send command to increase
  send_command("press_ant", "")

  // pull new antenna setting
  get_ant();
}

function get_ant() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('ant').value = this.responseText;
    }
  };
  xhttp.open("GET", "get_ant", true);
  xhttp.send();
}

function press_lna() {
  // send command to increase
  send_command("press_lna", "")

  // pull new lna setting
  get_lna();
}

function get_lna() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      document.getElementById('lna').value = this.responseText;
    }
  };
  xhttp.open("GET", "get_lna", true);
  xhttp.send();
}

function special(val) {
  send_command("special", val)
}


function memory(num) {
  send_command("enqueue", keyer_mem)
}

function memory(num) {
  if(num == 1)
    send_command("enqueue", keyer_mem_1)
  if(num == 2)
    send_command("enqueue", keyer_mem_2)
}

function incr_decr_freq(num) {
  get_freq()

  console.log(num)
  document.getElementById('freq_mhz').value = parseFloat(document.getElementById('freq_mhz').value) + num

  set_freq()
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
setInterval(function() { get_freq();}, 1000);_