#include <libinput.h>
#include <libudev.h>

#include <poll.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif
#define MOV_SLOTS 10  // number of slots (eg maximum number of supported touch points
#define MIN_EDGE_DISTANCE 10.0  // minimum gesture distance from edge (in mm)
#define LOGGING false
#define CALIBRATION_NUM 5
#define DISPLAYCONF "dims.txt"


enum DIRECTION {  // if applied to borders, will just denote the side of the border
	DIR_NONE,
	DIR_TOP, // movement towards top
	DIR_RIGHT,  // movement towards right
	DIR_BOT, // movement towards bottom
	DIR_LEFT, // movement towards left
};

enum GESTTYPE {
	GT_NONE,  // not a gesture
	GT_TAP,  // single tap on the screen with no movement
	GT_MOVEMENT,  // general moving gesture
	GT_BORDER,  // movement starting on border of screen
};

typedef struct gesture {
	enum GESTTYPE type;  // type of gesture
	enum DIRECTION dir;  // direction of gesture
	uint8_t num;  // multitouch number of fingers
} gesture;

typedef struct rule {
	gesture key;
	char command[512];
} rule;

typedef struct node {
	void *value;
	size_t size;
	struct node *next;
} node;

typedef struct list {
	node *head;
	node *tail;
} list;

enum GESTTYPE str_to_gesttype(const char *s) {
	if (strncmp(s, "BORDER", 16) == 0) {
		return GT_BORDER;
	}
	if (strncmp(s, "MOVEMENT", 16) == 0) {
		return GT_MOVEMENT;
	}
	if (strncmp(s, "TAP", 16) == 0) {
		return GT_TAP;
	}
	return GT_NONE;
}

enum DIRECTION str_to_direction(const char *s) {
	if (strncmp(s, "N", 16) == 0) {
		return DIR_TOP;
	}
	if (strncmp(s, "W", 16) == 0) {
		return DIR_RIGHT;
	}
	if (strncmp(s, "S", 16) == 0) {
		return DIR_BOT;
	}
	if (strncmp(s, "E", 16) == 0) {
		return DIR_LEFT;
	}
	return DIR_NONE;
}

void list_destroy(list *list) {
	node *cur = list->head;
	node *temp;
	while (cur != NULL) {
		temp = cur;
		cur = cur->next;
		free(temp->value);
		free(temp);
	}
	free(list);
}

list *list_new(const void *first_val, size_t size) {
	list *l = calloc(1, sizeof *l);
	l->head = calloc(1, sizeof(node));
	l->tail = l->head;
	// copy arbitraty value into the location
	l->head->value = malloc(size);
	l->head->size = size;
	memcpy(l->head->value, first_val, size);
	return l;
}

list *list_append(list *l, const void *newval, size_t size) {
	l->tail->next = calloc(1, sizeof(node));
	l->tail = l->tail->next;
	l->tail->value = malloc(size);
	l->tail->size = size;
	memcpy(l->tail->value, newval, size);
	return l;
}

void list_print(list *l) {
	size_t index = 0;
	node *cur = l->head;
	while (cur != NULL) {
		printf("%lu\n", index++);
		cur = cur->next;
	}
}

size_t list_len(list *l) {
	node *cur = l->head;
	size_t len = 0;
	while (cur != NULL) {
		cur = cur->next;
		len++;
	}
	return len;
}

typedef struct vec2 {
	double x;
	double y;
} vec2;

typedef struct movement {
	vec2 start;
	uint32_t tstart;
	vec2 end;
	uint32_t tend;
	bool ready;
	bool down;
} movement;

// convert an angle in radians into direction enum
enum DIRECTION angle_to_direction(double angle) {
	double m_pi4 = M_PI / 4;
	if (isnan(angle)) {
		return DIR_NONE;
	}
	if ((3 * m_pi4 >= angle) && (angle > m_pi4)) {
		return DIR_TOP;
	}
	if ((5 * m_pi4 >= angle) && (angle > 3 * m_pi4)) {
		return DIR_LEFT;
	}
	if ((7 * m_pi4 >= angle) && (angle > 5 * m_pi4)) {
		return DIR_BOT;
	}
	return DIR_RIGHT;
}

double distance_euclidian(struct vec2 a, struct vec2 b) {
	return sqrt(pow((a.x - b.x), 2.0) + pow((a.y - b.y), 2.0));
}

struct vec2 vec2_sub(struct vec2 a, struct vec2 b) {
	struct vec2 r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	return r;
}

double rad_to_deg(double rad) {
	return rad * 180.0 / M_PI;
}

double vec2_len(struct vec2 a) {
	return sqrt(pow(a.x, 2.0) + pow(a.y, 2.0));
}

double scalar_product(struct vec2 a, struct vec2 b) {
	return a.x * b.x + a.y * b.y;
}

double vec2_angle(struct vec2 a, struct vec2 b) {
	return acos(scalar_product(a, b) / (vec2_len(a) * vec2_len(b)));
}

double movement_angle(const struct movement *m) {
	struct vec2 diff = vec2_sub(m->end, m->start);
	struct vec2 base = (struct vec2){1, 0};

	if (diff.x == 0 && diff.y == 0) {
		return NAN;
	}

	double angle = vec2_angle(diff, base);
	base.x = 1;
	base.y = 0;
	// ref 0 0 is UPPER left corner
	if (diff.y > 0) {
		angle = 2 * M_PI - angle;
	}
	return angle;
}

double movement_length(const struct movement *m) {
	return distance_euclidian(m->end, m->start);
}

uint32_t movement_timedelta(const struct movement *m) {
	return m->tend - m->tstart;
}

void print_timedelta(uint32_t timedelta) {
	printf("Time %ds\n", timedelta);
}


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

void handle_event(struct libinput_event *event, struct movement *m) {
	int32_t slot;

	struct libinput_event_touch *tevent;

	switch(libinput_event_get_type(event)) {
	case LIBINPUT_EVENT_TOUCH_DOWN:
		tevent = libinput_event_get_touch_event(event);
		slot = libinput_event_touch_get_slot(tevent);
		m[slot].start.x = libinput_event_touch_get_x(tevent);
		m[slot].start.y = libinput_event_touch_get_y(tevent);
		m[slot].tstart = libinput_event_touch_get_time(tevent);
		m[slot].end.x = m[slot].start.x;
		m[slot].end.y = m[slot].start.y;
		m[slot].tend = m[slot].tstart;
		m[slot].down = true;
#if LOGGING
		printf("%d down\n", slot);
#endif
		break;
	case LIBINPUT_EVENT_TOUCH_UP:
		tevent = libinput_event_get_touch_event(event);
		slot = libinput_event_touch_get_slot(tevent);
		m[slot].ready = true;
		m[slot].down = false;
#if LOGGING
		printf("%d up\n", slot);
#endif
		break;
	case LIBINPUT_EVENT_TOUCH_CANCEL:
		tevent = libinput_event_get_touch_event(event);
#if LOGGING
		printf("%dTouch cancel.\n", slot);
#endif
		break;
	case LIBINPUT_EVENT_TOUCH_MOTION:
		tevent = libinput_event_get_touch_event(event);
		slot = libinput_event_touch_get_slot(tevent);
		m[slot].end.x = libinput_event_touch_get_x(tevent);
		m[slot].end.y = libinput_event_touch_get_y(tevent);
		m[slot].tend = libinput_event_touch_get_time(tevent);
#if LOGGING
		printf("%d Motion\n", slot);
#endif
		break;
	case LIBINPUT_EVENT_TOUCH_FRAME:
#if LOGGING
		printf("Touch frame\n");
#endif
		break;
	default:
		printf("Unknown event type. %d\n", libinput_event_get_type(event));
		break;
	}
}

gesture get_edge_event(const movement *m, const movement *screen) {
	gesture ev = (gesture){GT_NONE, DIR_NONE, 1};
	if (m->start.x <= screen->start.x) {
		ev.dir = DIR_LEFT;
		ev.type = GT_BORDER;
	} else if (m->start.x >= screen->end.x) {
		ev.dir = DIR_RIGHT;
		ev.type = GT_BORDER;
	} else if (m->start.y <= screen->start.y) {
		ev.dir = DIR_TOP;
		ev.type = GT_BORDER;
	} else if (m->start.y >= screen->end.y) {
		ev.dir = DIR_BOT;
		ev.type = GT_BORDER;
	}
	return ev;
}

double edge_distance(const struct movement *m, gesture *edge) {
	double distance;
	switch (edge->dir) {
	case DIR_TOP:
	case DIR_BOT:
		distance = abs(m->start.y - m->end.y);
		break;
	case DIR_LEFT:
	case DIR_RIGHT:
		distance = abs(m->start.x - m->end.x);
		break;
	case DIR_NONE:
		distance = -1.0;
		break;
	}
	return distance;
}

bool valid_action(const struct movement *m, gesture *e) {
	return (e->type == GT_BORDER) && (edge_distance(m, e) > MIN_EDGE_DISTANCE);
}

// Get all movement slots that are currently ready
list *get_ready_movements(struct movement *m) {
	list *ready = NULL;
	for (size_t i = 0; i < MOV_SLOTS; i++) {
		if (m[i].ready) {
			if (ready == NULL) {
				ready = list_new(&i, sizeof(size_t));
			} else {
				list_append(ready, &i, sizeof(size_t));
			}
			m[i].ready = false;
		}
	}
	return ready;
}

bool any_down(struct movement *m) {
	for (size_t i = 0; i < MOV_SLOTS; i++){
		if (m[i].down) {
			return true;
		}
	}
	return false;
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
	while (cur != NULL) {
		cm = (m + *((size_t *)cur->value));
		startvec = cm->start;
		if (movement_length(cm) < MIN_EDGE_DISTANCE) {
			continue;
		}
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
		cur = cur->next;
	}
	return DIR_NONE;
}

gesture get_gesture(movement *m, movement *screen, list *ready) {
	gesture g = {0};
	g.num = list_len(ready);
	g.dir = movement_direction(m, ready);
	enum DIRECTION border_dir;
	if (g.dir == DIR_NONE) {
		g.type = GT_TAP;
	} else if (g.num > 1) {
		g.type = GT_MOVEMENT;
	} else if ((border_dir = movement_border_direction(m, ready, screen)) != DIR_NONE) {
		g.type = GT_BORDER;
		g.dir = border_dir;
	}
	return g;
}

void print_gesture(gesture *g) {
	printf("G(%d) T%d D%d\n", g->num, g->type, g->dir);
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
#if LOGGING
		printf("Still down\n");
#endif
		return;
	}
	list *ready = get_ready_movements(m);
	if (ready == NULL) {
#if LOGGING
		printf("None ready\n");
#endif
		return;
	}
#if LOGGING
	printf("Get gestures\n");
#endif
	gesture g = get_gesture(m, screen, ready);
	print_gesture(&g);

	trigger_rules(&g, rules);

	list_destroy(ready);
}

struct movement read_screen_dimensions(const char *dimfile) {
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

void clear_event_pipe(struct libinput *li) {
	struct libinput_event *event;
	// clear up event pipeline
	libinput_dispatch(li);
	while ((event = libinput_get_event(li)) != NULL) {
		libinput_event_destroy(event);
		libinput_dispatch(li);
	}
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
		libinput_dispatch(li);
		while ((event = libinput_get_event(li)) != NULL) {
			// handle the event here
			handle_event(event, movements);
			libinput_event_destroy(event);
			libinput_dispatch(li);
		}
		handle_movements(movements, screen, rules);
	}
}

struct movement calibrate_touchscreen(const char *devpath, const char *dimfile) {
	struct movement screen = {0};
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
	FILE *f = fopen(dimfile, "w");
	if (f == NULL) {
		printf("Error opening dim file\n");
		return screen;
	}
	fprintf(f, "%lf %lf  # X dimensions\n", screen.start.x, screen.end.x);
	fprintf(f, "%lf %lf  # Y dimensions\n", screen.start.y, screen.end.y);
	fclose(f);
}

bool whitespace(char c) {
	switch(c) {
	case ' ':
	case '\t':
	case '\n':
		return true;
	default:
		return false;
	}
}

bool str_startswith(const char *line, char c) {
	char n;
	for (size_t i = 0; (n = line[i]) != '\0'; i++) {
		if (n == c){
			return true;
		}
		if (!whitespace(n)) {
			return false;
		}
	}
	return false;
}

gesture *str_to_gesture(char *line) {
	gesture *g = calloc(1, sizeof *g);
	char *next = strtok(line, " ");
	g->type = str_to_gesttype(next);
	next = strtok(NULL, " ");
	g->dir = str_to_direction(next);
	next = strtok(NULL, " ");
	g->num = atoi(next);
	if (g->type == GT_NONE) {
		free(g);
		return NULL;
	}
	return g;
}

char *str_to_command(const char *line) {
	char *command = NULL;
	// check that line starts with 4 spaces
	for (int i = 0; i < 4; i++) {
		if (line[i] != ' ') {
			return command;
		}
	}
	size_t size = strlen(line);
	command = calloc(size - 3, 1);
	memcpy(command, line + 4, size - 5); // 5 to remove newline and zero delim
	return command;
}

bool blank_line(const char *line) {
	for (int i = 0; line[i] != '\0'; i++) {
		if (!whitespace(line[i])) {
			return false;
		}
	}
	return true;
}

#define BUFSIZE 512
// load a config file containing rules
list *load_rules(const char *path) {
	rule *currule = calloc(1, sizeof *currule);
	gesture *g;
	char *c;
	list *l = NULL;
	char buffer[BUFSIZE];
	FILE *f = fopen(path, "re");
	if (f == NULL) {
		printf("Failed to open config at %s\n", path);
		return l;
	}
	int state = 0;
	while (fgets(buffer, BUFSIZE, f) != NULL) {
		if (str_startswith(buffer, '#')) {
			continue;
		}
		if (blank_line(buffer)) {
			continue;
		}
		switch(state) {
		case 0:
			if ((g = str_to_gesture(buffer)) != NULL) {
				currule->key = *g;
				free(g);
				state = 1;
			}
			break;
		case 1:
			c = str_to_command(buffer);
			strncpy(currule->command, c, 512);
			free(c);
			state = 0;
			if (l == NULL) {
				l = list_new(currule, sizeof *currule);
			} else {
				list_append(l, currule, sizeof *currule);
			}
			currule = calloc(1, sizeof *currule);
			break;
		default:
			break;
		}
	}
	return l;
}

int get_device_event_loop(const char *devpath, const char *rulespath) {
	struct movement screen;
	if (access(DISPLAYCONF, F_OK) != -1) {
		screen = read_screen_dimensions(DISPLAYCONF);
	} else {
		screen = calibrate_touchscreen(devpath, DISPLAYCONF);
	}

	// load rules
	list *rules = load_rules(rulespath);

	node *cur = rules->head;
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

char *get_devpath(struct libinput_device *dev) {
	struct udev_device *uddev = libinput_device_get_udev_device(dev);
	const char *origpath = udev_device_get_devnode(uddev);
	size_t plen = strlen(origpath);
	char *devpath = calloc(plen + 1, sizeof *devpath);
	strncpy(devpath, origpath, plen);
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
	char *devicepath;

	struct udev *ud;

	ud = udev_new();
	li = libinput_udev_create_context(&interface, NULL, ud);

	libinput_udev_assign_seat(li, "seat0");

	libinput_dispatch(li);
	while ((ev = libinput_get_event(li))) {
		if (check_is_touchscreen(ev, &devicepath))
			break;
		libinput_dispatch(li);
	}

	udev_unref(ud);
	libinput_unref(li);
	return devicepath;
}

int main(void) {
	char *devpath;
	devpath = find_touch_device();
	printf("Device found: %s\n", devpath);
	get_device_event_loop(devpath, "./config");
	free(devpath);
	return 0;
}
