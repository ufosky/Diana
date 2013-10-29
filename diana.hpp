#include "diana.h"

#include <map>
#include <typeinfo>
#include <string>
#include <cstdlib>

#include <cstdio>

namespace Diana {

class Component { };

class System;
class Entity;

struct typeCompare {
    bool operator () (const std::type_info * a, const std::type_info * b) {
        return a->before(*b);
    }
};

class World {
public:
    World(void *(*malloc)(size_t) = malloc, void (*free)(void *) = free);

    template<class T>
    DLuint registerComponent() {
        const std::type_info * tid = &typeid(T);

        if(components.count(tid) == 0) {
            components[tid] = diana_registerComponent(diana, tid->name(), sizeof(T));
        }
        return components[tid];
    }

    template<class T>
    DLuint getComponentId() {
        const std::type_info * tid = &typeid(T);

        return components[tid];
    }

    void registerSystem(System *system);

    void initialize();

    Entity spawn();

    void process(DLfloat delta);

    struct diana *getDiana() { return diana; }

private:
    struct diana *diana;
    std::map<const std::type_info *, DLuint> components;
};

class Entity {
public:
    Entity(World *world, DLuint id) : _world(world), _id(id) { }

    template<class T>
    void setComponent(const T * data = NULL) {
        DLuint cid = _world->getComponentId<T>();
        diana_setComponent(_world->getDiana(), _id, cid, data);
    }

    template<class T>
    void setComponent(const T & data) {
        setComponent(&data);
    }

    template<class T>
    T &getComponent() {
        DLuint cid = _world->getComponentId<T>();
        return *(T *)diana_getComponent(_world->getDiana(), _id, cid);
    }

    void add();
    void enable();
    void disable();
    void _delete();

    DLuint getId() { return _id; }

private:
    World * _world;
    DLuint _id;
};

class System {
public:
    System(const char *name) : _name(name) { }

    void setWorld(World *);
    World *getWorld() { return _world; }
    DLuint getId() { return _id; }

    virtual void addWatches() { }

    virtual void started() { }
    virtual void process(Entity &entity, DLfloat delta) { }
    virtual void ended() { }

    virtual void subscribed(Entity &entity) { }
    virtual void unsubscribed(Entity &entity) { }

    template<class T>
    void watch() {
        DLuint cid = _world->registerComponent<T>();
        diana_watch(_world->getDiana(), _id, cid);
    }

private:
    std::string _name;
    World * _world;
    DLuint _id;
};

};
