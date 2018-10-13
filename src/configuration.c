#include "libinput-touchscreen.h"
#include "configuration.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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
	if(strncmp(line, "    ", 4) != 0)
		return command
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
