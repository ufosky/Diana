// vim: ts=2:sw=2:noexpandtab

#include "diana.h"

#include <string.h>
#include <limits.h>

static void DEBUG(void) { }

// {{{ TYPES
// {{{ VECTOR
/* UNUSED
struct _vector {
	size_t size;
	unsigned int capacity;
	unsigned int length;
	void *data;
};
*/
// }}}

// {{{ SPARSE INTEGER SET
// - faster clearing
// - easy tracking of population
// - can be used to 'pop' elements off
// - and iterating over
// - used for delaying entity adding, enabling, disabled and deleting
// - used by each system to track entities it has
struct _sparseIntegerSet {
	unsigned int *dense;
	unsigned int *sparse;
	unsigned int population;
	unsigned int capacity;
};
// }}}

// {{{ DENSE INTEGER SET
// - more memory effecient
// - used for tracking components each entity is using
// - and for the active entities
struct _denseIntegerSet {
	unsigned char *bytes;
	size_t capacity;
};
// }}}

// {{{ COMPONENT
struct _componentBag {
	unsigned int count;
	unsigned int *indexes;
};

#if DL_COMPUTE
struct _computingComponentStack {
	struct _computingComponentStack *previous;
	unsigned int component;
};
#endif

struct _component {
	const char *name;
	size_t size;
	size_t offset;
	unsigned int flags;

	void **data;
	struct _sparseIntegerSet freeDataIndexes;
	unsigned int nextDataIndex;

#if DL_COMPUTE
	void (*compute)(struct diana *, void *, unsigned int entity, unsigned int index, void *);
	void *userData;

	struct _sparseIntegerSet componentsToDirty;
#endif
};
// }}}

// {{{ SYSTEM
struct _system {
	const char *name;
	unsigned int flags;
	void *userData;
	void (*starting)(struct diana *, void *user_data);
	void (*process)(struct diana *, void *user_data, unsigned int entity, float delta);
	void (*ending)(struct diana *, void *user_data);
	void (*subscribed)(struct diana *, void *user_data, unsigned int entity);
	void (*unsubscribed)(struct diana *, void *user_data, unsigned int entity);
	struct _sparseIntegerSet watch;
	struct _sparseIntegerSet exclude;
	struct _denseIntegerSet entities;
};
// }}}

// {{{ MANAGER
struct _manager {
	const char *name;
	unsigned int flags;
	void *userData;
	void (*added)(struct diana *diana, void *userData, unsigned int entity);
	void (*enabled)(struct diana *diana, void *userData, unsigned int entity);
	void (*disabled)(struct diana *diana, void *userData, unsigned int entity);
	void (*deleted)(struct diana *diana, void *userData, unsigned int entity);
};
// }}}

// {{{ DIANA
struct diana {
	void *(*malloc)(size_t);
	void (*free)(void *);

	void *(*create_mutex)(void);
	void (*mutex_lock)(void *);
	void (*mutex_unlock)(void *);
	void (*mutex_free)(void *);

	void *endpoint_mutex;

	int initialized;
	int processing;

	// manage the entity ids
	// reuse deleted entity ids
	struct _sparseIntegerSet freeEntityIds;
	unsigned int nextEntityId;

	// entity data
	// first 'column' is bits of components defined
	// the rest are the components
	unsigned int dataWidth;
	unsigned int dataHeight;
	unsigned int dataHeightCapacity;
	void *data;

	unsigned int processingDataHeight;
	void **processingData;

	// buffer entity status notifications
	struct _sparseIntegerSet added;
	struct _sparseIntegerSet enabled;
	struct _sparseIntegerSet disabled;
	struct _sparseIntegerSet deleted;

	// all active entities (added and enabled)
	struct _denseIntegerSet active;

	unsigned int num_components;
	struct _component *components;

	unsigned int num_systems;
	struct _system *systems;

	unsigned int num_managers;
	struct _manager *managers;

#if DL_COMPUTE
	struct _computingComponentStack *computingComponentStack;
#endif
};
// }}}
// }}}

// {{{ INTERNAL FUNCTIONS
// {{{ FOREACH_*
#define FOREACH_SPARSEINTSET(I, N, S) for(N = 0, I = (S)->dense ? (S)->dense[N] : 0; N < (S)->population; N++, I = (S)->dense[N])
#define FOREACH_DENSEINTSET(I, D) for(I = 0; I < (D)->capacity; I++) if(_bits_isSet((D)->bytes, I))
#define FOREACH_ARRAY(T, N, A, S) for(N = 0, T = A; N < S; N++, T++)
// }}}

// {{{ NULL MUTEX
static void *_create_mutex_noop(void) {
	return NULL;
}

static void _mutex_noop(void *noop) {
	(void)noop;
}
// }}}

// {{{ MEMORY
static void *_malloc(struct diana *diana, size_t size) {
	void *r = diana->malloc(size);
	if(r == NULL) {
		return NULL;
	}
	memset(r, 0, size);
	return r;
}

static void _free(struct diana *diana, void *ptr) {
	if(ptr != NULL) {
		diana->free(ptr);
	}
}

static char *_strdup(struct diana *diana, const char *s) {
	unsigned int l;
	char *r;
	if(s == NULL) {
		return NULL;
	}
	l = strlen(s);
	r = diana->malloc(l + 1);
	if(r == NULL) {
		return NULL;
	}
	memcpy(r, s, l + 1);
	return r;
}

static void *_realloc(struct diana *diana, void *ptr, size_t oldSize, size_t newSize) {
	if(oldSize == newSize) {
		return ptr;
	}
	if(newSize == 0) {
		_free(diana, ptr);
		return NULL;
	}
	void *r = diana->malloc(newSize);
	if(r == NULL) {
		return NULL;
	}
	if(oldSize < newSize) {
		memset((unsigned char *)r + oldSize, 0, newSize - oldSize);
	}
	if(ptr != NULL) {
		memcpy(r, ptr, oldSize < newSize ? oldSize : newSize);
		_free(diana, ptr);
	}
	return r;
}
// }}}

// {{{ VECTOR
/* UNUSED
struct _vector {
	size_t size;
	unsigned int capacity;
	unsigned int length;
	void *data;
};

void _vector_reserve(struct diana *diana, struct _vector *v, unsigned int capacity) {
	v->data = _realloc(diana, v->data, v->size * v->capacity, v->size * capacity);
	v->capacity = capacity;
}

void _vector_init(struct diana *diana, struct _vector *v, size_t size, unsigned int capacity) {
	memset(v, 0, sizeof(*v));
	v->size = size;
	_vector_reserve(diana, v, capacity);
}

void _vector_push(struct diana *diana, struct _vector *v, void *data) {
	unsigned int length = ++v->length;
	if(length > v->capacity) {
		_vector_reserve(diana, v, (length + 1) * 1.5);
	}
	if(data != NULL) {
		memcpy((unsigned char *)v->data + v->length * v->size, data, v->size);
	}
}

int _vector_pop(struct diana *diana, struct _vector *v, void *r) {
	if(v->length == 0) {
		return 0;
	}
	if(r != NULL) {
		memcpy(r, (unsigned char *)v->data + --v->length * v->size, v->size);
	}
	return 1;
}
*/
// }}}

// {{{ SPARSE INTEGER SETS
/* UNUSED
static int _sparseIntegerSet_contains(struct diana *diana, struct _sparseIntegerSet *is, unsigned int i) {
	if(i >= is->capacity) {
		return 0;
	}
	unsigned int a = is->sparse[i];
	unsigned int n = is->population;
	return a < n && is->dense[a] == i;
}
*/

static int _sparseIntegerSet_insert(struct diana *diana, struct _sparseIntegerSet *is, unsigned int i) {
	if(i >= is->capacity) {
		unsigned int newCapacity = (i + 1) * 1.5;
		is->dense = _realloc(diana, is->dense, is->capacity * sizeof(unsigned int), newCapacity * sizeof(unsigned int));
		is->sparse = _realloc(diana, is->sparse, is->capacity * sizeof(unsigned int), newCapacity * sizeof(unsigned int));
		is->capacity = newCapacity;
	}
	unsigned int a = is->sparse[i];
	unsigned int n = is->population;
	if(a >= n || is->dense[a] != i) {
		is->sparse[i] = n;
		is->dense[n] = i;
		is->population = n + 1;
		return 0;
	}
	return 1;
}

static int _sparseIntegerSet_delete(struct diana *diana, struct _sparseIntegerSet *is, unsigned int i) {
	if(i >= is->capacity || is->population == 0) {
		return 0;
	}
	unsigned int a = is->sparse[i];
	unsigned int n = is->population - 1;
	if(a <= n || is->dense[a] == i) {
		unsigned int e = is->dense[n];
		is->population = n;
		is->dense[n] = e;
		is->sparse[e] = a;
		return 1;
	}
	return 0;
}

static void _sparseIntegerSet_clear(struct diana *diana, struct _sparseIntegerSet *is) {
	is->population = 0;
}

static int _sparseIntegerSet_isEmpty(struct diana *diana, struct _sparseIntegerSet *is) {
	return is->population == 0;
}

static unsigned int _sparseIntegerSet_pop(struct diana *diana, struct _sparseIntegerSet *is) {
	if(is->population >= 1) {
		return is->dense[--is->population];
	}
	return UINT_MAX;
}

static void _sparseIntegerSet_free(struct diana *diana, struct _sparseIntegerSet *is) {
	_free(diana, is->dense);
	_free(diana, is->sparse);
	memset(is, 0, sizeof(*is));
}
// }}}

// {{{ BIT VECTOR
static int _bits_set(unsigned char *bytes, unsigned int bit) {
	int r = !!(bytes[bit >> 3] & (1 << (bit & 7)));
	bytes[bit >> 3] |= (1 << (bit & 7));
	return r;
}

static int _bits_isSet(unsigned char *bytes, unsigned int bit) {
	return !!(bytes[bit >> 3] & (1 << (bit & 7)));
}

static int _bits_clear(unsigned char *bytes, unsigned int bit) {
	int r = !!(bytes[bit >> 3] & (1 << (bit & 7)));
	bytes[bit >> 3] &= ~(1 << (bit & 7));
	return r;
}
// }}}

// {{{ DENSE INTEGER SET
static int _denseIntegerSet_contains(struct diana *diana, struct _denseIntegerSet *is, unsigned int i) {
	return i < is->capacity && _bits_isSet(is->bytes, i);
}

static unsigned int _denseIntegerSet_insert(struct diana *diana, struct _denseIntegerSet *is, unsigned int i) {
	if(i >= is->capacity) {
		size_t newCapacity = (i + 1) * 1.5;
		is->bytes = _realloc(diana, is->bytes, (is->capacity + 7) >> 3, (newCapacity + 7) >> 3);
		is->capacity = newCapacity;
	}

	return _bits_set(is->bytes, i);
}

static unsigned int _denseIntegerSet_delete(struct diana *diana, struct _denseIntegerSet *is, unsigned int i) {
	if(i < is->capacity) {
		return _bits_clear(is->bytes, i);
	}
	return 0;
}

/* UNUSED
static void _denseIntegerSet_clear(struct diana *diana, struct _denseIntegerSet *is) {
	memset(is->bytes, 0, (is->capacity + 7) >> 3);
}

static int _denseIntegerSet_isEmpty(struct diana *diana, struct _denseIntegerSet *is) {
	unsigned int n = (is->capacity + 7) >> 3, i = 0;
	for(; i < n; i++) {
		if(is->bytes[i]) {
			return 0;
		}
	}
	return 1;
}
*/

static void _denseIntegerSet_free(struct diana *diana, struct _denseIntegerSet *is) {
	_free(diana, is->bytes);
	memset(is, 0, sizeof(*is));
}
// }}}

// {{{ COMPONENT
static void _component_free(struct diana *diana, struct _component *component) {
	unsigned int i = 0;
	_free(diana, (void *)component->name);
	for(i = 0; i < component->nextDataIndex; i++) {
		_free(diana, component->data[i]);
	}
	_free(diana, component->data);
	_sparseIntegerSet_free(diana, &component->freeDataIndexes);
#if DL_COMPUTE
	_sparseIntegerSet_free(diana, &component->componentsToDirty);
#endif
	memset(component, 0, sizeof(*component));
}

static int _getAComponentIndex(struct diana *diana, struct _component *c, unsigned int * out) {
	unsigned int index;

	if(_sparseIntegerSet_isEmpty(diana, &c->freeDataIndexes)) {
		if(c->flags & DL_COMPONENT_LIMITED_BIT) {
			return DL_ERROR_FULL_COMPONENT;
		}

		index = c->nextDataIndex++;

		c->data = _realloc(diana, c->data, sizeof(void *) * index, sizeof(void *) * c->nextDataIndex);
		c->data[index] = _malloc(diana, c->size);
	} else {
		index = _sparseIntegerSet_pop(diana, &c->freeDataIndexes);
	}

	*out = index;

	return DL_ERROR_NONE;
}
// }}}

// {{{ SYSTEM
static unsigned char *_data_getEntity(struct diana *diana, unsigned int entity);
static void _data_fix(struct diana *diana);

static void _system_free(struct diana *diana, struct _system *system) {
	_free(diana, (void *)system->name);
	_sparseIntegerSet_free(diana, &system->watch);
	_sparseIntegerSet_free(diana, &system->exclude);
	_denseIntegerSet_free(diana, &system->entities);
	memset(system, 0, sizeof(*system));
}

static void _subscribe(struct diana *diana, struct _system *system, unsigned int entity) {
	int included = _denseIntegerSet_insert(diana, &system->entities, entity);
	if(!included && system->subscribed != NULL) {
		system->subscribed(diana, system->userData, entity);
	}
}

static void _unsubscribe(struct diana *diana, struct _system *system, unsigned int entity) {
	int included = _denseIntegerSet_delete(diana, &system->entities, entity);
	if(included && system->unsubscribed != NULL) {
		system->unsubscribed(diana, system->userData, entity);
	}
}

static void _check(struct diana *diana, struct _system *system, unsigned int entity) {
	unsigned char *entity_components = _data_getEntity(diana, entity);
	unsigned int component, i;
	int wanted = 1;

	FOREACH_SPARSEINTSET(component, i, &system->watch) {
		if(!(entity_components[component >> 3] & (1 << (component & 7)))) {
			wanted = 0;
			break;
		}
	}

	FOREACH_SPARSEINTSET(component, i, &system->exclude) {
		if((entity_components[component >> 3] & (1 << (component & 7)))) {
			wanted = 0;
			break;
		}
	}

	if(wanted) {
		_subscribe(diana, system, entity);
	} else {
		_unsubscribe(diana, system, entity);
	}
}

static void _processSystem(struct diana *diana, struct _system *system, float delta) {
	unsigned int entity;

	__sync_add_and_fetch(&diana->processing, 1);

	if(system->starting != NULL) {
		system->starting(diana, system->userData);
	}
	FOREACH_DENSEINTSET(entity, &system->entities) {
		system->process(diana, system->userData, entity, delta);
	}
	if(system->ending != NULL) {
		system->ending(diana, system->userData);
	}

	if(__sync_sub_and_fetch(&diana->processing, 1) == 0) {
		_data_fix(diana);
	}
}

// }}}

// {{{ MANAGER
static void _manager_free(struct diana *diana, struct _manager *manager) {
	_free(diana, (void *)manager->name);
	memset(manager, 0, sizeof(*manager));
}
// }}}

// {{{ ENTITY
int _spawn(struct diana *diana, unsigned int * result) {
	unsigned int r;

	if(_sparseIntegerSet_isEmpty(diana, &diana->freeEntityIds)) {
		r = diana->nextEntityId++;
	} else {
		r = _sparseIntegerSet_pop(diana, &diana->freeEntityIds);
	}

	diana->dataHeight = diana->dataHeight > (r + 1) ? diana->dataHeight : (r + 1);

	if(diana->dataHeight > diana->dataHeightCapacity) {
		if(diana->processing) {
			void *entityData = _malloc(diana, diana->dataWidth);

			if(entityData == NULL) {
				return 0;
			}

			diana->processingData = _realloc(diana, diana->processingData, sizeof(*diana->processingData) * diana->processingDataHeight, sizeof(*diana->processingData) * (diana->processingDataHeight + 1));

			if(diana->processingData == NULL) {
				return 0;
			}

			diana->processingData[diana->processingDataHeight++] = entityData;
		} else {
			unsigned int newDataHeightCapacity = diana->dataHeight * 1.5;
			diana->data = _realloc(diana, diana->data, diana->dataWidth * diana->dataHeightCapacity, diana->dataWidth * newDataHeightCapacity);
			diana->dataHeightCapacity = newDataHeightCapacity;
		}
	}

	*result = r;

	return DL_ERROR_NONE;
}
// }}}

// {{{ DATA
static unsigned char *_data_getEntity(struct diana *diana, unsigned int entity) {
	if(entity >= diana->dataHeightCapacity) {
		unsigned char *result;
		result = diana->processingData[entity - diana->dataHeightCapacity];
	}
	return (void *)((unsigned char *)diana->data + (diana->dataWidth * entity));
}

static int _data_getComponentCount(struct diana *diana, unsigned int entity, unsigned int component, unsigned int * result) {
	unsigned char *entityData;
	struct _component *c;

	entityData = _data_getEntity(diana, entity);
	c = diana->components + component;

	if(c->flags & DL_COMPONENT_MULTIPLE_BIT) {
		struct _componentBag *bag = (struct _componentBag *)(entityData + c->offset);
		*result = bag->count;
	} else {
		*result = _bits_isSet(entityData, component);
	}

	return DL_ERROR_NONE;
}

static void _data_fix(struct diana *diana) {
	diana->mutex_lock(diana->endpoint_mutex);

	// take care of spawns that happen during processing
	if(diana->processingData != NULL) {
		unsigned int newDataHeight = diana->dataHeight + diana->processingDataHeight, i;

		if(newDataHeight >= diana->dataHeightCapacity) {
			unsigned int newDataHeightCapacity = (newDataHeight + 1) * 1.5;
			diana->data = _realloc(diana, diana->data, diana->dataWidth * diana->dataHeightCapacity, diana->dataWidth * newDataHeightCapacity);
			diana->dataHeightCapacity = newDataHeightCapacity;
		}

		for(i = 0; i < diana->processingDataHeight; i++) {
			memcpy((unsigned char *)diana->data + (diana->dataWidth * (diana->dataHeight + i)), diana->processingData[i], diana->dataWidth);
			diana->free(diana->processingData[i]);
		}
		diana->free(diana->processingData);

		diana->dataHeight = newDataHeight;

		diana->processingData = NULL;
		diana->processingDataHeight = 0;
	}

	diana->mutex_unlock(diana->endpoint_mutex);
}

static int _setComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i, const void * data) {
	unsigned char *entityData = _data_getEntity(diana, entity);
	struct _component *c = diana->components + component;
	int defined = _bits_set(entityData, component);
	void *componentData = NULL;
	int err = DL_ERROR_NONE;

	// can only add a component if its inactive or if it already has that component
	if(_denseIntegerSet_contains(diana, &diana->active, entity) && !defined) {
		DEBUG();
		return DL_ERROR_INVALID_OPERATION;
	}

#if DL_COMPUTE
	if(c->compute) {
		entityData[c->offset - 1] = !defined;
	}
#endif

	if(c->flags & DL_COMPONENT_MULTIPLE_BIT) {
		struct _componentBag *bag = (struct _componentBag *)(entityData + c->offset);
		unsigned int index;

		if(i >= bag->count) {
			err = _getAComponentIndex(diana, c, &index);

			if(err != DL_ERROR_NONE) {
				return err;
			}

			bag->indexes = _realloc(diana, bag->indexes, sizeof(unsigned int) * bag->count, sizeof(unsigned int) * (bag->count + 1));
			bag->indexes[i = bag->count++] = index;
		}

		componentData = (void *)((unsigned char *)c->data[bag->indexes[i]]);
	} else if(c->flags & DL_COMPONENT_INDEXED_BIT) {
		unsigned int *index = (unsigned int *)(entityData + c->offset);

		if(!defined) {
			err = _getAComponentIndex(diana, c, index);

			if(err != DL_ERROR_NONE) {
				return err;
			}
		}

		componentData = (void *)((unsigned char *)c->data[*index]);
	} else {
		componentData = (void *)(entityData + c->offset);
	}

	if(data != NULL) {
		memcpy(componentData, data, c->size);
	}

	return err;
}

static int _getComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i, void ** ptr) {
	unsigned char *entityData = _data_getEntity(diana, entity);
	struct _component *c = diana->components + component;
	void *componentData = NULL;

#if DL_COMPUTE
	unsigned int calculate = 0;
#endif

	if(!_bits_isSet(entityData, component)) {
		return DL_ERROR_INVALID_VALUE;
	}

#if DL_COMPUTE
	if(diana->computingComponentStack) {
		_sparseIntegerSet_insert(diana, &c->componentsToDirty, diana->computingComponentStack->component);
	}

	if(c->compute) {
		if(entityData[c->offset - 1]) {
			calculate = 1;
			entityData[c->offset - 1] = 0;
		}
	}
#endif

	if(c->flags & DL_COMPONENT_MULTIPLE_BIT) {
		struct _componentBag *bag = (struct _componentBag *)(entityData + c->offset);
		if(i >= bag->count) {
			return DL_ERROR_INVALID_VALUE;
		}
		componentData = (void *)((unsigned char *)c->data[bag->indexes[i]]);
	} else if(c->flags & DL_COMPONENT_INDEXED_BIT) {
		unsigned int *index = (unsigned int *)(entityData + c->offset);
		if(*index == UINT_MAX) {
			return DL_ERROR_INVALID_VALUE;
		}
		componentData = (void *)((unsigned char *)c->data[*index]);
	} else {
		componentData = (void *)(entityData + c->offset);
	}

#if DL_COMPUTE
	if(calculate) {
		struct _computingComponentStack ccs;
		ccs.previous = diana->computingComponentStack;
		ccs.component = component;
		diana->computingComponentStack = &ccs;

		c->compute(diana, c->userData, entity, i, componentData);

		diana->computingComponentStack = ccs.previous;
	}
#endif

	*ptr = componentData;

	return DL_ERROR_NONE;
}

static int _removeComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i) {
	unsigned char *entityData = _data_getEntity(diana, entity);
	struct _component *c = diana->components + component;

	if(_denseIntegerSet_contains(diana, &diana->active, entity)) {
		return DL_ERROR_INVALID_OPERATION;
	}

	if(!_bits_clear(entityData, component)) {
		return DL_ERROR_NONE;
	}

	if(c->flags & DL_COMPONENT_MULTIPLE_BIT) {
		struct _componentBag *bag = (struct _componentBag *)(entityData + c->offset);
		if(i >= bag->count) {
			return DL_ERROR_NONE;
		}
		_sparseIntegerSet_insert(diana, &c->freeDataIndexes, bag->indexes[i]);
		memcpy(bag->indexes + i, bag->indexes + i + 1, bag->count - i * sizeof(unsigned int));
		bag->indexes = _realloc(diana, bag->indexes, sizeof(unsigned int) * bag->count, sizeof(unsigned int) * (bag->count - 1));
		bag->count--;
		return DL_ERROR_NONE;
	}

	if(c->flags & DL_COMPONENT_INDEXED_BIT) {
		unsigned int *index = (unsigned int *)(entityData + c->offset);
		_sparseIntegerSet_insert(diana, &c->freeDataIndexes, *index);
		*index = 0;
	}

	return DL_ERROR_NONE;
}
// }}}
// }}}

// {{{ API
// {{{ DIANA
int allocate_diana(void *(*malloc)(size_t), void (*free)(void *), struct diana ** result) {
	*result = malloc(sizeof(**result));

	if(*result == NULL) {
		return DL_ERROR_OUT_OF_MEMORY;
	}

	memset(*result, 0, sizeof(**result));
	(*result)->malloc = malloc;
	(*result)->free = free;

	(*result)->create_mutex = _create_mutex_noop;
	(*result)->mutex_lock = _mutex_noop;
	(*result)->mutex_unlock = _mutex_noop;
	(*result)->mutex_free = _mutex_noop;

	return DL_ERROR_NONE;
}

int diana_free(struct diana *diana) {
	struct _component *component;
	struct _system *system;
	struct _manager *manager;
	unsigned int i, j;

	_data_fix(diana);

	for(i = 0; i < diana->nextEntityId; i++) {
		for(j = 0; j < diana->num_components; j++) {
			diana_removeComponents(diana, i, j);
		}
	}

	_free(diana, diana->data);
	_sparseIntegerSet_free(diana, &diana->freeEntityIds);
	_sparseIntegerSet_free(diana, &diana->added);
	_sparseIntegerSet_free(diana, &diana->enabled);
	_sparseIntegerSet_free(diana, &diana->disabled);
	_sparseIntegerSet_free(diana, &diana->deleted);
	_denseIntegerSet_free(diana, &diana->active);

	FOREACH_ARRAY(component, i, diana->components, diana->num_components) {
		_component_free(diana, component);
	}
	_free(diana, diana->components);

	FOREACH_ARRAY(system, i, diana->systems, diana->num_systems) {
		_system_free(diana, system);
	}
	_free(diana, diana->systems);

	FOREACH_ARRAY(manager, i, diana->managers, diana->num_managers) {
		_manager_free(diana, manager);
	}
	_free(diana, diana->managers);

	diana->free(diana);

	return DL_ERROR_NONE;
}

int diana_initialize(struct diana *diana) {
	unsigned int extraBytes = (diana->num_components + 7) >> 3, n;
	struct _component *c;
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	FOREACH_ARRAY(c, n, diana->components, diana->num_components) {
		c->offset += extraBytes;
	}

	diana->dataWidth += extraBytes;

	diana->initialized = 1;

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_mutexFunctions(struct diana *diana, void *(*create_mutex)(void), void (*mutex_lock)(void *), void (*mutex_unlock)(void *), void (*mutex_free)(void *)) {
	if(diana->initialized || diana->processing) {
		return DL_ERROR_INVALID_OPERATION;
	}

	if(create_mutex == NULL || mutex_lock == NULL || mutex_unlock == NULL || mutex_free == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_free(diana->endpoint_mutex);

	diana->create_mutex = create_mutex;
	diana->mutex_lock = mutex_lock;
	diana->mutex_unlock = mutex_unlock;
	diana->mutex_free = mutex_free;

	diana->endpoint_mutex = diana->create_mutex();

	return DL_ERROR_NONE;
}
// }}}

// {{{ COMPONENT
int diana_createComponent(
	struct diana *diana,
	const char *name,
 	size_t size,
 	unsigned int flags,
	unsigned int * result
) {
	struct _component c;
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	memset(&c, 0, sizeof(c));
	c.name = _strdup(diana, name);
	if(c.name == NULL) {
		err = DL_ERROR_NONE;
		goto unlock;
	}
	c.size = size;
	c.flags = flags;
	c.offset = diana->dataWidth;

	if(flags & DL_COMPONENT_MULTIPLE_BIT) {
		size = sizeof(struct _componentBag);
	} else if(flags & DL_COMPONENT_INDEXED_BIT) {
		size = sizeof(unsigned int);
	}

	diana->dataWidth += size;

	if(flags & DL_COMPONENT_LIMITED_BIT) {
		unsigned int count = (flags >> 3);
		c.data = _malloc(diana, c.size * count);
		while(count) {
			_sparseIntegerSet_insert(diana, &c.freeDataIndexes, count--);
		}
	}

	diana->components = _realloc(diana, diana->components, sizeof(*diana->components) * diana->num_components, sizeof(*diana->components) * (diana->num_components + 1));
	diana->components[diana->num_components++] = c;

	*result = diana->num_components - 1;

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

#if DL_COMPUTE
int diana_componentCompute(struct diana *diana, unsigned int component, void (*compute)(struct diana *, void *, unsigned int entity, unsigned int index, void *), void *userData) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	diana->components[component].compute = compute;
	diana->components[component].userData = userData;
	diana->dataWidth += sizeof(char);

	// give this component room
	while(component < diana->num_components) {
		diana->components[component].offset += sizeof(char);
		component++;
	}

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}
#endif
// }}}

// {{{ SYSTEM
int diana_createSystem(
	struct diana *diana,
	const char *name,
	void (*starting)(struct diana *, void *),
	void (*process)(struct diana *, void *, unsigned int, float),
	void (*ending)(struct diana *, void *),
	void (*subscribed)(struct diana *, void *, unsigned int),
	void (*unsubscribed)(struct diana *, void *, unsigned int),
	void *userData,
	unsigned int flags,
	unsigned int * result
) {
	struct _system s;
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	memset(&s, 0, sizeof(s));
	s.name = _strdup(diana, name);
	if(s.name == NULL) {
		err = DL_ERROR_OUT_OF_MEMORY;
	}
	s.starting = starting;
	s.process = process;
	s.ending = ending;
	s.subscribed = subscribed;
	s.unsubscribed = unsubscribed;
	s.userData = userData;
	s.flags = flags;

	diana->systems = _realloc(diana, diana->systems, sizeof(*diana->systems) * diana->num_systems, sizeof(*diana->systems) * (diana->num_systems + 1));
	diana->systems[diana->num_systems++] = s;

	*result = diana->num_systems - 1;

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_watch(struct diana *diana, unsigned int system, unsigned int component) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if(system >= diana->num_systems) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	_sparseIntegerSet_insert(diana, &diana->systems[system].watch, component);

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_exclude(struct diana *diana, unsigned int system, unsigned int component) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if(system >= diana->num_systems) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	_sparseIntegerSet_insert(diana, &diana->systems[system].exclude, component);

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}
// }}}

// {{{ MANAGER
int diana_createManager(
	struct diana *diana,
	const char *name,
	void (*added)(struct diana *, void *, unsigned int),
	void (*enabled)(struct diana *, void *, unsigned int),
	void (*disabled)(struct diana *, void *, unsigned int),
	void (*deleted)(struct diana *, void *, unsigned int),
	void *userData,
	unsigned int flags,
	unsigned int * result
) {
	struct _manager m;
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	memset(&m, 0, sizeof(m));
	m.name = _strdup(diana, name);
	if(m.name == NULL) {
		err = DL_ERROR_OUT_OF_MEMORY;
		goto unlock;
	}
	m.added = added;
	m.enabled = enabled;
	m.disabled = disabled;
	m.deleted = deleted;
	m.userData = userData;
	m.flags = flags;

	diana->managers = _realloc(diana, diana->managers, sizeof(*diana->managers) * diana->num_managers, sizeof(*diana->managers) * (diana->num_managers + 1));
	diana->managers[diana->num_managers++] = m;

	*result = diana->num_managers - 1;

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}
// }}}

// {{{ PROCESSING
int diana_process(struct diana *diana, float delta) {
	unsigned int i, j;
	struct _system *system;
	struct _manager *manager;
	int *added, num_added;
	int *enabled, num_enabled;
	int *disabled, num_disabled;
	int *deleted, num_deleted;

	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);
	
		if(!diana->initialized) {
			diana->mutex_unlock(diana->endpoint_mutex);
			return DL_ERROR_INVALID_OPERATION;
		}

		num_added = diana->added.population;
		added = _malloc(diana, sizeof(unsigned int) * num_added);
		memcpy(added, diana->added.dense, sizeof(unsigned int) * num_added);
		_sparseIntegerSet_clear(diana, &diana->added);

		num_enabled = diana->enabled.population;
		enabled = _malloc(diana, sizeof(unsigned int) * num_enabled);
		memcpy(enabled, diana->enabled.dense, sizeof(unsigned int) * num_enabled);
		_sparseIntegerSet_clear(diana, &diana->enabled);

		num_disabled = diana->disabled.population;
		disabled = _malloc(diana, sizeof(unsigned int) * num_disabled);
		memcpy(disabled, diana->disabled.dense, sizeof(unsigned int) * num_disabled);
		_sparseIntegerSet_clear(diana, &diana->disabled);

		num_deleted = diana->deleted.population;
		deleted = _malloc(diana, sizeof(unsigned int) * num_deleted);
		memcpy(deleted, diana->deleted.dense, sizeof(unsigned int) * num_deleted);
		_sparseIntegerSet_clear(diana, &diana->deleted);

	diana->mutex_unlock(diana->endpoint_mutex);

	for(i = 0; i < num_added; i++) {
		unsigned int entity = added[i];
		FOREACH_ARRAY(manager, j, diana->managers, diana->num_managers) {
			if(manager->added != NULL) {
				manager->added(diana, manager->userData, entity);
			}
		}
	}

	for(i = 0; i < num_enabled; i++) {
		unsigned int entity = enabled[i];
		FOREACH_ARRAY(system, j, diana->systems, diana->num_systems) {
			_check(diana, system, entity);
		}
		FOREACH_ARRAY(manager, j, diana->managers, diana->num_managers) {
			if(manager->enabled != NULL) {
				manager->enabled(diana, manager->userData, entity);
			}
		}
	}

	for(i = 0; i < num_disabled; i++) {
		unsigned int entity = disabled[i];
		FOREACH_ARRAY(system, j, diana->systems, diana->num_systems) {
			_unsubscribe(diana, system, entity);
		}
		FOREACH_ARRAY(manager, j, diana->managers, diana->num_managers) {
			if(manager->disabled != NULL) {
				manager->disabled(diana, manager->userData, entity);
			}
		}
	}

	for(i = 0; i < num_deleted; i++) {
		unsigned int entity = deleted[i];
		FOREACH_ARRAY(system, j, diana->systems, diana->num_systems) {
			_unsubscribe(diana, system, entity);
		}
		FOREACH_ARRAY(manager, j, diana->managers, diana->num_managers) {
			if(manager->deleted != NULL) {
				manager->deleted(diana, manager->userData, entity);
			}
		}
		for(j = 0; j < diana->num_components; j++) {
			diana_removeComponents(diana, entity, j);
		}
	}

	diana->mutex_lock(diana->endpoint_mutex);

		for(i = 0; i < num_enabled; i++) {
			unsigned int entity = enabled[i];
			_denseIntegerSet_insert(diana, &diana->active, entity);
		}
		for(i = 0; i < num_disabled; i++) {
			unsigned int entity = disabled[i];
			_denseIntegerSet_delete(diana, &diana->active, entity);
		}
		for(i = 0; i < num_deleted; i++) {
			unsigned int entity = deleted[i];
			_sparseIntegerSet_insert(diana, &diana->freeEntityIds, entity);
		}

	diana->mutex_unlock(diana->endpoint_mutex);

	_free(diana, added);
	_free(diana, enabled);
	_free(diana, disabled);
	_free(diana, deleted);

	FOREACH_ARRAY(system, j, diana->systems, diana->num_systems) {
		if(system->flags & DL_SYSTEM_PASSIVE_BIT) {
			continue;
		}
		_processSystem(diana, system, delta);
	}

	return err;
}

int diana_processSystem(struct diana *diana, unsigned int system, float delta) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

		if(!diana->initialized) {
			diana->mutex_unlock(diana->endpoint_mutex);
			return DL_ERROR_INVALID_OPERATION;
		}

		if(system >= diana->num_systems) {
			diana->mutex_unlock(diana->endpoint_mutex);
			return DL_ERROR_INVALID_VALUE;
		}

	diana->mutex_unlock(diana->endpoint_mutex);

	_processSystem(diana, diana->systems + system, delta);

	return err;
}
// }}}

// {{{ ENTITY
int diana_spawn(struct diana *diana, unsigned int * result) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	err = _spawn(diana, result);

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_clone(struct diana *diana, unsigned int parentEntity, unsigned int * result) {
	unsigned int newEntity, ci, cbi, cbn;
	struct _component *c;
	unsigned char *parentEntityData;
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && parentEntity >= diana->dataHeight) || (diana->processing && parentEntity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	err = _spawn(diana, &newEntity);
	if(err != DL_ERROR_NONE) {
		goto unlock;
	}

	parentEntityData = _data_getEntity(diana, parentEntity);

	for(ci = 0; ci < diana->num_components; ci++) {
		if(!_bits_isSet(parentEntityData, ci)) {
			continue;
		}

		c = diana->components + ci;

		err = _data_getComponentCount(diana, parentEntity, ci, &cbn);
		if(err != DL_ERROR_NONE) {
			goto unlock;
		}

		for(cbi = 0; cbi < cbn; cbi++) {
			void *cd;
			err = _getComponentI(diana, parentEntity, ci, cbi, (void **)&cd);
			if(err) {
				goto unlock;
			}
			_setComponentI(diana, newEntity, ci, cbi, cd);
		}
	}

	*result = newEntity;

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_signal(struct diana *diana, unsigned int entity, unsigned int signal) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	switch(signal) {
	case DL_ENTITY_ADDED:
		_sparseIntegerSet_insert(diana, &diana->added, entity);
		_sparseIntegerSet_insert(diana, &diana->enabled, entity);
		_sparseIntegerSet_delete(diana, &diana->disabled, entity);
		_sparseIntegerSet_delete(diana, &diana->deleted, entity);
		break;
	case DL_ENTITY_ENABLED:
		_sparseIntegerSet_insert(diana, &diana->enabled, entity);
		_sparseIntegerSet_delete(diana, &diana->disabled, entity);
		_sparseIntegerSet_delete(diana, &diana->deleted, entity);
		break;
	case DL_ENTITY_DISABLED:
		_sparseIntegerSet_delete(diana, &diana->enabled, entity);
		_sparseIntegerSet_insert(diana, &diana->disabled, entity);
		_sparseIntegerSet_delete(diana, &diana->deleted, entity);
		break;
	case DL_ENTITY_DELETED:
		_sparseIntegerSet_delete(diana, &diana->added, entity);
		_sparseIntegerSet_delete(diana, &diana->enabled, entity);
		_sparseIntegerSet_insert(diana, &diana->disabled, entity);
		_sparseIntegerSet_insert(diana, &diana->deleted, entity);
		break;
	default:
		err = DL_ERROR_INVALID_VALUE;
	}

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}
// }}}

// {{{ DATA
// {{{ single
int diana_setComponent(struct diana *diana, unsigned int entity, unsigned int component, const void * data) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		return DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		return DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	err = _setComponentI(diana, entity, component, 0, data);

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_getComponent(struct diana *diana, unsigned int entity, unsigned int component, void ** out) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	err = _getComponentI(diana, entity, component, 0, out);

unlock:
	diana->mutex_lock(diana->endpoint_mutex);

	return err;
}

#if DL_COMPUTE
int diana_dirtyComponent(struct diana *diana, unsigned int entity, unsigned int component) {
	struct _component *c;
	unsigned char *entityData;
	unsigned int i, ci;
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	entityData = _data_getEntity(diana, entity);
	c = diana->components + component;

	FOREACH_SPARSEINTSET(ci, i, &c->componentsToDirty) {
		struct _component *c2 = diana->components + ci;
		entityData[c2->offset - 1] = 1;
	}

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}
#endif

int diana_removeComponent(struct diana *diana, unsigned int entity, unsigned int component) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	err = _removeComponentI(diana, entity, component, 0);

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}
// }}}

// {{{ multiple
int diana_getComponentCount(struct diana *diana, unsigned int entity, unsigned int component, unsigned int * result) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	err = _data_getComponentCount(diana, entity, component, result);

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_appendComponent(struct diana *diana, unsigned int entity, unsigned int component, const void * data) {
	unsigned char *entityData;
	struct _component *c;
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		DEBUG();
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		DEBUG();
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		DEBUG();
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	entityData = _data_getEntity(diana, entity);
	c = diana->components + component;

	if(c->flags & DL_COMPONENT_MULTIPLE_BIT) {
		unsigned int cbn;
		_data_getComponentCount(diana, entity, component, &cbn);
		err = _setComponentI(diana, entity, component, cbn, data);
	} else {
		err = _setComponentI(diana, entity, component, 0, data);
	}

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_removeComponents(struct diana *diana, unsigned int entity, unsigned int component) {
	unsigned char *entityData;
	struct _component *c;
	unsigned int i;
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	entityData = _data_getEntity(diana, entity);
	c = diana->components + component;

	if(c->flags & DL_COMPONENT_MULTIPLE_BIT) {
		struct _componentBag *bag = (struct _componentBag *)(entityData + c->offset);
		if(bag->count) {
			for(i = 0; i < bag->count; i++) {
				_sparseIntegerSet_insert(diana, &c->freeDataIndexes, bag->indexes[i]);
			}
			bag->count = 0;
			diana->free(bag->indexes);
			bag->indexes = NULL;
		}
	} else {
		_removeComponentI(diana, entity, component, 0);
	}

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}
// }}}

// {{{ low level
int diana_setComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i, const void * data) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	err = _setComponentI(diana, entity, component, i, data);

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_getComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i, void ** out) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	err = _getComponentI(diana, entity, component, i, out);

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}

int diana_removeComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i) {
	int err = DL_ERROR_NONE;

	if(diana == NULL) {
		return DL_ERROR_INVALID_VALUE;
	}

	diana->mutex_lock(diana->endpoint_mutex);

	if(!diana->initialized) {
		err = DL_ERROR_INVALID_OPERATION;
		goto unlock;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	if(component >= diana->num_components) {
		err = DL_ERROR_INVALID_VALUE;
		goto unlock;
	}

	err = _removeComponentI(diana, entity, component, i);

unlock:
	diana->mutex_unlock(diana->endpoint_mutex);

	return err;
}
// }}}
// }}}
// }}}
