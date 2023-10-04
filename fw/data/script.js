var freq = 0.0;
var wd_count_max = 15;
var wd_count = wd_count_max;
var queue_len = 0;
var vfo_digits = 4;
var keyer_mem = ["CQ SOTA DE KI6SYD K", "QRZ DE KI6SYD K"]
var rpt_flag = false;
var rpt_dly = 10000;
var rpt_timer = rpt_dly;
var rpt_count_step_ms = 500;
var mon_min = -2;
var mon_max = 2;

var api_base_url = "/api/v1/"
var sota_url_1 = "https://api2.sota.org.uk/api/spots/";
var sota_url_2 = "/all/"
var sotlas_url = "https://sotl.as/summits/"
var qrz_url = "https://www.qrz.com/db/"
var num_spots = 20;
var spot_fetch_errors = 0;

col_names = ['UTC', 'Call', 'Freq', 'Mode', 'Summit']

function zeroPad(num, places) {
  var zero = places - num.toString().length + 1;
  return Array(+(zero > 0 && zero)).join("0") + num;
}

// TODO: move these limits to top of file
function freq_in_band(num) {
  if(num >= 7 && num <= 7.3)
    return true;
  if(num >= 14.0 && num <= 14.35)
    return true;
  if(num >= 21 && num <= 21.45)
    return true;
  return false;
}

function get_utc_time() {
  const d = new Date();
  var time = zeroPad(d.getUTCHours(), 2) + ":" + zeroPad(d.getUTCMinutes(), 2);
  document.getElementById('time').value = time;
}

function http_request(type, path, keys, values) {
  // form the string to send
  var str = api_base_url + path + "?";
  for(var i = 0; i < keys.length; i++) {
    str += keys[i] + "=" + values[i] + "&";
  }
  str = str.substring(0, str.length - 1);

  var xhr = new XMLHttpRequest();
  xhr.open(type, str, true);
  // add a callback if the type is GET
  if(type == "GET") {
    xhr.onreadystatechange = arguments[4]
  }
  else {
    // add a callback to PUT/POST if we passed one
    if(arguments.length == 5)
      xhr.onreadystatechange = arguments[4]
  }
  xhr.send();
}

function send_ft8() {
  var ft8String = document.getElementById('ft8String').value;
  var timeNow = Date.now();
  var rf = 14074000;
  var af = parseInt(document.getElementById('ft8AudioFreq').value);

  http_request("POST", "ft8", ["messageText", "timeNow", "rfFrequency", "audioFrequency"], [ft8String, timeNow, rf, af])
}

function send_wspr() {
  var call = document.getElementById('wsprCall').value;
  var timeNow = Date.now();
  var rf = 14095600;
  var af = parseInt(document.getElementById('wsprFreq').value);
  var grid = document.getElementById('wsprGrid').value;
  var power = parseInt(document.getElementById('wsprPower').value);

  http_request("POST", "wspr", ["callSign", "timeNow", "rfFrequency", "audioFrequency", "gridSquare", "power"], [call, timeNow, rf, af, grid, power])
}


function send_cw() {
  var cur_char = document.getElementById('freeform').value
  // replace space with _, it goes into URL
  cur_char = cur_char.replace(" ", "_")

  document.getElementById('freeform').value = ""

  http_request("POST", "cw", ["messageText"], [cur_char])
}

function get_queue_len() {
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      queue_len = parseInt(this.responseText)
    }
  };
  http_request("GET", "queue", [], [], func)
}

function clear_queue() {
  http_request("DELETE", "queue", [], [])
}

function set_epoch_ms() {
  const time_now = Date.now();
  http_request("PUT", "time", ["timeNow"], [time_now])
}


function set_freq() {
  freq = parseFloat(document.getElementById('freq_mhz').value) * 1e6;
  http_request("PUT", "frequency", ["frequency"], [freq])

  // request freq back as a way to update UI fast
  get_freq();
}


function get_freq() {
  // don't update if the user is typing in this text box
  if(document.activeElement.id == 'freq_mhz')
    return;

  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      var response = parseFloat(this.responseText)/1e6;
      response = response.toFixed(vfo_digits)
      document.getElementById('freq_mhz').value = response

      if(freq_in_band(response))
        document.getElementById('freq_mhz').className = 'vfo_display'
      else
        document.getElementById('freq_mhz').className = 'vfo_display_invalid'
    }
  };
  http_request("GET", "frequency", [], [], func)
}

function adj_freq(change) {
  document.getElementById('freq_mhz').value = parseFloat(document.getElementById('freq_mhz').value) + change

  set_freq()
  get_freq()
}

function next_band() {
  var cur_freq = parseFloat(document.getElementById('freq_mhz').value);
  var round_down_freq = Math.round(cur_freq);

  // todo: clean this up. parametrize it
  if(round_down_freq == 7)
    document.getElementById('freq_mhz').value = cur_freq + (14.0-7.0)
  if(round_down_freq == 14)
    document.getElementById('freq_mhz').value = cur_freq + (21.0-14.0)
  if(round_down_freq == 21)
    document.getElementById('freq_mhz').value = cur_freq + (7.0-21.0)

  set_freq();
  get_freq();
}


function set_volume() {
  vol = document.getElementById('af_gain').value;
  http_request("PUT", "volume", ["audioLevel"], [vol])
}


function get_volume() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('af_gain').value = this.responseText;
    }
  };
  http_request("GET", "volume", [], [], func)
}

function adj_vol(change) {
  new_vol = parseInt(document.getElementById('af_gain').value) + change;
  document.getElementById('af_gain').value = new_vol

  // set and get volume to force update on both sides
  set_volume();
  get_volume();
}



function set_sidetone() {
  sidetone = document.getElementById('mon').value;
  http_request("PUT", "sidetone", ["sidetoneLevelOffset"], [sidetone])
}

function get_sidetone() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('mon').value = this.responseText;
    }
  };
  http_request("GET", "sidetone", [], [], func)
}

function press_mon() {
  var mon = parseInt(document.getElementById('mon').value);
  if(mon < mon_max)
    mon = mon + 1;
  else
    mon = mon_min;

  document.getElementById('mon').value = mon;

  set_sidetone();
  get_sidetone();
}


function set_speed() {
  speed = document.getElementById('keyer_speed').value;
  http_request("PUT", "cwSpeed", ["speed"], [speed])
}

function get_speed() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('keyer_speed').value = this.responseText;
    }
  };
  http_request("GET", "cwSpeed", [], [], func)
}

function adj_speed(change) {
  var new_speed = parseInt(document.getElementById('keyer_speed').value) + change;

  document.getElementById('keyer_speed').value = new_speed;

  set_speed();
  get_speed();
}


function set_bw() {
  bw = document.getElementById('bw').value;
  http_request("PUT", "rxBandwidth", ["bw"], [bw])
}

function get_bw() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('bw').value = this.responseText;
    }
  };
  http_request("GET", "rxBandwidth", [], [], func)
}

function press_bw() {
  var bw = document.getElementById('bw').value;

  if(bw == "CW")
    bw = "SSB"
  else if(bw == "SSB")
    bw = "CW"
  else
    return;

  document.getElementById('bw').value = bw;

  set_bw();
  get_bw();
}

function set_lna() {
  lna = document.getElementById('lna').value;
  http_request("PUT", "lna", ["lnaState"], [lna])
}

function get_lna() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('lna').value = this.responseText;
    }
  };
  http_request("GET", "lna", [], [], func)
}

function press_lna() {
  var lna = document.getElementById('lna').value;

  if(lna == "ON")
    lna = "OFF"
  else if(lna == "OFF")
    lna = "ON"
  else
    return;

  document.getElementById('lna').value = lna;

  set_lna();
  get_lna();
}

function set_antenna() {
  antenna = document.getElementById('ant').value;
  http_request("PUT", "antenna", ["antennaPath"], [antenna])
}

function get_antenna() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('ant').value = this.responseText;
    }
  };
  http_request("GET", "antenna", [], [], func)
}

function press_ant() {
  var antenna = document.getElementById('ant').value;

  if(antenna == "DIRECT")
    antenna = "EFHW";
  else if(antenna == "EFHW")
    antenna = "DIRECT";
  else {
    get_antenna();
    return;
  }

  document.getElementById('ant').value = antenna;

  set_antenna();
  get_antenna();
}


function set_speaker() {
  speaker = document.getElementById('speaker').value;
  http_request("PUT", "speaker", ["speakerState"], [speaker])
}

function get_speaker() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('speaker').value = this.responseText;
    }
  };
  http_request("GET", "speaker", [], [], func)
}

function press_speaker() {
  var speaker = document.getElementById('speaker').value;
  if(speaker == 'ON')
    speaker = 'OFF'
  else
    speaker = 'ON'

  document.getElementById('speaker').value = speaker;

  set_speaker();
  get_speaker();
}


function get_input_voltage() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('v_bat').value = this.responseText;
    }
  };
  http_request("GET", "inputVoltage", [], [], func)
}

function get_s_meter() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('s_meter').value = this.responseText;

      console.log(this.responseText)

      // do heartbeat update in this function, we look at S meter often.
      wd_count = wd_count_max;
    }
  };
  http_request("GET", "sMeter", [], [], func)
}

function get_githash() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('githash').value = this.responseText;
    }
  };
  http_request("GET", "githash", [], [], func)
}

function get_address() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('address').value = this.responseText;
    }
  };
  http_request("GET", "address", [], [], func)
}

function debug_action(number) {
  http_request("POST", "debug", ["command"], [number])
}

function self_test(test_name) {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 201) {
      var json_response = JSON.parse(this.responseText);
      console.log(json_response)

      plot_dataset(json_response["data"], "my_chart", 'linear')
    }
  };
	http_request("POST", "selfTest", ["testName"], [test_name], func)
}

function sample() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 201) {
      var json_response = JSON.parse(this.responseText);
      console.log(json_response)

      plot_dataset(json_response["data"], "my_chart", 'linear')
    }
  };
  http_request("POST", "rawSamples", [], [], func)
}



function get_debug() {
  // define callback function
  func = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('dbg').value = this.responseText;

      // do heartbeat update in this function, we look at S meter often.
      wd_count = wd_count_max;
    }
  };
  http_request("GET", "debug", [], [], func)
}


// looks for the minimum value for the given key name
function min_from_json_data(json_data, key_name) {
  // set minimum to first value
  var first_obj = json_data[0]
  var min = first_obj[key_name]

  // iterate
  for(var i = 0; i < json_data.length; i++) {
    var obj = json_data[i];
    if(obj[key_name] < min)
      min = obj[key_name]
  }

  console.log(min)

  return min;
}

function max_from_json_data(json_data, key_name) {
  // set minimum to first value
  var first_obj = json_data[0]
  var max = first_obj[key_name]

  // iterate
  for(var i = 0; i < json_data.length; i++) {
    var obj = json_data[i];
    if(obj[key_name] > max)
      max = obj[key_name]
  }

  console.log(max)

  return max;
}


function plot_dataset(json_data, chart_name, y_axis_type) {

  var x_min = min_from_json_data(json_data, 'x') / 1.1
  var y_min = min_from_json_data(json_data, 'y') / 1.1
  var x_max = max_from_json_data(json_data, 'x') * 1.1
  var y_max = max_from_json_data(json_data, 'y') * 1.1

  new Chart("my_chart", {
    type: 'scatter',
    data: {
      datasets: [{
        pointRadius: 4,
        pointBackgroundColor: "rgb(0,0,255)",
        data: json_data
      }]
    },
    options: {
      legend: {
        display: false
      },
      scales: {
        x: {
          min: x_min,
          max: x_max
        },
        y: {
          min: y_min,
          max: y_max
        },
        yAxes: [{
          type: y_axis_type
        }]
      },
      events: []
    }
  });
}

// TODO - remove concept of multiple memories, keep CQ separate. This function will cancel repeat on CQ if you press mem.
function memory(num) {
  // force update on keyer queue length
  get_queue_len();

  // button press should cancel repeat and clear the queue, regardless of whether repeat is on
  if(queue_len > 0) {
    rpt_flag = false;
    http_request("DELETE", "queue", [], [])
    document.getElementById('cq').className = 'button'
  }
  // button press should add to queue when repeat mode is off
  else {
    // don't ask for a keyer message that doesn't exist
    if(num > keyer_mem.length)
      return;

    // send and update keyer length
    http_request("POST", "cw", ["messageText"], [keyer_mem[num]])
    get_queue_len();
  }
}



function repeat_update() {
  get_queue_len();

  // update timer if we are on repeat and the queue is empty
  if(rpt_flag && queue_len == 0)
    rpt_timer = rpt_timer - rpt_count_step_ms;

  // enqueue the CQ memory again if both of these are true:
  //    1. repeat flag is on
  //    2. repeat timer has expired
  if(rpt_flag && rpt_timer <= 0) {
    memory(0);

    // reset the repeat timer. will start decrementing when queue expires
    rpt_timer = rpt_dly;
  }
}


function watchdog_update() {
  wd_count = wd_count - 1;

  if(wd_count <= 0) {
    document.getElementById('address').className = 'readout_tiny_alert'
  }
  else {
    document.getElementById('address').className = 'readout_tiny'
  }
}

// convenient way for sotawatch links to set frequency
function jump_to_spot(freq, mode) {
  document.getElementById('freq_mhz').value = freq.toFixed(vfo_digits);
  set_freq()

  // TODO - fix this hack.
  var v = document.getElementById('bw').value
  if(mode.toUpperCase() != document.getElementById('bw').value.toUpperCase())
    press_bw();
}

function json_spots_to_table(num_spots, col_names, table_id) {

  var url_string = sota_url_1 + num_spots + sota_url_2;

  var container = document.getElementById(table_id);

  var table = document.createElement("table")

  var thead = document.createElement("thead");
  var tr = document.createElement("tr");

    // Loop through the column names and create header cells
   col_names.forEach((item) => {
      var th = document.createElement("th");
      th.innerText = item; // Set the column name as the text of the header cell
      tr.appendChild(th); // Append the header cell to the header row
   });
   thead.appendChild(tr); // Append the header row to the header
   table.append(tr) // Append the header to the table

  try {
    var req = new XMLHttpRequest(); // a new request
    req.open("GET", url_string, false);
    req.send(null);
    var json_obj = JSON.parse(req.responseText);
     
     // Loop through the JSON data and create table rows
     json_obj.forEach((item) => {
        var tr = document.createElement("tr");
        
        // Get the values of the current object in the JSON data
        var vals = Object.values(item);

        // get time
        var time = item.timeStamp.substring(11, 16);
        var td = document.createElement("td");
        td.innerText = time;
        tr.appendChild(td);

        // get activator
        td = document.createElement("td");
        var a = document.createElement("a")
        var a_text = document.createTextNode(item.activatorCallsign);
        var href_string = qrz_url + item.activatorCallsign;
        a.setAttribute('href', href_string);
        a.setAttribute('target', "_blank")
        a.setAttribute('rel', "noreferrer")
        a.appendChild(a_text)
        td.appendChild(a)
        tr.appendChild(td);

        // get frequency
        td = document.createElement("td");
        a = document.createElement("a")
        a_text = document.createTextNode(item.frequency);
        href_string = "javascript:jump_to_spot(" + item.frequency + ", \"" + item.mode + "\")";
        a.setAttribute('href', href_string);
        a.appendChild(a_text)
        td.appendChild(a)
        tr.appendChild(td);

        // get mode
        td = document.createElement("td");
        td.innerText = item.mode.toUpperCase();
        tr.appendChild(td);

        // get summit
        td = document.createElement("td");
        var summit_string = item.associationCode + "/" + item.summitCode;
        a = document.createElement("a")
        a_text = document.createTextNode(summit_string);
        href_string = sotlas_url + summit_string;
        a.setAttribute('href', href_string)
        a.setAttribute('target', "_blank")
        a.setAttribute('rel', "noreferrer")
        a.appendChild(a_text)
        td.appendChild(a)

        tr.appendChild(td);


        table.appendChild(tr);
        
     });

    // clear the container
    while (container.firstChild) {
      container.removeChild(container.firstChild);
    }

     container.appendChild(table) // Append the table to the container element

     spot_fetch_errors = 0;

  }
  catch {
    spot_fetch_errors += 1;

    // allow up to 5 failures to fetch spots before showing the failure
    if(spot_fetch_errors > 5) {
      // clear the container
      while (container.firstChild) {
        container.removeChild(container.firstChild);
      }
      var p = document.createElement("p");
      var p_text = document.createTextNode('Could not retrieve data!')
      p.appendChild(p_text)
      container.appendChild(p)
    }
  }

}

// load all parameters from radio
function on_load() {
  get_input_voltage();
  get_s_meter();
  get_volume();
  get_sidetone();
  get_speed();
  get_bw();
  get_lna();
  get_antenna();
  // get_debug();
  get_freq();
  get_utc_time();
  get_address();
  get_githash();
  set_epoch_ms();
  json_spots_to_table(num_spots, col_names, "spots");
}

// add listener, and a function for enqueueing/deleting characters
document.getElementById('freeform').addEventListener('input', send_cw);

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


// attach a long press listener to the CQ button for repeat
document.getElementById('cq').addEventListener('long-press', function(e) {
  e.preventDefault()
  document.getElementById('cq').className = 'button_pressed'
  rpt_flag = true;

  // call CQ
  memory(0);
});

// refresh variables
setInterval(function() { get_freq();}, 500);
setInterval(function() { get_input_voltage();}, 1500);
setInterval(function() { get_s_meter();}, 500);
setInterval(function() { get_volume();}, 5000); 
setInterval(function() { get_speed();}, 5000); 
setInterval(function() { get_bw();}, 3000); 
setInterval(function() { get_lna();}, 3000); 
setInterval(function() { get_antenna();}, 3000); 
setInterval(function() { get_sidetone();}, 3000); 
// setInterval(function() { get_queue_len();}, 2000); 
// setInterval(function() { get_debug();}, 500);
setInterval(function() { get_utc_time();}, 5000) 
setInterval(function() { set_epoch_ms();}, 60000)
// watchdog update runs slightly slower than s-meter
setInterval(function() { watchdog_update();}, 500)
// repeat logic runs every 500ms
setInterval(function() { repeat_update();}, rpt_count_step_ms)
// pull new SOTA spots every 10 seconds
setInterval(function() { json_spots_to_table(num_spots, col_names, "spots");}, 10000)
