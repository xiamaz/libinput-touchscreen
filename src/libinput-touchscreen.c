#include "libinput-touchscreen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#if LOGGING
void logger(const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	vfprintf(stderr, format, argptr);
	va_end(argptr);
}
#else
void logger(const char *format, ...) {
};
#endif

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

double vec2_angle(vec2 a, vec2 b) {
	return acos(scalar_product(a, b) / (vec2_len(a) * vec2_len(b)));
}

// Calculate an angle between the vector spanned by a to b regarding a
// reference [1 0] vector.
double vec2_normalized_angle(vec2 a, vec2 b) {
	vec2 diff = vec2_sub(a, b);
	// ref 0 0 is UPPER left corner
	vec2 base = (vec2){.x = 1, .y = 0};

	if (diff.x == 0 && diff.y == 0) {
		return NAN;
	}

	double angle = vec2_angle(diff, base);
	if (diff.y > 0) {
		angle = 2 * M_PI - angle;
	}
	return angle;
}

double movement_angle(const movement *m) {
	vec2 diff = vec2_sub(m->end[0], m->start);
	// ref 0 0 is UPPER left corner
	vec2 base = (vec2){.x = 1, .y = 0};

	if (diff.x == 0 && diff.y == 0) {
		return NAN;
	}

	double angle = vec2_angle(diff, base);
	if (diff.y > 0) {
		angle = 2 * M_PI - angle;
	}
	return angle;
}

double movement_length(const movement *m) {
	return distance_euclidian(m->end[0], m->start);
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

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) MIN(Y, X)

void handle_event(struct libinput_event *event, movement *m) {
	int32_t slot;
	double angle, distance;
	vec2 cur;
	vec2 da, db;
	double ra, rb;
	vec2 ref = {.x = 1, .y = 0};

	struct libinput_event_touch *tevent;

	switch(libinput_event_get_type(event)) {
	case LIBINPUT_EVENT_TOUCH_DOWN:
		tevent = libinput_event_get_touch_event(event);
		slot = libinput_event_touch_get_slot(tevent);
		m[slot].start = (vec2){
			.x = libinput_event_touch_get_x(tevent),
			.y = libinput_event_touch_get_y(tevent),
		};
		m[slot].tstart = libinput_event_touch_get_time(tevent);
		m[slot].end[0] = m[slot].start;
		m[slot].end[1] = m[slot].start;
		m[slot].end[2] = m[slot].start;
		m[slot].tend = m[slot].tstart;
		m[slot].sum_distance = 0;
		m[slot].sum_dangle = 0;
		m[slot].steps = 1;
		m[slot].down = true;
		logger("E<Down> %d\n", slot);
		break;
	case LIBINPUT_EVENT_TOUCH_UP:
		tevent = libinput_event_get_touch_event(event);
		slot = libinput_event_touch_get_slot(tevent);
		m[slot].ready = true;
		m[slot].down = false;
		logger("E<Up> %d\n", slot);
		break;
	case LIBINPUT_EVENT_TOUCH_CANCEL:
		tevent = libinput_event_get_touch_event(event);
		slot = libinput_event_touch_get_slot(tevent);
		m[slot].ready = false;
		m[slot].down = false;
		logger("E<Cancel> %d\n", slot);
		break;
	case LIBINPUT_EVENT_TOUCH_MOTION:
		tevent = libinput_event_get_touch_event(event);
		slot = libinput_event_touch_get_slot(tevent);

		cur = (vec2){
			.x = libinput_event_touch_get_x(tevent),
			.y = libinput_event_touch_get_y(tevent),
		};
		distance = distance_euclidian(cur, m[slot].end[0]);
		da = vec2_sub(cur, m[slot].start);
		db = vec2_sub(m[slot].end[0], m[slot].start);
		angle = vec2_angle(da, db);
		ra = vec2_angle(da, ref);
		rb = vec2_angle(db, ref);
		if (da.y > 0) {
			ra = 2 * M_PI - ra;
		}
		if (db.y > 0) {
			rb = 2 * M_PI - rb;
		}
		if ((ra + 2 * M_PI - rb) < (rb - ra)) {
			if (ra + 2 * M_PI < rb) {
				angle = -angle;
			}
		} else {
			if (ra < rb) {
				angle = -angle;
			}
		}

		// add deltas
		if (m[slot].steps < 3) {
			m[slot].sum_dangle = 0;
		} else {
			m[slot].sum_dangle += angle;
		}
		m[slot].angle = angle;
		m[slot].sum_distance += distance;
		m[slot].end[2] = m[slot].end[1];
		m[slot].end[1] = m[slot].end[0];
		m[slot].end[0] = cur;
		m[slot].steps++;
		m[slot].tend = libinput_event_touch_get_time(tevent);
		printf("D: %lf A: %lf %lf SA: %lf SD: %lf\n",
		       distance,
		       rad_to_deg(ra), rad_to_deg(rb),
		       rad_to_deg(m[slot].sum_dangle),
		       m[slot].sum_distance);

		// printf("A %lf\n",
		//        rad_to_deg(movement_angle(m + slot)));

		logger("E<Motion> %d\n", slot);
		break;
	case LIBINPUT_EVENT_TOUCH_FRAME:
		logger("Touch frame\n");
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
