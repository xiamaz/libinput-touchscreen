#include "libinput-touchscreen.h"
#include "calibration.h"
#include "list.h"

#include <poll.h>
#include <stdio.h>

movement read_screen_dimensions(const char *dimfile) {
	FILE *dfile;
	char buffer[128];
	char *b;
	struct movement screen;

	printf("Loading screen calibration from %s\n", dimfile);

	dfile = fopen(dimfile, "re");
	fgets(buffer, 128, dfile);
	b = buffer;
	screen.start.x = strtod(b, &b);
	screen.end.x = strtod(b, &b);
	fgets(buffer, 128, dfile);
	b = buffer;
	screen.start.y = strtod(b, &b);
	screen.end.y = strtod(b, &b);
	fclose(dfile);
#if LOGGING
	printf("Screen: <%lf %lf> <%lf %lf>", screen.start.x, screen.end.x, screen.start.y, screen.end.y);
#endif
	return screen;
}


movement calibrate_touchscreen(const char *devpath, const char *dimfile) {
	struct movement screen = {{0}};
	struct libinput *li = create_libinput_device_interface(devpath);
	if (li == NULL) {
		return screen;
	}
	clear_event_pipe(li);

	struct movement movements[10] = {{{0}}};
	struct libinput_event *event;
	struct pollfd fds;
	list *ready;
	fds.fd = libinput_get_fd(li);
	fds.events = POLLIN;
	fds.revents = 0;

	enum DIRECTION calibration_stage = DIR_TOP;
	printf("Start touchscreen calibration. Please start with top edge.\n");
	double calibration_buffer[CALIBRATION_NUM] = {0.0};
	size_t cal = 0;
	while (poll(&fds, 1, -1) > -1) {
		libinput_dispatch(li);
		while ((event = libinput_get_event(li)) != NULL) {
			// handle the event here
			handle_event(event, movements);
			libinput_event_destroy(event);
			libinput_dispatch(li);
		}
		ready = get_ready_movements(movements);
		if (list_len(ready) > 1) {
			printf("Please only use one finger\n");
		} else {
			node *cur = ready->head;
			movement *m;
			while (cur != NULL) {
				m = movements + *((size_t *)cur->value);
				switch(calibration_stage) {
				case DIR_LEFT:
				case DIR_RIGHT:
					calibration_buffer[cal++] = m->start.x;
					break;
				case DIR_TOP:
				case DIR_BOT:
					calibration_buffer[cal++] = m->start.y;
					break;
				default:
					break;
				}
				printf("Registered event\n");
				cur = cur->next;
			}
		}
		list_destroy(ready);
		if (cal == CALIBRATION_NUM) {
			switch(calibration_stage++) {
			case DIR_TOP:
				screen.start.y = calibration_buffer[0];
				for (int i = 0; i < CALIBRATION_NUM; i++) {
					if (screen.start.y > calibration_buffer[i]) {
						screen.start.y = calibration_buffer[i];
					}
				}
				break;
			case DIR_BOT:
				screen.end.y = calibration_buffer[0];
				for (int i = 0; i < CALIBRATION_NUM; i++) {
					if (screen.end.y < calibration_buffer[i]) {
						screen.end.y = calibration_buffer[i];
					}
				}
				break;
			case DIR_LEFT:
				screen.start.x = calibration_buffer[0];
				for (int i = 0; i < CALIBRATION_NUM; i++) {
					if (screen.start.x > calibration_buffer[i]) {
						screen.start.x = calibration_buffer[i];
					}
				}
				break;
			case DIR_RIGHT:
				screen.end.x = calibration_buffer[0];
				for (int i = 0; i < CALIBRATION_NUM; i++) {
					if (screen.end.x < calibration_buffer[i]) {
						screen.end.x = calibration_buffer[i];
					}
				}
				break;
			case DIR_NONE:
				break;
			}
			cal = 0;
			switch(calibration_stage) {
			case DIR_TOP:
				printf("Top edge\n");
				break;
			case DIR_BOT:
				printf("Bottom edge\n");
				break;
			case DIR_LEFT:
				printf("Left edge\n");
				break;
			case DIR_RIGHT:
				printf("Right edge\n");
				break;
			case DIR_NONE:
				printf("Finished calibrating. Saving to %s\n", dimfile);
				break;
			}
		}
		if (calibration_stage == DIR_NONE) {
			break;
		}
	}
#if LOGGING
	printf("Screen: <%lf %lf> <%lf %lf>", screen.start.x, screen.end.x, screen.start.y, screen.end.y);
#endif
	FILE *f = fopen(dimfile, "we");
	if (f == NULL) {
		printf("Error opening dim file\n");
		return screen;
	}
	fprintf(f, "%lf %lf  # X dimensions\n", screen.start.x, screen.end.x);
	fprintf(f, "%lf %lf  # Y dimensions\n", screen.start.y, screen.end.y);
	fclose(f);
	return screen;
}
