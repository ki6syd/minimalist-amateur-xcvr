var freq = 0.0;
var wd_count_max = 15;
var wd_count = wd_count_max;
var queue_len = 0;
var vfo_digits = 4;
var keyer_mem_1 = "CQ SOTA DE KI6SYD K";
var keyer_mem_2 = "QRZ DE KI6SYD K"
var cancel_char = "*"
var rpt_flag = false
var rpt_dly = 10000
var rpt_timer = rpt_dly


var sota_url_1 = "https://api2.sota.org.uk/api/spots/";
var sota_url_2 = "/all/"
var sotlas_url = "https://sotl.as/summits/"
var qrz_url = "https://www.qrz.com/db/"
var num_spots = 20;
var spot_fetch_errors = 0;

col_names = ['UTC', 'Call', 'Freq', 'Mode', 'Summit']


function GET(yourUrl){
    var Httpreq = new XMLHttpRequest(); // a new request
    Httpreq.open("GET",yourUrl,false);
    Httpreq.send(null);
    // console.log(Httpreq.responseText);
    return Httpreq.responseText;          
}


function zeroPad(num, places) {
  var zero = places - num.toString().length + 1;
  return Array(+(zero > 0 && zero)).join("0") + num;
}

function get_time() {
  const d = new Date();
  var time = zeroPad(d.getUTCHours(), 2) + ":" + zeroPad(d.getUTCMinutes(), 2);
  document.getElementById('time').value = time;
}


function send_command(param, val) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/"+param+"?value="+val, true);
  xhr.send();

  console.log(param, ': ', val)
}

function http_test() {
  var xhr = new XMLHttpRequest();
  xhr.open("POST", "/ft8?messageText=S%20ki6syd%2Ftce0", true);
  xhr.send();
}

function set_epoch_ms() {
  const time_now = Date.now();

  var xhr = new XMLHttpRequest();
  xhr.open("PUT", "/time?timeNow="+time_now)
  xhr.send();
}

function send_ft8() {
  var ft8String = document.getElementById('ft8String').value;
  var timeNow = Date.now();
  var rf = 14074000;
  var af = 1500;

  var tmp = '/ft8' + 
            '?messageText=' + ft8String + 
            '&timeNow=' + timeNow +
            '&rfFrequency' + rf +
            '&audioFrequency' + af;

  var xhr = new XMLHttpRequest();
  xhr.open("POST", tmp, true);
  xhr.send();
}

function stop_ft8() {
  var xhr = new XMLHttpRequest();
  xhr.open("DELETE", '/ft8', true);
  xhr.send();
}

function set_freq() {
  // send an update
  freq = parseFloat(document.getElementById('freq_mhz').value);
  // document.getElementById('freq_mhz').value = freq.toFixed(vfo_digits);
  send_command("set_freq", freq)

  // request freq back as a way to update
  get_freq();
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

// TODO: move these limits to top of file
function freq_in_band(num) {
  if(num >= 7 && num <= 7.3)
    return true;
  if(num >= 10.1 && num <= 10.15)
    return true;
  if(num >= 14 && num <= 14.35)
    return true;
  return false;
}

function get_freq() {
  console.log(document.activeElement.id);

  // don't update if the user is typing in this text box
  if(document.activeElement.id == 'freq_mhz')
    return;

  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
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
  xhttp.open("GET", "get_freq", true);
  xhttp.send();
}

function next_band() {
  var cur_freq = parseFloat(document.getElementById('freq_mhz').value);
  var round_down_freq = Math.round(cur_freq);

  // todo: clean this up.
  if(round_down_freq == 7)
    document.getElementById('freq_mhz').value = cur_freq + (10.11-7.06)
  if(round_down_freq == 10)
    document.getElementById('freq_mhz').value = cur_freq + (14.06-10.11)
  if(round_down_freq == 14)
    document.getElementById('freq_mhz').value = cur_freq + (7.06-14.06)

  set_freq();
}


function get_s_meter() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('s_meter').value = this.responseText;

      // do heartbeat update in this function, we look at S meter often.
      wd_count = wd_count_max;
    }
  };
  xhttp.open("GET", "get_s", true);
  xhttp.send();
}

function get_v_bat() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
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
      // document.getElementById('dbg').value = this.responseText;
      document.getElementById('dbg').value = document.activeElement.id;
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
      document.getElementById('lna').value = this.responseText;
    }
  };
  xhttp.open("GET", "get_lna", true);
  xhttp.send();
}


function special(val) {
  send_command("special", val)
}

// TODO - remove concept of multiple memories, keep CQ separate. This function will cancel repeat on CQ if you press mem.
function memory(num) {
  // button press should cancel repeat and clear the queue, regardless of whether repeat is on
  if(queue_len > 0) {
    console.log("canceling queue")
    rpt_flag = false;
    // enqueue a special character to clear the queue
    send_command("enqueue", cancel_char)
    document.getElementById('cq').className = 'button'
  }
  // button press should add to queue when repeat mode is off
  else {
    if(num == 1) {
      send_command("enqueue", keyer_mem_1)
      queue_len = keyer_mem_1.length
    }
    if(num == 2) {
      send_command("enqueue", keyer_mem_2)
      queue_len = keyer_mem_2.length
    }
  }
}

function press_mon() {
  // send command to increase
  send_command("press_mon", "")

  // pull new lna setting
  get_mon();
}

function get_mon() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById('mon').value = this.responseText;
    }
  };
  xhttp.open("GET", "get_mon", true);
  xhttp.send();
}

function get_queue_len() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      queue_len = this.responseText;
      console.log(queue_len)
    }
  };
  xhttp.open("GET", "get_queue_len", true);
  xhttp.send();
}

function repeat_update() {
  // update timer if we are on repeat and the queue is empty
  if(rpt_flag && queue_len == 0)
    rpt_timer = rpt_timer - 500;

  // enqueue the CQ memory again if both of these are true:
  //    1. repeat flag is on
  //    2. repeat timer has expired
  if(rpt_flag && rpt_timer <= 0) {
    send_command("enqueue", keyer_mem_1)

    // reset the repeat timer. will start decrementing when queue expires
    rpt_timer = rpt_dly;
  }
}

function incr_decr_freq(num) {
  document.getElementById('freq_mhz').value = parseFloat(document.getElementById('freq_mhz').value) + num

  set_freq()
}

function watchdog_update() {
  wd_count = wd_count - 1;

  if(wd_count <= 0) {
    document.getElementById('watchdog').value = "DISCONNECTED";
    document.getElementById('watchdog').className = 'readout_small_alert'
  }
  else {
    document.getElementById('watchdog').value = "CONNECTED";
    document.getElementById('watchdog').className = 'readout_small'
  }
}

function json_spots_to_table(num_spots, col_names, table_id) {
  
  // delete me!! TODO
  set_epoch_ms();

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
    var json_obj = JSON.parse(GET(url_string));
    console.log(json_obj);
     
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
  get_v_bat();
  get_s_meter();
  get_volume();
  get_speed();
  get_bw();
  get_lna();
  get_ant();
  get_mon();
  get_queue_len();
  get_debug();
  get_freq();
  get_time();
  json_spots_to_table(num_spots, col_names, "spots");
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


// attach a long press listener to the CQ button for repeat
document.getElementById('cq').addEventListener('long-press', function(e) {
  e.preventDefault()
  document.getElementById('cq').className = 'button_pressed'
  rpt_flag = true;
  // clear out the queue before adding to it
  send_command("enqueue", cancel_char)
  // call CQ
  send_command("enqueue", keyer_mem_1)
});

// refresh every few seconds
setInterval(function() { get_v_bat();}, 500);
setInterval(function() { get_s_meter();}, 250);
setInterval(function() { get_volume();}, 3000); 
setInterval(function() { get_speed();}, 5000); 
setInterval(function() { get_bw();}, 3000); 
setInterval(function() { get_lna();}, 3000); 
setInterval(function() { get_ant();}, 3000); 
setInterval(function() { get_mon();}, 3000); 
setInterval(function() { get_queue_len();}, 1000); 
setInterval(function() { get_debug();}, 500);
setInterval(function() { get_time();}, 5000) 
// watchdog update runs slightly slower than s-meter
setInterval(function() {watchdog_update();}, 300)
// repeat logic runs every 500ms
setInterval(function() {repeat_update();}, 500)
// don't do this repeatedly: makes frequency entry tough.
setInterval(function() { get_freq();}, 1000);
// pull new SOTA spots every 30 seconds
setInterval(function() {json_spots_to_table(num_spots, col_names, "spots");}, 10000)
//setInterval(function() {json_spots_to_table(num_spots, col_names, "spots");}, 30000)