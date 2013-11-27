#include "../diana.c"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

size_t allocations = 0, num_allocated = 0;
size_t frees = 0, num_freed = 0;

void *fuzz_malloc(size_t size) {
    size_t *ptr;
    allocations++;
    num_allocated += size;
    ptr = malloc(sizeof(size_t) + size);
    *ptr = size;
    return (void *)(ptr + 1);
}

void fuzz_free(void *ptr) {
    size_t *size = ((size_t *)ptr) - 1;
    frees++;
    num_freed += *size;
    free(size);
}

unsigned int num_spawns = 0;
unsigned int num_deletes = 0;
unsigned int num_active = 0;
unsigned int max_eid_spawned = 0;

unsigned int n_clones = 0, n_deletes = 0, n_disables = 0, n_spawns = 0;

void print_stats(void) {
    printf("===============================================================================\n");
    printf("Allocations: %zu %zu\n", allocations, num_allocated);
    printf("Frees: %zu %zu\n", frees, num_freed);
    printf("Total Memory: %.2lfkb %.2lfmb\n", (double)(num_allocated - num_freed) / 1024.0, (double)(num_allocated - num_freed) / 1024.0 / 1024.0);
    printf("Spawns %u, Deletes %u, Active %u, Max EID %u\n", num_spawns, num_deletes, num_active, max_eid_spawned);
    printf("In Process: clones %u, deletes %u, disabled %u, spawns %u\n", n_clones, n_deletes, n_disables, n_spawns);
}

unsigned int component_normal;
unsigned int component_indexed;
unsigned int component_multiple;
unsigned int component_indexed_limited;
unsigned int component_multiple_limited;
unsigned int components[5];

struct _sparseIntegerSet disabled_eids;

struct diana *global_diana;

#define DIANA(F, ...) do { int __err = diana_ ## F (global_diana, ## __VA_ARGS__); if(__err != DL_ERROR_NONE) { printf("%s:%i ERROR %i\n", __FILE__, __LINE__, __err); print_stats(); exit(0); } } while(0)

#define R(MIN, MAX) ((rand() % (MAX - MIN)) + MIN)

void add_random_component(unsigned int eid) {
    DIANA(appendComponent, eid, R(0, 5), NULL);
}

unsigned int spawn(void) {
    unsigned int eid;
    unsigned int actions = R(1, 6), action;
    DIANA(spawn, &eid);
    num_spawns++;
    if(eid > max_eid_spawned) {
        max_eid_spawned = eid;
    }
    for(action = 0; actions < actions; action++) {
        add_random_component(eid);
    }
    return eid;
}

unsigned int _clone(unsigned int eid) {
    unsigned int newEid;
    num_spawns++;
    DIANA(clone, eid, &newEid);
    return newEid;
}

void add(unsigned int eid) {
    DIANA(signal, eid, DL_ENTITY_ADDED);
}

void enable(unsigned int eid) {
    DIANA(signal, eid, DL_ENTITY_ENABLED);
}

void disable(unsigned int eid) {
    _sparseIntegerSet_insert(global_diana, &disabled_eids, eid);
    DIANA(signal, eid, DL_ENTITY_DELETED);
}

void delete(unsigned int eid) {
    DIANA(signal, eid, DL_ENTITY_DELETED);
    num_deletes++;
}

void random_subscribed(struct diana *diana, void *ud, unsigned int eid) {
    (void)diana;
    (void)ud;
    (void)eid;
    num_active++;
}

void random_unsubscribed(struct diana *diana, void *ud, unsigned int eid) {
    (void)diana;
    (void)ud;
    (void)eid;
    num_active--;
}

void random_process(struct diana *diana, void *ud, unsigned int eid, float delta) {
    unsigned int actions = R(0, 2), action;

    (void)diana;
    (void)ud;
    (void)delta;

    for(action = 0; action < actions; action++) {
        switch(R(0, 4)) {
        case 0:
            add(_clone(eid));
            n_clones++;
            break;
        case 1:
            break;
        case 2:
            disable(eid);
            n_disables++;
            break;
        case 3:
            add(spawn());
            n_spawns++;
            break;
        }
    }

    if(rand() % 2 == 0 && num_active > 128) {
        delete(eid);
        n_deletes++;
    }
}

struct timespec diff(struct timespec start, struct timespec end) {
    struct timespec temp;
    if((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

void print_timespec(struct timespec ts) {
    long int min = ts.tv_sec / 60;
    long int sec = ts.tv_sec % 60;
    printf("%02li:%02li.%09ld", min, sec, ts.tv_nsec);
}

struct thread_data {
    pthread_t thread;
    unsigned int system;
    unsigned int iterations;
    unsigned int alive;
};

void *thread_proc(void *td) {
    struct thread_data *data = (struct thread_data *)td;

    struct timespec rqtp;

    rqtp.tv_sec = 0;
    rqtp.tv_nsec = 10000;

    data->alive = 1;

    while(data->alive) {
        DIANA(processSystem, data->system, 1.0);
        data->iterations++;
    //    nanosleep(&rqtp, NULL);
    }

    return NULL;
}

static void *create_mutex(void) {
    pthread_mutex_t *m = malloc(sizeof(*m));
    pthread_mutex_init(m, NULL);
    return m;
}

static void mutex_lock(void *_m) {
    pthread_mutex_t *m = (pthread_mutex_t *)_m;
    pthread_mutex_lock(m);
}

static void mutex_unlock(void *_m) {
    pthread_mutex_t *m = (pthread_mutex_t *)_m;
    pthread_mutex_unlock(m);
}

static void mutex_free(void *_m) {
    pthread_mutex_t *m = (pthread_mutex_t *)_m;
    pthread_mutex_destroy(m);
    free(m);
}

int main(int argc, char *argv[]) {
    unsigned int eid, stati = 0, i;
    struct timespec time1, time2, time3, setup_time, iteration_time;

    struct thread_data t1, t2, t3;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);

    allocate_diana(fuzz_malloc, fuzz_free, &global_diana);

    DIANA(createComponent, "Normal", 8, DL_COMPONENT_FLAG_INLINE, &components[4]);
    DIANA(createComponent, "Indexed", 16, DL_COMPONENT_FLAG_INDEXED, &components[4]);
    DIANA(createComponent, "Multiple", 8, DL_COMPONENT_FLAG_MULTIPLE, &components[4]);
    DIANA(createComponent, "Indexed Limited", 256, DL_COMPONENT_FLAG_INDEXED | DL_COMPONENT_FLAG_LIMITED(128), &components[4]);
    DIANA(createComponent, "Multiple Limited", 256, DL_COMPONENT_FLAG_MULTIPLE | DL_COMPONENT_FLAG_LIMITED(128), &components[4]);

    DIANA(createSystem, "Random", NULL, random_process, NULL, random_subscribed, random_unsubscribed, NULL, DL_SYSTEM_FLAG_PASSIVE, &t1.system);
    DIANA(createSystem, "Random", NULL, random_process, NULL, random_subscribed, random_unsubscribed, NULL, DL_SYSTEM_FLAG_PASSIVE, &t2.system);
    DIANA(createSystem, "Random", NULL, random_process, NULL, random_subscribed, random_unsubscribed, NULL, DL_SYSTEM_FLAG_PASSIVE, &t3.system);

    DIANA(mutexFunctions, create_mutex, mutex_lock, mutex_unlock, mutex_free);

    DIANA(initialize);

    for(i = 0; i < 128; i++) {
        add(spawn());
    }

    pthread_create(&t1.thread, NULL, thread_proc, &t1);
    pthread_create(&t2.thread, NULL, thread_proc, &t2);
    pthread_create(&t3.thread, NULL, thread_proc, &t3);

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);

    while(stati < 3000000) {
        if(!_sparseIntegerSet_isEmpty(global_diana, &disabled_eids)) {
            eid = _sparseIntegerSet_pop(global_diana, &disabled_eids);
            add_random_component(eid);
            DIANA(removeComponents, eid, R(0, 5));
            enable(eid);
        }

        stati = t1.iterations + t2.iterations + t3.iterations;

        DIANA(process, 0);
    }

    t1.alive = 0;
    t2.alive = 0;
    t3.alive = 0;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time3);

    diana_free(global_diana);

    setup_time = diff(time1, time2);
    iteration_time = diff(time2, time3);

    print_stats();

    printf("setup completed in ");
    print_timespec(setup_time);
    printf("\n");

    printf("%i iterations completed in ", stati);
    print_timespec(iteration_time);
    printf("\n");

    return 0;
}
