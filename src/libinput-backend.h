#ifndef LIBINPUT_BACKEND_H
#define LIBINPUT_BACKEND_H
#include <libinput.h>
#include <libudev.h>

// Remove all pending events from queue
void clear_event_pipe(struct libinput *li);

// Create a libinput device interface
struct libinput *create_libinput_device_interface(const char *devpath);

// Find a touchscreen device and return the devicepath
char *find_touch_device();
#endif
