#ifndef LIBINPUT_TOUCHSCREEN_H
#include "libinput-backend.h"
#include "list.h"
#include <stdbool.h>
#include <stdint.h>
#define LIBINPUT_TOUCHSCREEN_H
#define MOV_SLOTS 10  // number of slots (eg maximum number of supported touch points
#define MIN_EDGE_DISTANCE 10.0  // minimum gesture distance from edge (in mm)
#define DISPLAYCONF "dims.txt" // name of display configuration
#define LOGGING false
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

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

// String gesture to enum
enum GESTTYPE str_to_gesttype(const char *s);
// String direction to enum
enum DIRECTION str_to_direction(const char *s);
// Radians angle to direction enum
enum DIRECTION angle_to_direction(double angle);

// Calculate euclidean distance between two points
double distance_euclidian(vec2 a, vec2 b);
// Subtract two points
vec2 vec2_sub(vec2 a, vec2 b);
// Radian degree to degree
double rad_to_deg(double rad);
// Length of a 2-element vector
double vec2_len(vec2 a);
// Scalar product between two vectors, using standard norm
double scalar_product(vec2 a, vec2 b);
// Angle between two vectors in radians
double vec2_angle(vec2 a, vec2 b);

// Angle of movement end to movement start in radians
double movement_angle(const movement *m);
// Euclidian distance between movement start and end
double movement_length(const movement *m);
// Movement time difference start end
uint32_t movement_timedelta(const movement *m);

/* Movement Array functions */
// Get a indices of all ready movements
list *get_ready_movements(movement *m);
// Check whether any movement struct pressed down
bool any_down(movement *m);

// Fill movements with libinput events
void handle_event(struct libinput_event *event, movement *m);
#endif
