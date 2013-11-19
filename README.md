Diana
=====

Diana is a ECS (Entity Component System) with a simple C API.

Diana is simular to Artemis, it has components, managers and systems to process and store data about entities. An entity is just an integer, no other data defines one.

The design of Diana tries to use dependency injection and even lets you define how it allocates and frees memory.

    struct diana * allocate_diana(void *(*malloc)(size_t), void (*free)(void *ptr));
    
    unsigned int diana_getError(struct diana *);
    
    void diana_free(struct diana *);
    
Initialization
==============

The Diana API is split between two modes. Un-Initialized and Initialized. The application will be spend most time in Initialized mode. The application can only create components, systems and managers in uninitialized mode. Entities can be created and modified in Initialized mode. This limitation might be lifted in the future, but that is how it works today.

Entity
======

Being just an integer, an entity can have 5 states (un-added, added, enabled, disabled and deleted). A spawned `diana_spawn` or cloned `diana_clone` entity starts at the un-added status. The status can be updated by `diana_signal`.

An entity is automatically enabled when added, and disabled when deleted. Both signals will go through.

    unsigned int diana_spawn(struct diana *);
    
    unsigned int diana_clone(struct diana *, unsigned int parentEntity);
    
    void diana_signal(struct diana *, unsigned int entity, unsigned int signal);

Component
=========

A component holds data an entity might be interested in. Since Diana stores most components inline (with other component data) only one instance of a component is normally allowed to be associated with an entity. The other types are Indexed and Multiple. Indexed allows Diana to hold data seperatly and more compact while Mutiple does pretty much the same thing but allow multiple instances of a component with the same entity. Both types can be limited, so for example only 50 "dead body" cmoponents are allowed to exist at any time.

Diana also supports a small portion of Reactive programming, by giving a component a compute function. It will call the compute function when a component that it depends on is tagged as dirty. This allows components to delay computation and cache old results until it has a reason to change.

    unsigned int diana_createComponent(
        struct diana *diana,
        const char *name,
        size_t size,
        unsigned int flags
    );

    void diana_componentCompute(struct diana *diana, unsigned int component, void (*compute)(struct diana *, void *, unsigned int entity, unsigned int index, void *), void *userData);

Manager
=======

A manager is a simple system. When created you can give it zero or more functions that are called when an entity changes it's status

    unsigned int diana_createManager(
        struct diana *diana,
        const char *name,
        void (*added)(struct diana *, void *, unsigned int),
        void (*enabled)(struct diana *, void *, unsigned int),
        void (*disabled)(struct diana *, void *, unsigned int),
        void (*deleted)(struct diana *, void *, unsigned int),
        void *userData,
        unsigned int flags
    );
    
System
======

A system is where most computation happens. A system watches for entities with certain components, or entities without certian components, and processes them one at a time. when `diana_process` is called it will do book keeping, update managers and keep systems up to date, then it will go through each system that is not "passive" and process the entities it is interested in. "passive" systems can be processed by calling `diana_processSystem`.

    unsigned int diana_createSystem(
        struct diana *diana,
        const char *name,
        void (*starting)(struct diana *, void *),
        void (*process)(struct diana *, void *, unsigned int, float),
        void (*ending)(struct diana *, void *),
        void (*subscribed)(struct diana *, void *, unsigned int),
        void (*unsubscribed)(struct diana *, void *, unsigned int),
        void *userData,
        unsigned int flags
    );

    void diana_watch(struct diana *diana, unsigned int system, unsigned int component);

    void diana_exclude(struct diana *diana, unsigned int system, unsigned int component);
    
Entity Components
=================

Dealing with components at run time uses 3 basic functions, set, get and remove. However, because Diana supports many types of components, there are essentially 3 ways to deal with them.

The simple basic functions, used for inline and indexed components.

    void diana_setComponent(struct diana *diana, unsigned int entity, unsigned int component, const void * data);

    void * diana_getComponent(struct diana *diana, unsigned int entity, unsigned int component);

    void diana_removeComponent(struct diana *diana, unsigned int entity, unsigned int component);

These functions allow the application to work with multiple instances of a component on an entity.

    unsigned int diana_getComponentCount(struct diana *diana, unsigned int entity, unsigned int component);

    void diana_appendComponent(struct diana *diana, unsigned int entity, unsigned int component, const void * data);

    void diana_removeComponents(struct diana *diana, unsigned int entity, unsigned int component);

The functions above essentially, with exception of `diana_getComponentCount`, use these internally.

    void diana_setComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i, const void * data);

    void * diana_getComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i);

    void diana_removeComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i);
