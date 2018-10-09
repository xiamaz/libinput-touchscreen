#ifndef CALIBRATION_H
#define CALIBRATION_H
#include "libinput-touchscreen.h"
#define CALIBRATION_NUM 5  // number of required calibration attempts per side

movement read_screen_dimensions(const char *dimfile);
movement calibrate_touchscreen(const char *devpath, const char *dimfile);
#endif
