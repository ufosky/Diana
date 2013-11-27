// vim: ts=2:sw=2:noexpandtab

#include "diana.hpp"

namespace Diana {

// ============================================================================
// WORLD
// - really 'diana' but world is a better name
World::World(void *(*malloc)(size_t), void (*free)(void *)) {
	allocate_diana(malloc, free, &diana);
}

void World::registerSystem(System *system) {
	system->setWorld(this);
}

void World::registerManager(Manager *manager) {
	manager->setWorld(this);
}

void World::initialize() {
	diana_initialize(diana);
}

void World::process(float delta) {
	diana_process(diana, delta);
}

Entity World::spawn() {
	unsigned int eid;
 	diana_spawn(diana, &eid);
	return Entity(this, eid);
}

// ============================================================================
// ENTITY
void Entity::add() {
	diana_signal(_world->getDiana(), _id, DL_ENTITY_ADDED);
}

void Entity::enable() {
	diana_signal(_world->getDiana(), _id, DL_ENTITY_ENABLED);
}

void Entity::disable() {
	diana_signal(_world->getDiana(), _id, DL_ENTITY_DISABLED);
}

void Entity::_delete() {
	diana_signal(_world->getDiana(), _id, DL_ENTITY_DELETED);
}

// ============================================================================
// SYSTEM
static void _system_starting(struct diana *, void *user_data) {
	System *sys = (System *)user_data;
	sys->starting();
}

static void _system_process(struct diana *, void *user_data, unsigned int entity_id, float delta) {
	System *sys = (System *)user_data;
	Entity entity(sys->getWorld(), entity_id);
	sys->process(entity, delta);
}

static void _system_ending(struct diana *, void *user_data) {
	System *sys = (System *)user_data;
	sys->ending();
}

static void _system_subscribed(struct diana *, void *user_data, unsigned int entity_id) {
	System *sys = (System *)user_data;
	Entity entity(sys->getWorld(), entity_id);
	sys->subscribed(entity);
}

static void _system_unsubscribed(struct diana *, void *user_data, unsigned int entity_id) {
	System *sys = (System *)user_data;
	Entity entity(sys->getWorld(), entity_id);
	sys->unsubscribed(entity);
}

void System::setWorld(World *world) {
	_world = world;
	diana_createSystem(world->getDiana(), _name.c_str(), _system_starting, _system_process, _system_ending, _system_subscribed, _system_unsubscribed, this, systemFlags(), &_id);
	addWatches();
}

// ============================================================================
// MANAGER
static void _manager_added(struct diana *, void *user_data, unsigned int entity_id) {
	Manager *man = (Manager *)user_data;
	Entity entity(man->getWorld(), entity_id);
	man->added(entity);
}

static void _manager_enabled(struct diana *, void *user_data, unsigned int entity_id) {
	Manager *man = (Manager *)user_data;
	Entity entity(man->getWorld(), entity_id);
	man->enabled(entity);
}

static void _manager_disabled(struct diana *, void *user_data, unsigned int entity_id) {
	Manager *man = (Manager *)user_data;
	Entity entity(man->getWorld(), entity_id);
	man->disabled(entity);
}

static void _manager_deleted(struct diana *, void *user_data, unsigned int entity_id) {
	Manager *man = (Manager *)user_data;
	Entity entity(man->getWorld(), entity_id);
	man->deleted(entity);
}

void Manager::setWorld(World *world) {
	_world = world;
	diana_createManager(world->getDiana(), _name.c_str(), _manager_added, _manager_enabled, _manager_disabled, _manager_deleted, this, managerFlags(), &_id);
}

};
