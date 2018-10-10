#include "libinput-touchscreen.h"
#include "calibration.h"
#include "configuration.h"
#include "list.h"

#include <poll.h>
#include <wordexp.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void print_timedelta(uint32_t timedelta) {
	printf("Time %ds\n", timedelta);
}

void print_gesture(gesture *g) {
	printf("G(%d) T%d D%d\n", g->num, g->type, g->dir);
}

int argmax(const size_t *arr, size_t len) {
	size_t highest = 0;
	int index = 0;
	for (size_t i = 0; i < len; i++) {
		if (arr[i] > highest) {
			highest = arr[i];
			index = i;
		} else if (arr[i] == highest) {
			index = -1;
		}
	}
	return index;
}

enum DIRECTION movement_direction(movement *m, list *ready) {
	enum DIRECTION dir = 0;
	size_t enum_votes[5] = {0}, i = 0;
	// collect individually transformed directions
	node *cur = ready->head;
	while (cur != NULL) {
		i = *((size_t *)cur->value);
		enum_votes[angle_to_direction(movement_angle(m + i))]++;
		cur = cur->next;
	}
	// check if multiple directions had the same maximum vote
	if ((dir = argmax(enum_votes, 5)) < 0) {
		dir = DIR_NONE;
	}
	return dir;
}

enum DIRECTION movement_border_direction(movement *m, list *ready, movement *screen) {
	movement *cm;
	vec2 startvec;
	node *cur = ready->head;
	logger("Movement border dir: start\n");
	while (cur != NULL) {
		cm = (m + *((size_t *)cur->value));
		startvec = cm->start;
		if (movement_length(cm) >= MIN_EDGE_DISTANCE) {
			if (startvec.x <= screen->start.x) {
				return DIR_LEFT;
			}
			if (startvec.x >= screen->end.x) {
				return DIR_RIGHT;
			}
			if (startvec.y <= screen->start.y) {
				return DIR_TOP;
			}
			if (startvec.y >= screen->end.y) {
				return DIR_BOT;
			}
		}
		cur = cur->next;
	}
	logger("Movement border dir: end\n");
	return DIR_NONE;
}

gesture get_gesture(movement *m, movement *screen, list *ready) {
	logger("Get gesture: begin\n");
	gesture g = {0};
	g.num = list_len(ready);
	logger("Get gesture: list len %d\n", g.num);
	g.dir = movement_direction(m, ready);
	enum DIRECTION border_dir;
	logger("Get gesture: got dir\n");
	if (g.dir == DIR_NONE) {
		g.type = GT_TAP;
	} else if (g.num > 1) {
		g.type = GT_MOVEMENT;
	} else if ((border_dir = movement_border_direction(m, ready, screen)) != DIR_NONE) {
		g.type = GT_BORDER;
		g.dir = border_dir;
	}
	logger("Get gesture: end\n");
	return g;
}

void trigger_rules(gesture *g, list *rules) {
	node *cur = rules->head;
	rule *r;
	while (cur != NULL) {
		r = (rule *)cur->value;
		if ((g->dir == r->key.dir)&&(g->type == r->key.type)&&(g->num == r->key.num)){
			printf("Trigger %s\n", r->command);
			system(r->command);
			return;
		}
		cur = cur->next;
	}
}

void handle_movements(movement *m, movement *screen, list *rules) {
	// skip if some fingers are still on the screen
	if (any_down(m)) {
		logger("SKIP handle movements: any down\n");
		return;
	}
	list *ready = get_ready_movements(m);
	if (ready == NULL) {
		logger("SKIP handle movements: none ready\n");
		return;
	}
	logger("Handle movements: begin\n");
	gesture g = get_gesture(m, screen, ready);
	logger("Handle movements: got gesture\n");
	print_gesture(&g);

	trigger_rules(&g, rules);

	list_destroy(ready);
	logger("Handle movements: end\n");
}

void get_movements(struct libinput *li, struct movement *screen, list *rules) {
	// movements have pointer structs inside
	struct movement movements[10] = {{{0}}};
	struct libinput_event *event;
	struct pollfd fds;
	fds.fd = libinput_get_fd(li);
	fds.events = POLLIN;
	fds.revents = 0;

	while (poll(&fds, 1, -1) > -1) {
		logger("Start poll cycle\n");
		libinput_dispatch(li);
		while ((event = libinput_get_event(li)) != NULL) {
			// handle the event here
			handle_event(event, movements);
			libinput_event_destroy(event);
			libinput_dispatch(li);
		}
		handle_movements(movements, screen, rules);
		logger("End poll cycle\n");
	}
}

int get_device_event_loop(const char *devpath, const char *rulespath, const char *calibpath) {
	struct movement screen;
	if (access(calibpath, F_OK) != -1) {
		screen = read_screen_dimensions(calibpath);
	} else {
		screen = calibrate_touchscreen(devpath, calibpath);
	}

	// load rules
	list *rules = load_rules(rulespath);

	// node *cur = rules->head;
	// while (cur != NULL) {
	// 	print_gesture(&((rule *)cur->value)->key);
	// 	printf("Command: %s\n", ((rule *)cur->value)->command);
	// 	cur = cur->next;
	// }
	// return 0;

	struct libinput *li = create_libinput_device_interface(devpath);
	if (li == NULL) {
		return -1;
	}
	clear_event_pipe(li);

	get_movements(li, &screen, rules);

	list_destroy(rules);
	libinput_unref(li);
	return 0;
}

#define PROGNAME "libinput-touchscreen"

char *get_conf_path(const char *filename) {
	const char *config_frag = "/.config/";
	char *homedir = getenv("HOME");
	size_t lconfdir = strlen(homedir) + strlen(config_frag) + strlen(PROGNAME) + 1 + strlen(filename) + 1;
	char *confdir = calloc(lconfdir, sizeof *confdir);
	memcpy(confdir, homedir, strlen(homedir));
	strncat(confdir, config_frag, lconfdir);
	strncat(confdir, PROGNAME, lconfdir);
	strncat(confdir, "/", lconfdir);
	strncat(confdir, filename, lconfdir);
	return confdir;
}


int main(void) {
	char *devpath = find_touch_device();
	printf("Device found: %s\n", devpath);

	char *config = get_conf_path(CONFIG_PATH);
	char *display = get_conf_path(DISPLAYCONF);

	get_device_event_loop(devpath, config, display);
	free(devpath);
	free(config);
	free(display);
	return 0;
}
