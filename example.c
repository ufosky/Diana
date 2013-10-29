#include "diana.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

struct position {
	float x, y;
};
static DLuint positionComponent;

struct velocity {
	float x, y;
};
static DLuint velocityComponent;

struct renderer {
	int c;
};
static DLuint rendererComponent;

void _DEBUG(struct diana *diana, const char *file, int line) {
	DLenum err = diana_getError(diana);
	if(err == DL_ERROR_NONE) {
		return;
	}
	printf("ERROR %s:%i\n", file, line);
	exit(1);
}
#if 0
#define DEBUG(D) _DEBUG(D, __FILE__, __LINE__)
#else
#define DEBUG(D)
#endif

static void movementSystem_process(struct diana *diana, void *user_data, DLuint entity, DLfloat delta) {
	struct position *position = (struct position *)diana_getComponent(diana, entity, positionComponent); DEBUG(diana);
	struct velocity *velocity = (struct velocity *)diana_getComponent(diana, entity, velocityComponent); DEBUG(diana);

	position->x += velocity->x * delta;
	position->y += velocity->y * delta;

	printf("%i move to (%f,%f)\n", entity, position->x, position->y);
}

static void renderSystem_process(struct diana *diana, void *user_data, DLuint entity, DLfloat delta) {
	struct position *position = (struct position *)diana_getComponent(diana, entity, positionComponent); DEBUG(diana);
	struct renderer *renderer = (struct renderer *)diana_getComponent(diana, entity, rendererComponent); DEBUG(diana);

	printf("%i rendered at (%f,%f)\n", entity, position->x, position->y);
}

int main() {
	struct diana *diana = allocate_diana(malloc, free);
	struct velocity ev = {1.5, 0};

	positionComponent = diana_registerComponent(diana, "position", sizeof(struct position)); DEBUG(diana);
	velocityComponent = diana_registerComponent(diana, "velocity", sizeof(struct velocity)); DEBUG(diana);
	rendererComponent = diana_registerComponent(diana, "renderer", sizeof(struct renderer)); DEBUG(diana);

	DLuint movementSystem = diana_registerSystem(diana, "movement", movementSystem_process, NULL); DEBUG(diana);
	diana_watch(diana, movementSystem, positionComponent); DEBUG(diana);
	diana_watch(diana, movementSystem, velocityComponent); DEBUG(diana);

	DLuint renderSystem = diana_registerSystem(diana, "render", renderSystem_process, NULL); DEBUG(diana);
	diana_watch(diana, renderSystem, positionComponent); DEBUG(diana);
	diana_watch(diana, renderSystem, rendererComponent); DEBUG(diana);

	diana_initialize(diana); DEBUG(diana);

	DLuint e = diana_spawn(diana); DEBUG(diana);
	diana_setComponent(diana, e, positionComponent, NULL); DEBUG(diana);
	diana_setComponent(diana, e, velocityComponent, &ev); DEBUG(diana);
	diana_add(diana, e); DEBUG(diana);

	DLuint e1 = diana_spawn(diana); DEBUG(diana);
	diana_setComponent(diana, e1, positionComponent, NULL); DEBUG(diana);
	diana_setComponent(diana, e1, rendererComponent, NULL); DEBUG(diana);
	diana_add(diana, e1); DEBUG(diana);

	while(1) {
		// 30 fps
		diana_process(diana, 1.0/30.0); DEBUG(diana);
		sleep(1);
	}
}
