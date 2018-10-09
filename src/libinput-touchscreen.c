#include "libinput-touchscreen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

double distance_euclidian(vec2 a, vec2 b) {
	return sqrt(pow((a.x - b.x), 2.0) + pow((a.y - b.y), 2.0));
}

vec2 vec2_sub(vec2 a, vec2 b) {
	vec2 r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	return r;
}

double rad_to_deg(double rad) {
	return rad * 180.0 / M_PI;
}

double vec2_len(vec2 a) {
	return sqrt(pow(a.x, 2.0) + pow(a.y, 2.0));
}

double scalar_product(vec2 a, vec2 b) {
	return a.x * b.x + a.y * b.y;
}

double vec2_angle(vec2 a, struct vec2 b) {
	return acos(scalar_product(a, b) / (vec2_len(a) * vec2_len(b)));
}

double movement_angle(const movement *m) {
	vec2 diff = vec2_sub(m->end, m->start);
	vec2 base = (vec2){1, 0};

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

double movement_length(const movement *m) {
	return distance_euclidian(m->end, m->start);
}

uint32_t movement_timedelta(const movement *m) {
	return m->tend - m->tstart;
}

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

void handle_event(struct libinput_event *event, movement *m) {
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
		slot = libinput_event_touch_get_slot(tevent);
		m[slot].ready = false;
		m[slot].down = false;
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

// Get all movement slots that are currently ready
list *get_ready_movements(struct movement *m) {
	list *ready = NULL;
	for (size_t i = 0; i < MOV_SLOTS; i++) {
		if (m[i].ready) {
			if (ready == NULL) {
				ready = list_new(&i, sizeof(i));
			} else {
				list_append(ready, &i, sizeof(i));
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
