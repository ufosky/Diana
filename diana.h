// vim: ts=2:sw=2:noexpandtab

#ifndef __DIANA_H__
#define __DIANA_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DIANA_VERSION "0.0.2.0"

#include <stddef.h>

typedef int DLint;
typedef unsigned int DLuint;
typedef unsigned int DLenum;
typedef unsigned char DLubyte;
typedef float DLfloat;
typedef char DLboolean;

// boolean
#define DL_FALSE 0
#define DL_TRUE  1

// errors
#define DL_ERROR_NONE              0x0000
#define DL_ERROR_OUT_OF_MEMORY     0x0001
#define DL_ERROR_INVALID_VALUE     0x0002
#define DL_ERROR_INVALID_OPERATION 0x0003

// events
#define DL_ENTITY_ADDED       0x1000
#define DL_ENTITY_ENABLED     0x1001
#define DL_ENTITY_DISABLED    0x1002
#define DL_ENTITY_DELETED     0x1003
#define DL_SUBSCRIBED         0x1004
#define DL_UNSUBSCRIBED       0x1005
#define DL_PROCESSING_STARTED 0x1006
#define DL_PROCESSING_ENDED   0x1007

// inspection
#define DL_NUM_COMPONENTS      0x2000
#define DL_COMPONENT_NAME      0x2001
#define DL_COMPONENT_SIZE      0x2002
#define DL_NUM_SYSTEMS         0x2003
#define DL_SYSTEM_NAME         0x2004
#define DL_SYSTEM_PROCESS      0x2005
#define DL_SYSTEM_NUM_WATCHES  0x2006
#define DL_SYSTEM_WATCHES      0x2007
#define DL_SYSTEM_NUM_ENTITIES 0x2008
#define DL_SYSTEM_ENTITIES     0x2009
#define DL_NUM_MANAGERS        0x2010
#define DL_MANAGER_NAME        0x2011
#define DL_MANAGER_NUM_OBSERVE 0x2012
#define DL_MANAGER_USER_DATA   0x2013
#define DL_SYSTEM_USER_DATA    0x2014

struct diana;

struct diana *allocate_diana(void *(*malloc)(size_t), void (*free)(void *));

DLenum diana_getError(struct diana *);

void diana_initialize(struct diana *);

void diana_process(struct diana *, DLfloat delta);

void diana_free(struct diana *);

// COMPONENT
DLuint diana_registerComponent(struct diana *diana, const char *name, size_t size);

// SYSTEM
DLuint diana_registerSystem(struct diana *diana, const char *name, void (*process)(struct diana *, void *user_data, DLuint entity, DLfloat delta), void *user_data);

void diana_setSystemProcessCallback(struct diana *diana, DLuint system, void (*process)(struct diana *, void *user_data, DLuint entity, DLfloat delta));

void diana_setSystemProcessCallbacks(struct diana *diana, DLuint system, void (*started)(struct diana *, void *user_data), void (*process)(struct diana *, void *user_data, DLuint entity, DLfloat delta), void (*ended)(struct diana *, void *user_data));

void diana_setSystemEventCallback(struct diana *diana, DLuint system, DLenum event, void (*callback)(struct diana *, void *user_data, DLuint entity));

void diana_setSystemUserData(struct diana *diana, DLuint system, void *user_data);

void diana_watch(struct diana *diana, DLuint system, DLuint component);

void diana_exclude(struct diana *diana, DLuint system, DLuint component);

// MANAGER
DLuint diana_registerManager(struct diana *diana, const char *name, void *user_data);

void diana_observe(struct diana *diana, DLuint manager, DLenum callback, void (*function)(struct diana *diana, void *user_data, DLenum callback, DLuint entity));

void diana_observeAll(struct diana *diana, DLuint manager, void (*function)(struct diana *diana, void *user_data, DLenum callback, DLuint entity));

void diana_setManagerUserData(struct diana *diana, DLuint system, void *user_data);

// ENTITY
DLuint diana_spawn(struct diana *diana);

void diana_setComponent(struct diana *diana, DLuint entity, DLuint component, void const * data);

void *diana_getComponent(struct diana *diana, DLuint entity, DLuint component);

void diana_removeComponent(struct diana *diana, DLuint entity, DLuint component);

void diana_add(struct diana *diana, DLuint entity);

void diana_enable(struct diana *diana, DLuint entity);

void diana_disable(struct diana *diana, DLuint entity);

void diana_delete(struct diana *diana, DLuint entity);

// INSPECT
DLint diana_getI(struct diana *, DLenum property);
void diana_getIV(struct diana *, DLenum property, DLint *);

void const * diana_getP(struct diana *, DLenum property);
void diana_getPV(struct diana *, DLenum property, void const * *);

DLint diana_getObjectI(struct diana *, DLuint object, DLenum property);
void diana_getObjectIV(struct diana *, DLuint object, DLenum property, DLint *);

void const * diana_getObjectP(struct diana *, DLuint object, DLenum property);
void diana_getObjectPV(struct diana *, DLuint object, DLenum property, void const * *);

#ifdef __cplusplus
}
#endif

#endif
