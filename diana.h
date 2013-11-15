// vim: ts=2:sw=2:noexpandtab

#ifndef __DIANA_H__
#define __DIANA_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DIANA_VERSION "0.0.2.0"

#include <stddef.h>

// errors
enum {
	DL_ERROR_NONE,
	DL_ERROR_OUT_OF_MEMORY,
	DL_ERROR_INVALID_VALUE,
	DL_ERROR_INVALID_OPERATION,
	DL_ERROR_FULL_COMPONENT
};

// component flags
#define DL_COMPONENT_INDEXED_BIT  1
#define DL_COMPONENT_MULTIPLE_BIT 2
#define DL_COMPONENT_LIMITED_BIT  4

#define DL_COMPONENT_FLAG_INLINE     0
#define DL_COMPONENT_FLAG_INDEXED    DL_COMPONENT_INDEXED_BIT
#define DL_COMPONENT_FLAG_MULTIPLE   (DL_COMPONENT_INDEXED_BIT | DL_COMPONENT_MULTIPLE_BIT)
#define DL_COMPONENT_FLAG_LIMITED(X) (DL_COMPONENT_INDEXED_BIT | DL_COMPONENT_FLAG_LIMITED_BIT | ((X) << 3))

// entity signal
enum {
	DL_ENTITY_ADDED,
	DL_ENTITY_ENABLED,
	DL_ENTITY_DISABLED,
	DL_ENTITY_DELETED
};

// ============================================================================
// DIANA
struct diana;

struct diana *allocate_diana(void *(*malloc)(size_t), void (*free)(void *));

unsigned int diana_getError(struct diana *);

void diana_free(struct diana *);

// ============================================================================
// INITIALIZATION TIME
void diana_initialize(struct diana *);

// ============================================================================
// component
unsigned int diana_createComponent(
	struct diana *diana,
	const char *name,
 	size_t size,
 	unsigned int flags
);

// ============================================================================
// system
unsigned int diana_createSystem(
	struct diana *diana,
	const char *name,
	void (*starting)(struct diana *, void *),
	void (*process)(struct diana *, void *, unsigned int, float),
	void (*ending)(struct diana *, void *),
	void (*subscribed)(struct diana *, void *, unsigned int),
	void (*unsubscribed)(struct diana *, void *, unsigned int),
	void *userData
);

void diana_watch(struct diana *diana, unsigned int system, unsigned int component);

void diana_exclude(struct diana *diana, unsigned int system, unsigned int component);

// ============================================================================
// manager
unsigned int diana_createManager(
	struct diana *diana,
	const char *name,
	void (*added)(struct diana *, void *, unsigned int),
	void (*enabled)(struct diana *, void *, unsigned int),
	void (*disabled)(struct diana *, void *, unsigned int),
	void (*deleted)(struct diana *, void *, unsigned int),
	void *userData
);

// ============================================================================
// RUNTIME
void diana_process(struct diana *, float delta);

// ============================================================================
// entity
unsigned int diana_spawn(struct diana *diana);

unsigned int diana_clone(struct diana *diana, unsigned int entity);

void diana_signal(struct diana *diana, unsigned int entity, unsigned int signal);

// single
void diana_setComponent(struct diana *diana, unsigned int entity, unsigned int component, const void * data);

void * diana_getComponent(struct diana *diana, unsigned int entity, unsigned int component);

void diana_removeComponent(struct diana *diana, unsigned int entity, unsigned int component);

// multiple
unsigned int diana_getComponentCount(struct diana *diana, unsigned int entity, unsigned int component);

void diana_appendComponent(struct diana *diana, unsigned int entity, unsigned int component, const void * data);

void diana_removeComponents(struct diana *diana, unsigned int entity, unsigned int component);

// low level
void diana_setComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i, const void * data);

void * diana_getComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i);

void diana_removeComponentI(struct diana *diana, unsigned int entity, unsigned int component, unsigned int i);

#ifdef __cplusplus
}
#endif

#endif
