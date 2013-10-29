// vim: ts=2:sw=2:noexpandtab

#include "diana.h"

#include <string.h>
#include <limits.h>

static void *_realloc(struct diana *diana, void *ptr, size_t oldSize, size_t newSize);

// ============================================================================
// SPARSE INTEGER SET
// - faster clearing
// - easy tracking of population
// - can be used to 'pop' elements off
// - and iterating over
// - used for delaying entity adding, enabling, disabled and deleting
// - used by each system to track entities it has
struct _sparseIntegerSet {
	DLuint *dense;
	DLuint *sparse;
	DLuint population;
	DLuint capacity;
};

static DLboolean _sparseIntegerSet_contains(struct diana *diana, struct _sparseIntegerSet *is, DLuint i) {
	if(i >= is->capacity) {
		return DL_FALSE;
	}
	DLuint a = is->sparse[i];
	DLuint n = is->population;
	return a < n && is->dense[a] == i;
}

static void _sparseIntegerSet_insert(struct diana *diana, struct _sparseIntegerSet *is, DLuint i) {
	if(i >= is->capacity) {
		DLuint newCapacity = (i + 1) * 1.5;
		is->dense = _realloc(diana, is->dense, is->capacity * sizeof(DLuint), newCapacity * sizeof(DLuint));
		is->sparse = _realloc(diana, is->sparse, is->capacity * sizeof(DLuint), newCapacity * sizeof(DLuint));
		is->capacity = newCapacity;
	}
	DLuint a = is->sparse[i];
	DLuint n = is->population;
	if(a >= n || is->dense[a] != i) {
		is->sparse[i] = n;
		is->dense[n] = i;
		is->population = n + 1;
	}
}

static void _sparseIntegerSet_delete(struct diana *diana, struct _sparseIntegerSet *is, DLuint i) {
	if(i >= is->capacity) {
		return;
	}
	DLuint a = is->sparse[i];
	DLuint n = is->population - 1;
	if(a <= n || is->dense[a] == i) {
		DLuint e = is->dense[n];
		is->population = n;
		is->dense[n] = e;
		is->sparse[e] = a;
	}
}

static void _sparseIntegerSet_clear(struct diana *diana, struct _sparseIntegerSet *is) {
	is->population = 0;
}

static DLboolean _sparseIntegerSet_isEmpty(struct diana *diana, struct _sparseIntegerSet *is) {
	return is->population == 0;
}

static DLuint _sparseIntegerSet_pop(struct diana *diana, struct _sparseIntegerSet *is) {
	if(is->population) {
		return is->dense[--is->population];
	}
	return UINT_MAX;
}

// ============================================================================
// DENSE INTEGER SET
// - more memory effecient
// - used for tracking components each entity is using
// - and for the active entities
static void _bits_set(DLubyte *bytes, DLuint bit) {
	bytes[bit >> 3] |= (1 << (bit & 7));
}

static DLboolean _bits_isSet(DLubyte *bytes, DLuint bit) {
	return !!(bytes[bit >> 3] & (1 << (bit & 7)));
}

static void _bits_clear(DLubyte *bytes, DLuint bit) {
	bytes[bit >> 3] &= ~(1 << (bit & 7));
}

// ============================================================================

struct _denseIntegerSet {
	DLubyte *bytes;
	DLuint capacity;
};

static DLboolean _denseIntegerSet_contains(struct diana *diana, struct _denseIntegerSet *is, DLuint i) {
	return i < is->capacity && _bits_isSet(is->bytes, i);
}

static void _denseIntegerSet_insert(struct diana *diana, struct _denseIntegerSet *is, DLuint i) {
	if(i >= is->capacity) {
		DLuint newCapacity = (i + 1) * 1.5;
		is->bytes = _realloc(diana, is->bytes, (is->capacity + 7) >> 3, (newCapacity + 7) >> 3);
		is->capacity = newCapacity;
	}
	_bits_set(is->bytes, i);
}

static void _denseIntegerSet_delete(struct diana *diana, struct _denseIntegerSet *is, DLuint i) {
	if(i < is->capacity) {
		_bits_clear(is->bytes, i);
	}
}

static void _denseIntegerSet_clear(struct diana *diana, struct _denseIntegerSet *is) {
	memset(is->bytes, 0, (is->capacity + 7) >> 3);
}

static DLboolean _denseIntegerSet_isEmpty(struct diana *diana, struct _denseIntegerSet *is) {
	DLuint n = (is->capacity + 7) >> 3, i = 0;
	for(; i < n; i++) {
		if(is->bytes[i]) {
			return DL_FALSE;
		}
	}
	return DL_TRUE;
}

// ============================================================================
// PRIMARY DATA
struct _component {
	const char *name;
	size_t size;
	size_t offset;
};

struct _system {
	const char *name;
	void (*started)(struct diana *);
	void (*process)(struct diana *, DLuint entity, DLfloat delta);
	void (*ended)(struct diana *);
	void (*subscribed)(struct diana *, DLuint entity);
	void (*unsubscribed)(struct diana *, DLuint entity);
	struct _sparseIntegerSet watch;
	struct _sparseIntegerSet entities;
};

struct diana {
	void *(*malloc)(size_t);
	void (*free)(void *);

	DLenum error;
	DLboolean initialized;
	DLboolean processing;

	// manage the entity ids
	// reuse deleted entity ids
	struct _sparseIntegerSet freeEntityIds;
	DLuint nextEntityId;

	// entity data
	// first 'column' is bits of components defined
	// the rest are the components
	DLuint dataWidth;
	DLuint dataHeight;
	DLuint dataHeightCapacity;
	void *data;

	DLuint processingDataHeight;
	void **processingData;

	// buffer entity status notifications
	struct _sparseIntegerSet added;
	struct _sparseIntegerSet enabled;
	struct _sparseIntegerSet disabled;
	struct _sparseIntegerSet deleted;

	// all active entities (added and enabled)
	struct _denseIntegerSet active;

	DLuint num_components;
	struct _component *components;

	DLuint num_systems;
	struct _system *systems;

#ifdef DIANA_MANAGER_ENABLED
	DLuint num_managers;
	struct _managers *managers;
#endif
};

// ============================================================================
// UTILITY
static void *_malloc(struct diana *diana, size_t size) {
	void *r = diana->malloc(size);
	if(r == NULL) {
		diana->error = DL_ERROR_OUT_OF_MEMORY;
		return NULL;
	}
	memset(r, 0, size);
	return r;
}

static char *_strdup(struct diana *diana, const char *s) {
	unsigned int l = strlen(s);
	char *r = diana->malloc(l + 1);
	if(r == NULL) {
		diana->error = DL_ERROR_OUT_OF_MEMORY;
		return NULL;
	}
	memcpy(r, s, l + 1);
	return r;
}

static void *_realloc(struct diana *diana, void *ptr, size_t oldSize, size_t newSize) {
	if(oldSize == newSize) {
		return ptr;
	}
	void *r = diana->malloc(newSize);
	if(r == NULL) {
		diana->error = DL_ERROR_OUT_OF_MEMORY;
		return NULL;
	}
	if(oldSize < newSize) {
		memset((unsigned char *)r + oldSize, 0, newSize - oldSize);
	}
	if(ptr != NULL) {
		memcpy(r, ptr, oldSize < newSize ? oldSize : newSize);
		diana->free(ptr);
	}
	return r;
}

// ============================================================================
// DIANA
struct diana *allocate_diana(void *(*malloc)(size_t), void (*free)(void *)) {
	struct diana *r = malloc(sizeof(*r));
	if(r != NULL) {
		memset(r, 0, sizeof(*r));
		r->malloc = malloc;
		r->free = free;
	}
	return r;
}

DLenum diana_getError(struct diana *diana) {
	DLenum r = diana->error;
	diana->error = DL_ERROR_NONE;
	return r;
}

#define FOREACH_SPARSEINTSET(I, N, S) for(N = 0, I = (S)->dense ? (S)->dense[N] : 0; N < (S)->population; N++, I = (S)->dense[N])
#define FOREACH_ARRAY(T, N, A, S) for(N = 0, T = A; N < S; N++, T++)

void diana_initialize(struct diana *diana) {
	DLuint extraBytes = (diana->num_components + 7) >> 3, n;
	struct _component *c;

	if(diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	FOREACH_ARRAY(c, n, diana->components, diana->num_components) {
		c->offset += extraBytes;
	}

	diana->initialized = DL_TRUE;
}

static void _check(struct diana *, struct _system *, DLuint entity);
static void _remove(struct diana *, struct _system *, DLuint entity);

void diana_process(struct diana *diana, DLfloat delta) {
	DLuint entity, i, j;
	struct _system *system;

	diana->processing = DL_TRUE;

	FOREACH_SPARSEINTSET(entity, i, &diana->added) {
		/*
		FOREACH_ARRAY(system, j, diana->systems, diana->num_systems) {
			_check(diana, system, entity);
		}
		*/
	}
	_sparseIntegerSet_clear(diana, &diana->added);

	FOREACH_SPARSEINTSET(entity, i, &diana->enabled) {
		FOREACH_ARRAY(system, j, diana->systems, diana->num_systems) {
			_check(diana, system, entity);
		}
		_denseIntegerSet_insert(diana, &diana->active, entity);
	}
	_sparseIntegerSet_clear(diana, &diana->enabled);

	FOREACH_SPARSEINTSET(entity, i, &diana->disabled) {
		FOREACH_ARRAY(system, j, diana->systems, diana->num_systems) {
			_remove(diana, system, entity);
		}
		_denseIntegerSet_delete(diana, &diana->active, entity);
	}
	_sparseIntegerSet_clear(diana, &diana->disabled);

	FOREACH_SPARSEINTSET(entity, i, &diana->deleted) {
		FOREACH_ARRAY(system, j, diana->systems, diana->num_systems) {
			_remove(diana, system, entity);
		}
		_sparseIntegerSet_insert(diana, &diana->freeEntityIds, entity);
	}
	_sparseIntegerSet_clear(diana, &diana->deleted);

	FOREACH_ARRAY(system, j, diana->systems, diana->num_systems) {
		if(system->started != NULL) {
			system->started(diana);
		}
		FOREACH_SPARSEINTSET(entity, i, &system->entities) {
			system->process(diana, entity, delta);
		}
		if(system->ended != NULL) {
			system->ended(diana);
		}
	}

	diana->processing = DL_FALSE;

	// take care of spawns that happen during processing
	if(diana->processingData != NULL) {
		DLuint newDataHeight = diana->dataHeight + diana->processingDataHeight, i;

		if(newDataHeight >= diana->dataHeightCapacity) {
			DLuint newDataHeightCapacity = (newDataHeight + 1) * 1.5;
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
}

void diana_free(struct diana *diana) {
	// TODO
}

// ============================================================================
// COMPONENT
DLuint diana_registerComponent(struct diana *diana, const char *name, size_t size) {
	struct _component c;

	if(diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return 0;
	}

	memset(&c, 0, sizeof(c));
	c.name = _strdup(diana, name);
	if(c.name == NULL) {
		return 0;
	}
	c.size = size;
	c.offset = diana->dataWidth;
	diana->dataWidth += size;

	diana->components = _realloc(diana, diana->components, sizeof(*diana->components) * diana->num_components, sizeof(*diana->components) * (diana->num_components + 1));
	diana->components[diana->num_components++] = c;

	return diana->num_components - 1;
}

// ============================================================================
// SYSTEM
DLuint diana_registerSystem(struct diana *diana, const char *name, void (*process)(struct diana *, DLuint entity, DLfloat delta)) {
	struct _system s;

	if(diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return 0;
	}

	memset(&s, 0, sizeof(s));
	s.name = _strdup(diana, name);
	if(s.name == NULL) {
		return 0;
	}
	s.process = process;

	diana->systems = _realloc(diana, diana->systems, sizeof(*diana->systems) * diana->num_systems, sizeof(*diana->systems) * (diana->num_systems + 1));
	diana->systems[diana->num_systems++] = s;

	return diana->num_systems - 1;
}

void diana_setSystemProcessCallback(struct diana *diana, DLuint system, void (*process)(struct diana *, DLuint entity, DLfloat delta)) {
	if(system >= diana->num_systems) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}

	diana->systems[system].process = process;
}

void diana_setSystemProcessCallbacks(struct diana *diana, DLuint system, void (*started)(struct diana *), void (*process)(struct diana *, DLuint entity, DLfloat delta), void (*ended)(struct diana *)) {
	if(system >= diana->num_systems) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}

	diana->systems[system].started = started;
	diana->systems[system].process = process;
	diana->systems[system].ended = ended;
}

void diana_setSystemEventCallback(struct diana *diana, DLuint system, DLenum event, void (*callback)(struct diana *, DLuint entity)) {
	if(system >= diana->num_systems) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}

	switch(event) {
	case DL_SUBSCRIBED:
		diana->systems[system].subscribed = callback;
		break;
	case DL_UNSUBSCRIBED:
		diana->systems[system].unsubscribed = callback;
		break;
	default:
		diana->error = DL_ERROR_INVALID_VALUE;
		break;
	}
}

void diana_watch(struct diana *diana, DLuint system, DLuint component) {
	if(diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	if(system >= diana->num_systems) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}

	if(component >= diana->num_components) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}

	_sparseIntegerSet_insert(diana, &diana->systems[system].watch, component);
}

static void *_getData(struct diana *, DLuint);

static void _check(struct diana *diana, struct _system *system, DLuint entity) {
	DLboolean already_included = _sparseIntegerSet_contains(diana, &system->entities, entity), wanted;
	DLubyte *entity_components = (DLubyte *)_getData(diana, entity);
	DLuint component, i;

	wanted = DL_TRUE;

	FOREACH_SPARSEINTSET(component, i, &system->watch) {
		if(!(entity_components[component >> 3] & (1 << (component & 7)))) {
			wanted = DL_FALSE;
			break;
		}
	}

	if(already_included && !wanted) {
		_remove(diana, system, entity);
		if(system->unsubscribed != NULL) {
			system->unsubscribed(diana, entity);
		}
		return;
	}

	if(wanted && !already_included) {
		_sparseIntegerSet_insert(diana, &system->entities, entity);
		if(system->subscribed != NULL) {
			system->subscribed(diana, entity);
		}
		return;
	}
}

static void _remove(struct diana *diana, struct _system *system, DLuint entity) {
	_sparseIntegerSet_delete(diana, &system->entities, entity);
}

// ============================================================================
// MANAGER
/*
DLuint diana_registerManager(struct diana *diana, const char *name) {
}

void diana_observe(struct diana *diana, DLuint manager, DLenum callback, void (*function)(struct diana *diana, DLuint manager, DLenum callback, DLuint entity)) {
}
*/

// ============================================================================
// ENTITY
static void *_getData(struct diana *diana, DLuint entity) {
	if(entity >= diana->dataHeightCapacity) {
		return diana->processingData[entity - diana->dataHeightCapacity];
	}
	return (void *)((unsigned char *)diana->data + (diana->dataWidth * entity));
}

DLuint diana_spawn(struct diana *diana) {
	DLuint r;

	if(!diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return 0;
	}

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
			DLuint newDataHeightCapacity = diana->dataHeight * 1.5;
			diana->data = _realloc(diana, diana->data, diana->dataWidth * diana->dataHeightCapacity, diana->dataWidth * newDataHeightCapacity);
			diana->dataHeightCapacity = newDataHeightCapacity;
		}
	}

	return r;
}

void diana_setComponent(struct diana *diana, DLuint entity, DLuint component, void *data) {
	DLubyte *entity_data;

	if(!diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}

	if(component >= diana->num_components) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}

	entity_data = (DLubyte *)_getData(diana, entity);

	// can only add a component if its inactive
	// no error if the component is already set
	if(_denseIntegerSet_contains(diana, &diana->active, entity) && !_bits_isSet(entity_data, component)) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	_bits_set(entity_data, component);

	if(data != NULL) {
		struct _component *c = &diana->components[component];
		memcpy(entity_data + c->offset, data, c->size);
	}
}

void *diana_getComponent(struct diana *diana, DLuint entity, DLuint component) {
	DLubyte *entity_data;

	if(!diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return NULL;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return NULL;
	}

	if(component >= diana->num_components) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return NULL;
	}

	entity_data = (DLubyte *)_getData(diana, entity);

	if(!_bits_isSet(entity_data, component)) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return NULL;
	}

	return entity_data + diana->components[component].offset;
}

void diana_removeComponent(struct diana *diana, DLuint entity, DLuint component) {
	struct _component *c;
	DLubyte *components;

	if(!diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	if((!diana->processing && entity >= diana->dataHeight) || (diana->processing && entity >= diana->dataHeightCapacity + diana->processingDataHeight)) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}

	if(component >= diana->num_components) {
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}

	// can only remove a component from an inactive entity
	// no error if the component is not already set
	if(_denseIntegerSet_contains(diana, &diana->active, entity)) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	components = (DLubyte *)_getData(diana, entity);
	_bits_clear(components, component);
}

void diana_add(struct diana *diana, DLuint entity) {
	if(!diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	// entities start enabled
	_sparseIntegerSet_insert(diana, &diana->added, entity);
	_sparseIntegerSet_insert(diana, &diana->enabled, entity);
	_sparseIntegerSet_delete(diana, &diana->disabled, entity);
	_sparseIntegerSet_delete(diana, &diana->deleted, entity);
}

void diana_enable(struct diana *diana, DLuint entity) {
	if(!diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	// if it was disabled or deleted, it no longer is
	_sparseIntegerSet_insert(diana, &diana->enabled, entity);
	_sparseIntegerSet_delete(diana, &diana->disabled, entity);
	_sparseIntegerSet_delete(diana, &diana->deleted, entity);
}

void diana_disable(struct diana *diana, DLuint entity) {
	if(!diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	// if it was enabled or deleted, it no longer is
	_sparseIntegerSet_delete(diana, &diana->enabled, entity);
	_sparseIntegerSet_insert(diana, &diana->disabled, entity);
	_sparseIntegerSet_delete(diana, &diana->deleted, entity);
}

void diana_delete(struct diana *diana, DLuint entity) {
	if(!diana->initialized) {
		diana->error = DL_ERROR_INVALID_OPERATION;
		return;
	}

	// stop all processing on this entity
	_sparseIntegerSet_delete(diana, &diana->added, entity);
	_sparseIntegerSet_delete(diana, &diana->enabled, entity);
	_sparseIntegerSet_insert(diana, &diana->disabled, entity);
	_sparseIntegerSet_insert(diana, &diana->deleted, entity);
}

// ============================================================================
// INSPECT
DLint diana_getI(struct diana *diana, DLenum property) {
	switch(property) {
	case DL_NUM_COMPONENTS:
		return diana->num_components;
	case DL_NUM_SYSTEMS:
		return diana->num_systems;
#ifdef DIANA_MANAGER_ENABLED
	case DL_NUM_MANAGERS:
		return diana->num_managers;
#endif
	default:
		diana->error = DL_ERROR_INVALID_VALUE;
		return 0;
	}
}

void diana_getIV(struct diana *diana, DLenum property, DLint *iv) {
	switch(property) {
	case DL_NUM_COMPONENTS:
		*iv = diana->num_components;
		break;
	case DL_NUM_SYSTEMS:
		*iv = diana->num_systems;
		break;
#ifdef DIANA_MANAGER_ENABLED
	case DL_NUM_MANAGERS:
		*iv = diana->num_managers;
		break;
#endif
	default:
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}
}

void const * diana_getP(struct diana *diana, DLenum property) {
	switch(property) {
	default:
		diana->error = DL_ERROR_INVALID_VALUE;
		return NULL;
	}
}

void diana_getPV(struct diana *diana, DLenum property, void const * * pv) {
	switch(property) {
	default:
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}
}

DLint diana_getObjectI(struct diana *diana, DLuint object, DLenum property) {
	switch(property) {
	case DL_COMPONENT_SIZE:
		if(object < diana->num_components) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return 0;
		}
		return diana->components[object].size;
	case DL_SYSTEM_NUM_WATCHES:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return 0;
		}
		return diana->systems[object].watch.population;
	case DL_SYSTEM_NUM_ENTITIES:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return 0;
		}
		return diana->systems[object].entities.population;
#ifdef DIANA_MANAGER_ENABLED
	case DL_MANAGER_NUM_OBSERVE:
#endif
	default:
		diana->error = DL_ERROR_INVALID_VALUE;
		return 0;
	}
}

void diana_getObjectIV(struct diana *diana, DLuint object, DLenum property, DLint *iv) {
	DLuint i, j;
	switch(property) {
	case DL_COMPONENT_SIZE:
		if(object >= diana->num_components) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return;
		}
		*iv = diana->components[object].size;
		break;
	case DL_SYSTEM_NUM_WATCHES:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return;
		}
		*iv = diana->systems[object].watch.population;
		break;
	case DL_SYSTEM_WATCHES:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return;
		}
		FOREACH_SPARSEINTSET(i, j, &diana->systems[object].watch) {
			*iv++ = i;
		}
		break;
	case DL_SYSTEM_NUM_ENTITIES:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return;
		}
		*iv = diana->systems[object].entities.population;
		break;
	case DL_SYSTEM_ENTITIES:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return;
		}
		FOREACH_SPARSEINTSET(i, j, &diana->systems[object].entities) {
			*iv++ = i;
		}
		break;
#ifdef DIANA_MANAGER_ENABLED
	case DL_MANAGER_NUM_OBSERVE:
#endif
	default:
		diana->error = DL_ERROR_INVALID_VALUE;
		return;
	}
}

void const * diana_getObjectP(struct diana *diana, DLuint object, DLenum property) {
	switch(property) {
#ifdef DIANA_MANAGER_ENABLED
	case DL_ENTITY_ADDED:
	case DL_ENTITY_ENABLED:
	case DL_ENTITY_DISABLED:
	case DL_ENTITY_DELETED:
#endif
	case DL_SUBSCRIBED:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return NULL;
		}
		return diana->systems[object].subscribed;
	case DL_UNSUBSCRIBED:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return NULL;
		}
		return diana->systems[object].unsubscribed;
	case DL_PROCESSING_STARTED:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return NULL;
		}
		return diana->systems[object].started;
	case DL_PROCESSING_ENDED:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return NULL;
		}
		return diana->systems[object].ended;
	case DL_COMPONENT_NAME:
		if(object >= diana->num_components) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return NULL;
		}
		return diana->components[object].name;
	case DL_SYSTEM_NAME:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return NULL;
		}
		return diana->systems[object].name;
	case DL_SYSTEM_PROCESS:
		if(object >= diana->num_systems) {
			diana->error = DL_ERROR_INVALID_VALUE;
			return NULL;
		}
		return diana->systems[object].process;
#ifdef DIANA_MANAGER_ENABLED
	case DL_MANAGER_NAME:
#endif
	default:
		diana->error = DL_ERROR_INVALID_VALUE;
		return NULL;
	}
}

void diana_getObjectPV(struct diana *diana, DLuint object, DLenum property, void const * * pv) {
	*pv = diana_getObjectP(diana, object, property);
}
