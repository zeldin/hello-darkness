# Hello Darkness
A functional custom firmware for the DasKeyboard 5Q

# Features
* All keys except for the brightness button generate HID events
* The Q-knob generates Dial HID events when turned
* Keys light up as you press them
* Brightness can be set by holding the brightness button and turning
  the Q-knob
* Not typing anything for 10 minutes results in a pause animation
  staring
* Holding down F12 when plugging in the keyboard puts the keyboard
  into DFU mode, so that the firmware can be upgraded

# Non-features
* No host control over LED function (definitiely no cloud!)
* No color schemes, the keys remain black until you press them or the
  pause animation starts

# See also
[5Q-info](https://github.com/diefarbe/5Q-info/tree/master/hello_world) -
a collection of reverse engineering information about the 5Q
