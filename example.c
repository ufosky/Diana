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

struct positionHash {
	unsigned int hash;
};
static unsigned int positionHashComponent;

static void movementSystem_process(struct diana *diana, void *user_data, unsigned int entity, float delta) {
	struct position *position;
	struct velocity *velocity;

	diana_getComponent(diana, entity, positionComponent, (void **)&position);
	diana_getComponent(diana, entity, velocityComponent, (void **)&velocity);

	position->x += velocity->x * delta;
	position->y += velocity->y * delta;

	printf("%i move to (%f,%f)\n", entity, position->x, position->y);
}

static void renderSystem_process(struct diana *diana, void *user_data, unsigned int entity, float delta) {
	struct position *position;
	struct renderer *renderer;

	diana_getComponent(diana, entity, positionComponent, (void **)&position);
	diana_getComponent(diana, entity, rendererComponent, (void **)&renderer);

	printf("%i rendered at (%f,%f,%c)\n", entity, position->x, position->y, renderer->c);
}

static void computeSystem_process(struct diana *diana, void *user_data, unsigned int entity, float delta) {
	struct positionHash *hash;
	diana_getComponent(diana, entity, positionHashComponent, (void **)&hash);
	printf("Compute, old %i -> ", hash->hash);
	diana_dirtyComponent(diana, entity, positionComponent);
	diana_getComponent(diana, entity, positionHashComponent, (void **)&hash);
	printf("new %i\n", hash->hash);
}

static void doCompute(struct diana *diana, void *user_data, unsigned int entity, unsigned int i, void *_component) {
	struct position *position;
	diana_getComponent(diana, entity, positionComponent, (void **)&position);
	((struct positionHash *)_component)->hash = (unsigned int)((position->x + 3) * 18.059 + (position->y + 5) * 20.983);
}

int main() {
	struct diana *diana;
	struct velocity ev = {1.5, 0};
	struct renderer r = {'@'};
	unsigned int movementSystem, renderSystem, computeSystem, e, e1;

	allocate_diana(malloc, free, &diana);

	diana_createComponent(diana, "position", sizeof(struct position), DL_COMPONENT_FLAG_INLINE, &positionComponent);
	diana_createComponent(diana, "velocity", sizeof(struct velocity), DL_COMPONENT_FLAG_INLINE, &velocityComponent);
	diana_createComponent(diana, "renderer", sizeof(struct renderer), DL_COMPONENT_FLAG_INLINE, &rendererComponent);

	diana_createComponent(diana, "positionHash", sizeof(struct positionHash), DL_COMPONENT_FLAG_INLINE, &positionHashComponent);
	diana_componentCompute(diana, positionHashComponent, doCompute, NULL);

	diana_createSystem(diana, "movement", NULL, movementSystem_process, NULL, NULL, NULL, NULL, DL_SYSTEM_FLAG_NORMAL, &movementSystem);
	diana_watch(diana, movementSystem, positionComponent);
	diana_watch(diana, movementSystem, velocityComponent);

	diana_createSystem(diana, "render", NULL, renderSystem_process, NULL, NULL, NULL, NULL, DL_SYSTEM_FLAG_NORMAL, &renderSystem);
	diana_watch(diana, renderSystem, positionComponent);
	diana_watch(diana, renderSystem, rendererComponent);

	diana_createSystem(diana, "compute", NULL, computeSystem_process, NULL, NULL, NULL, NULL, DL_SYSTEM_FLAG_NORMAL, &computeSystem);
	diana_watch(diana, computeSystem, positionComponent);
	diana_watch(diana, computeSystem, positionHashComponent);

	diana_initialize(diana);

	diana_spawn(diana, &e);
	diana_setComponent(diana, e, positionComponent, NULL);
	diana_setComponent(diana, e, velocityComponent, &ev);
	diana_setComponent(diana, e, positionHashComponent, NULL);
	diana_signal(diana, e, DL_ENTITY_ADDED);

	diana_spawn(diana, &e1);
	diana_setComponent(diana, e1, positionComponent, NULL);
	diana_setComponent(diana, e1, rendererComponent, &r);
	diana_signal(diana, e1, DL_ENTITY_ADDED);

	while(1) {
		// 30 fps
		diana_process(diana, 1.0/30.0);
		sleep(1);
	}
}
