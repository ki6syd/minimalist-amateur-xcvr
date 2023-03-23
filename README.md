# minimalist-amateur-xcvr

## Overview

The `MAX-3B` (**M**inimalist **A**mateur **X**cvr, **3** **B**and) is a bare bones QRP HF radio designed with the Summits On The Air (SOTA) operator in mind. There are a few goals for the project:

* Your phone's web browser is the screen! This gives a large, bright, highly-customizable screen. Send CW by typing on your screen.
* Power from USB-PD or a 6-15V DC source. RX capability with 5V from USB.
* Coverage of three HF bands (20m, 30m, and 40m tested)
* CW TX / RX, plus SSB RX for cross-mode QSOs.
* Where possible, multiple options for integrated circuits are supported. The design aims to be buildable in today's supply-constrained market.
* Similar BOM cost to LNR Mountain Topper radios.

## Technical Details

### Receiver

* Bandpass filter for each band, switched in with relays. QRP Labs bandpass modules are the starting point for this design.
* Single conversion superheterodyne architecture with 10 MHz IF
* IF bandwidth is ~3kHz for SSB reception
* ~500 Hz audio filter can be switched in for CW reception. Filter is bypassed for SSB reception.
* TBD dB unwanted sideband rejection (CW mode)
* Limiting and variable gain in AF chain for coarse control over headphone volume
* Drives normal computer earbuds. PCB has placeholder footprint for 1W speaker driver

### Transmitter

* Class E amplifier with 4x 2N7002 surface mount MOSFETs
* 5W @ 15V input on both bands
* TX harmonics at least 43dB below fundamental (per Section 97.303: https://www.ecfr.gov/current/title-47/chapter-I/subchapter-D/part-97/subpart-D/section-97.307)
* Key shaping circuit
* Low pass filter for each band, switched in with relays
* Semi-QSK
* SMA connector for RF output

### Power

* USB-C connector
    * USB-PD negotiation for 9-15V, and >=1.5A
    * If USB-PD negotiation fails, 5V supports RX and reprogramming
* 5.5 x 2.1mm barrel jack for 6-15V DC input
* TX: TBD mA @ 12V
* RX: TBD mA @ 12V

### ESP8266 and Peripherals

* The ESP8266 System on a Chip (SoC) can connect to an existing WiFi network so you can operate at home. If your home network is not found, the module will create its own WiFi network that your phone can join
* Onboard USB-serial converter for USB COM port. Used for programming, and can support a CAT interface (if implemented in software)
* LED for status indication

## Programming

### Arduino Board and Library Setup

* Install ESP8266 board (version 3.0.2): https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/
* Install ESP8266 File System uploader: https://randomnerdtutorials.com/install-esp8266-filesystem-uploader-arduino-ide/
* Settings
    * ![Screenshot at Aug 21 16-03-29](https://user-images.githubusercontent.com/5254153/185814808-e073ff35-fef8-4b9d-9403-3021a54b2171.png)
* Install ArduinoJson (version 6.19.3, by Benoit Blanchon)
* Install PCF8574 library (version 0.3.5, by Rob Tillaart)
* Install Etherkit Si5351 library (version 2.1.4, by Jason Milldrum)
* Create your credentials file (not included in the repository for privacy reasons)
    * Create a new file: fw/data/credentials.json
    * Add usernames and passwords in JSON format:
    <img width="347" alt="credentials" src="https://user-images.githubusercontent.com/5254153/185817286-cd13be90-a589-4186-8802-c14e2b2e26fa.png">

### Architecture

The radio front panel and (most) hardware details are loaded into a file structure created inside the ESP8266 flash chip. File system changes are possible by uploading new SPIFFS data through the Arduino IDE, or by uploading/editing new files through the Ace.js web interface (accessible with: admin/password).

* Hardware configuration parameters are stored inside `hardware_config.json`.
* WiFi credentials are stored inside `credentials.json`.
* Radio front panel layout stored in `index.html`.
* Radio front panel default values stored in `script.js` 
* Pinout-specific configurations are stored in the Arduino project.


## New Features

The following features are not implemented, but should be possible with existing hardware:
* WSPR beacon
* Digital modes
* CAT interface
* CW decoder
* RIT
* Split operation
* SOTA Watch integration (when cell service is available). Jump to frequency of a recent spot for S2S, post your own spot, and respot others.
* Adjustable sidetone level
* Oscillator frequency compensation
* IF filter frequency correction
* IF filter sweep
* BPF filter sweep
* AF filter sweep


## FAQ

### Does this only work with iPhones? 

Though the physical device is designed and tested with an iPhone 12 Mini, any web browser can act as the front panel for the `MAX-3B`. Collaboration on this project is appreciated; please get in touch with me if you would like to share a case design for a different phone, or add support for another screen size

### How well does this radio work?

"Good enough." The receiver is based on the SA612 mixer, which has relatively poor large signal performance. (See: https://studyres.com/doc/7790754/pa1dsp---why-not-to-use-the-ne602). Robust band-pass filters are included to avoid interaction with out-of-band signals, and otherwise performance is generally acceptable in remote operating locations (such as a SOTA peak). This is a similar receiver architecture to the LNR Mountain Topper series, which is widely regarded as suitablly high performance for SOTA.

The IF crystal filter is wider than most CW only radios in order to support SSB reception and minimize ringing from the crystal filter. The baseband audio bandpass filter provides ~500Hz bandwidth when CW reception is selected.


### Why use T37-2 toroids even in the 20m band sections, where Type 6 is more appropriate?

Performance is fine with Type 2, and it was simpler to buy a large bag of a single type of toroid.

### How accurate is the freqency?

TBD. 

The same 26MHz crystal part number is used for both the ESP8266 and Si5351 clock generator. The Si5351 configuration code uses the crystal frequency defined in the JSON file, and the IF filter frequency is also defined here.

### Can I modify this project? Can we collaborate?

TBD license.

Collaboration is appreciated on this project. I'm not a software engineer or web developer, so you are welcome to develop new features, improve the quality of the code, and make suggestions. Please contact me if you would like to work together on this.

Please consider working with me on the existing repository as opposed to forking it into a new project!

### I found a bug. What can I do?

Please file a bug on Github or send an email to ki6syd@gmail.com!

### How can I get one?

Currently, the `MAX-3B` is not for sale, and I am not able to support production runs of new hardware. I have a few boards set aside for people who commit significant time to field testing or software development.

### Can I have access to the raw schematic and layout files?

A schematic PDF is provided for debug and educational purposes, but the raw files (created in Altium's Circuit Maker) are not public. I would like to disincentivize the proliferation of derivative commercial products (as happened with the uSDX project). Please send an email to ki6syd@gmail.com if you would like to work together closely on the hardware.

### FCC Part 15

This project should not be considered a "product." The board is a useful subcomponent of a larger radio system that a builder can design, and once fully integrated can be one of the builder's home-built devices allowed under section 15.23: https://www.ecfr.gov/current/title-47/chapter-I/subchapter-A/part-15/subpart-A/section-15.23. 

The WiFi section of the board is not implemented with a FCC-certified module, so operation of this device should be in accordance with ham radio best practices and the ARRL band plan. No encryption should be used, identification should be done via a callsign in the SSID, and operation should not cause interference for other users of the spectrum.

Sparkfun electronics has a brief intro to topics like this: https://www.sparkfun.com/tutorials/398

### Connecting to internet while using Access Point

In cases where a cell network is available and might be useful for sending messages or looking at SOTAwatch, the `MAX-3B`'s WiFi network should be used for accessing the radio UI and the cell network should be used for everything else. Some phones (e.g. iPhones) will attempt to use the WiFi network for everything by default - assigning a static IP to the `MAX-3B` WiFi connection with *no gateway* will prevent this network from being used for all traffic. See below for iPhone setup.

![image](https://user-images.githubusercontent.com/5254153/196582879-0b61091f-078d-4294-a5e9-4ddce61f610a.png)


