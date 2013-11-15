// vim: ts=2:sw=2:noexpandtab

#include "diana.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

struct position {
	float x, y;
};
static unsigned int positionComponent;

struct velocity {
	float x, y;
};
static unsigned int velocityComponent;

struct renderer {
	int c;
};
static unsigned int rendererComponent;

void _DEBUG(struct diana *diana, const char *file, int line) {
	unsigned int err = diana_getError(diana);
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

static void movementSystem_process(struct diana *diana, void *user_data, unsigned int entity, float delta) {
	struct position *position = (struct position *)diana_getComponent(diana, entity, positionComponent); DEBUG(diana);
	struct velocity *velocity = (struct velocity *)diana_getComponent(diana, entity, velocityComponent); DEBUG(diana);

	position->x += velocity->x * delta;
	position->y += velocity->y * delta;

	printf("%i move to (%f,%f)\n", entity, position->x, position->y);
}

static void renderSystem_process(struct diana *diana, void *user_data, unsigned int entity, float delta) {
	struct position *position = (struct position *)diana_getComponent(diana, entity, positionComponent); DEBUG(diana);
	struct renderer *renderer = (struct renderer *)diana_getComponent(diana, entity, rendererComponent); DEBUG(diana);

	printf("%i rendered at (%f,%f,%c)\n", entity, position->x, position->y, renderer->c);
}

int main() {
	struct diana *diana = allocate_diana(malloc, free);
	struct velocity ev = {1.5, 0};
	unsigned int i = 0;

	positionComponent = diana_createComponent(diana, "position", sizeof(struct position), DL_COMPONENT_FLAG_INLINE); DEBUG(diana);
	velocityComponent = diana_createComponent(diana, "velocity", sizeof(struct velocity), DL_COMPONENT_FLAG_INLINE); DEBUG(diana);
	rendererComponent = diana_createComponent(diana, "renderer", sizeof(struct renderer), DL_COMPONENT_FLAG_INLINE); DEBUG(diana);

	unsigned int movementSystem = diana_createSystem(diana, "movement", NULL, movementSystem_process, NULL, NULL, NULL, NULL, DL_SYSTEM_FLAG_NORMAL); DEBUG(diana);
	diana_watch(diana, movementSystem, positionComponent); DEBUG(diana);
	diana_watch(diana, movementSystem, velocityComponent); DEBUG(diana);

	unsigned int renderSystem = diana_createSystem(diana, "render", NULL, renderSystem_process, NULL, NULL, NULL, NULL, DL_SYSTEM_FLAG_PASSIVE); DEBUG(diana);
	diana_watch(diana, renderSystem, positionComponent); DEBUG(diana);
	diana_watch(diana, renderSystem, rendererComponent); DEBUG(diana);

	diana_initialize(diana); DEBUG(diana);

	unsigned int e = diana_spawn(diana); DEBUG(diana);
	diana_setComponent(diana, e, positionComponent, NULL); DEBUG(diana);
	diana_setComponent(diana, e, velocityComponent, &ev); DEBUG(diana);
	diana_signal(diana, e, DL_ENTITY_ADDED); DEBUG(diana);

	unsigned int e1 = diana_spawn(diana); DEBUG(diana);
	diana_setComponent(diana, e1, positionComponent, NULL); DEBUG(diana);
	diana_setComponent(diana, e1, rendererComponent, NULL); DEBUG(diana);
	diana_signal(diana, e1, DL_ENTITY_ADDED); DEBUG(diana);

	while(1) {
		// 30 fps
		diana_process(diana, 1.0/30.0); DEBUG(diana);
		sleep(1);

		if(i++ % 2 == 0) {
			diana_processSystem(diana, renderSystem, 1.0/30.0); DEBUG(diana);
		}
	}
}
