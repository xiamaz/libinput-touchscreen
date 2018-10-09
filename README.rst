Libinput touchscreen gestures
-----------------------------

Small program listening to libinput touchscreen events and triggering commands.

Currently supports:

* edge-events
* multitouch taps and swipes (direction aware)
* link with arbitrary commands.

TODO:
* easier setup and calibration of screen
* more gestures
* visual indicators (will require overlays)

Non-goals:
* touch gesture filtering (requires integration into wayland compositor), but
  might be future project
