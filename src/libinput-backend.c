#include "libinput-backend.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>

static int open_restricted(const char *path, int flags, void *user_data) {
	int fd = open(path, flags);
	return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data) {
	close(fd);
}

const static struct libinput_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

char *get_devpath(struct libinput_device *dev) {
	struct udev_device *uddev = libinput_device_get_udev_device(dev);
	const char *origpath = udev_device_get_devnode(uddev);
	size_t plen = strlen(origpath);
	char *devpath = malloc((plen + 1) * sizeof *devpath);
	memcpy(devpath, origpath, plen);
	udev_device_unref(uddev);
	return devpath;
}

// Check event for device, if device is touchscreen set devpath and return true
bool check_is_touchscreen(struct libinput_event *ev, char **devpath) {
	bool is_touch = false;
	struct libinput_device *dev;
	if (libinput_event_get_type(ev) == LIBINPUT_EVENT_DEVICE_ADDED) {
		dev = libinput_event_get_device(ev);
		if (libinput_device_has_capability(dev,
						   LIBINPUT_DEVICE_CAP_TOUCH)) {
			*devpath = get_devpath(dev);
			is_touch = true;
		}
	}
	libinput_event_destroy(ev);
	return is_touch;
}

char *find_touch_device() {
	struct libinput *li;
	struct libinput_event *ev;
	char *devicepath = NULL;

	struct udev *ud;

	ud = udev_new();
	li = libinput_udev_create_context(&interface, NULL, ud);

	libinput_udev_assign_seat(li, "seat0");

	libinput_dispatch(li);
	while ((ev = libinput_get_event(li))) {
		if (check_is_touchscreen(ev, &devicepath)) {
			break;
		}
		libinput_dispatch(li);
	}

	udev_unref(ud);
	libinput_unref(li);
	return devicepath;
}

void clear_event_pipe(struct libinput *li) {
	struct libinput_event *event;
	// clear up event pipeline
	libinput_dispatch(li);
	while ((event = libinput_get_event(li)) != NULL) {
		libinput_event_destroy(event);
		libinput_dispatch(li);
	}
}

struct libinput *create_libinput_device_interface(const char *devpath) {
	struct libinput *li;
	struct libinput_device *dev;

	li = libinput_path_create_context(&interface, NULL);

	dev = libinput_path_add_device(li, devpath);
	if (dev == NULL) {
		printf("Error in device adding %s\n", devpath);
		return NULL;
	}
	return li;
}
