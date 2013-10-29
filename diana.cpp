// vim: ts=2:sw=2:noexpandtab

#include "diana.hpp"

namespace Diana {

World::World(void *(*malloc)(size_t), void (*free)(void *)) {
	diana = allocate_diana(malloc, free);
}

void World::registerSystem(System *system) {
	system->setWorld(this);
}

void World::initialize() {
	diana_initialize(diana);
}

void World::process(DLfloat delta) {
	diana_process(diana, delta);
}

Entity World::spawn() {
	DLuint eid = diana_spawn(diana);
	return Entity(this, eid);
}

void Entity::add() {
	diana_add(_world->getDiana(), _id);
}

void Entity::enable() {
	diana_enable(_world->getDiana(), _id);
}

void Entity::disable() {
	diana_disable(_world->getDiana(), _id);
}

void Entity::_delete() {
	diana_delete(_world->getDiana(), _id);
}

// DLuint diana_registerSystem(struct diana *diana, const char *name, void (*process)(struct diana *, void *user_data, DLuint entity, DLfloat delta), void *user_data);

static void _system_started(struct diana *, void *user_data) {
	System *sys = (System *)user_data;
	sys->started();
}

static void _system_process(struct diana *, void *user_data, DLuint entity_id, DLfloat delta) {
	System *sys = (System *)user_data;
	Entity entity(sys->getWorld(), entity_id);
	sys->process(entity, delta);
}

static void _system_ended(struct diana *, void *user_data) {
	System *sys = (System *)user_data;
	sys->ended();
}

static void _system_subscribed(struct diana *, void *user_data, DLuint entity_id) {
	System *sys = (System *)user_data;
	Entity entity(sys->getWorld(), entity_id);
	sys->subscribed(entity);
}

static void _system_unsubscribed(struct diana *, void *user_data, DLuint entity_id) {
	System *sys = (System *)user_data;
	Entity entity(sys->getWorld(), entity_id);
	sys->unsubscribed(entity);
}

void System::setWorld(World *world) {
	_world = world;
	_id = diana_registerSystem(world->getDiana(), _name.c_str(), _system_process, this);
	diana_setSystemProcessCallbacks(world->getDiana(), _id, _system_started, _system_process, _system_ended);
	diana_setSystemEventCallback(world->getDiana(), _id, DL_SUBSCRIBED, _system_subscribed);
	diana_setSystemEventCallback(world->getDiana(), _id, DL_UNSUBSCRIBED, _system_unsubscribed);
	addWatches();
}

};
