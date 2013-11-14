// vim: ts=2:sw=2:noexpandtab

#include "diana.h"

#include <map>
#include <typeinfo>
#include <string>
#include <cstdlib>

namespace Diana {

class Component { };

class Entity;
class System;
class Manager;

class World {
public:
	World(void *(*malloc)(size_t) = malloc, void (*free)(void *) = free);

	template<class T>
	unsigned int registerComponent() {
		const std::type_info * tid = &typeid(T);
		if(components.count(tid) == 0) {
			components[tid] = diana_createComponent(diana, tid->name(), sizeof(T), DL_COMPONENT_FLAG_INLINE);
		}
		return components[tid];
	}

	template<class T>
	unsigned int getComponentId() {
		const std::type_info * tid = &typeid(T);

		return components[tid];
	}

	void registerSystem(System *system);
	void registerManager(Manager *manager);

	void initialize();

	Entity spawn();

	void process(float delta);

	struct diana *getDiana() { return diana; }

private:
	struct diana *diana;
	std::map<const std::type_info *, unsigned int> components;
};

class Entity {
public:
	Entity(World *world, unsigned int id) : _world(world), _id(id) { }

	template<class T>
	void setComponent(const T * data = NULL) {
		unsigned int cid = _world->getComponentId<T>();
		diana_setComponent(_world->getDiana(), _id, cid, data);
	}

	template<class T>
	void setComponent(const T & data) {
		setComponent(&data);
	}

	template<class T>
	T &getComponent() {
		unsigned int cid = _world->getComponentId<T>();
		return *(T *)diana_getComponent(_world->getDiana(), _id, cid);
	}

	void add();
	void enable();
	void disable();
	void _delete();

	unsigned int getId() { return _id; }

private:
	World * _world;
	unsigned int _id;
};

class System {
public:
	System(const char *name) : _name(name) { }

	void setWorld(World *);
	World *getWorld() { return _world; }
	unsigned int getId() { return _id; }

	virtual void addWatches() { }

	virtual void starting() { }
	virtual void process(Entity &entity, float delta) { }
	virtual void ending() { }

	virtual void subscribed(Entity &entity) { }
	virtual void unsubscribed(Entity &entity) { }

	template<class T>
	void watch() {
		unsigned int cid = _world->registerComponent<T>();
		diana_watch(_world->getDiana(), _id, cid);
	}

	template<class T>
	void exclude() {
		unsigned int cid = _world->registerComponent<T>();
		diana_exclude(_world->getDiana(), _id, cid);
	}

private:
	std::string _name;
	World * _world;
	unsigned int _id;
};

class Manager {
public:
	Manager(const char *name) : _name(name) { }

	void setWorld(World *);
	World *getWorld() { return _world; }
	unsigned int getId() { return _id; }

	virtual void added(Entity &entity) { }
	virtual void enabled(Entity &entity) { }
	virtual void disabled(Entity &entity) { }
	virtual void deleted(Entity &entity) { }

private:
	std::string _name;
	World * _world;
	unsigned int _id;
};

};
