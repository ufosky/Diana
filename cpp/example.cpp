// vim: ts=2:sw=2:noexpandtab

#include "diana.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

using namespace Diana;

class Position : public Component {
public:
	float x, y;
};

class Velocity : public Component {
public:
	Velocity() : x(0), y(0) { }
	Velocity(float _x, float _y) : x(_x), y(_y) { }

	float x, y;
};

class Renderer : public Component {
public:
	int c;
};

class MovementSystem : public System {
public:
	MovementSystem() : System("Render System") { }

	virtual void addWatches() {
		watch<Position>();
		watch<Velocity>();
	}

	virtual void process(Entity & entity, float delta) {
		Position * position = entity.getComponent<Position>();
		Velocity * velocity = entity.getComponent<Velocity>();

		position->x += velocity->x * delta;
		position->y += velocity->y * delta;

		printf("%i move to (%f,%f)\n", entity.getId(), position->x, position->y);
	}
};

class RenderSystem : public System {
public:
	RenderSystem() : System("Render System") { }

	virtual void addWatches() {
		watch<Position>();
		watch<Renderer>();
	}

	virtual void process(Entity & entity, float delta) {
		Position * position = entity.getComponent<Position>();
		Renderer * renderer = entity.getComponent<Renderer>();

		printf("%i rendered at (%f,%f,%c)\n", entity.getId(), position->x, position->y, renderer->c);
	}
};

int main() {
	World *world = new World();

	world->registerSystem(new MovementSystem());
	world->registerSystem(new RenderSystem());

	world->initialize();

	Entity e = world->spawn();
	e.setComponent<Position>();
	e.setComponent(Velocity(1.5, 0));
	e.add();

	Entity e1 = world->spawn();
	e1.setComponent<Position>();
	e1.setComponent<Renderer>();
	e1.add();

	while(1) {
		// 30 fps
		world->process(1.0/30.0);
		sleep(1);
	}
}

