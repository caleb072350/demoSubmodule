#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "list.h"
#include "thrdpool.h"

struct __thrdpool {
    struct list_head task_queue;
    size_t nthreads;
    size_t stacksize;
    pthread_t tid;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_key_t key;
    pthread_cond_t *terminate;
};

struct __thrdpool_task_entry
{
    struct list_head list;
    struct thrdpool_task task;
};

static pthread_t __zero_tid;

static void *__thrdpool_routine(void *arg) 
{
    thrdpool_t *pool = (thrdpool_t *)arg;
    struct list_head **pos = &pool->task_queue.next;
    struct __thrdpool_task_entry *entry;
    pthread_t tid;

    pthread_setspecific(pool->key, pool);
    while (1) {
        pthread_mutex_lock(&pool->mutex);
        while (!pool->terminate && list_empty(&pool->task_queue))
            pthread_cond_wait(&pool->cond, &pool->mutex);
        if (pool->terminate) break;

        entry = list_entry(*pos, struct __thrdpool_task_entry, list);
        list_del(*pos);
        pthread_mutex_unlock(&pool->mutex);

        entry->task.routine(entry->task.context);
        free(entry);
    }

    tid = pool->tid;
    pool->tid = pthread_self();
    if (--pool->nthreads == 0) {
        pthread_cond_signal(pool->terminate);
    }

    pthread_mutex_unlock(&pool->mutex);
    if (memcmp(&tid, &__zero_tid, sizeof(pthread_t)) != 0)
        pthread_join(tid, NULL);
    return NULL;
}

static int __thrdpool_init_locks(thrdpool_t *pool) {
    int ret;
    ret = pthread_mutex_init(&pool->mutex, NULL);
    if (ret == 0) {
        ret = pthread_cond_init(&pool->cond, NULL);
        if (ret == 0) return 0;
        pthread_mutex_destroy(&pool->mutex);
    }
    errno = ret;
    return -1;
}

static void __thrdpool_destroy_locks(thrdpool_t *pool) {
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
}

static void __thrdpool_terminate(thrdpool_t *pool) {
    pthread_cond_t term = PTHREAD_COND_INITIALIZER;
    pthread_mutex_lock(&pool->mutex);

    pool->terminate = &term;
    pthread_cond_broadcast(&pool->cond);
    while (pool->nthreads > 0) 
        pthread_cond_wait(&term, &pool->mutex);
    
    pthread_mutex_unlock(&pool->mutex);

    if (memcmp(&pool->tid, &__zero_tid, sizeof(pthread_t)) != 0)
        pthread_join(pool->tid, NULL);
}

static int __thrdpool_create_threads(size_t nthreads, thrdpool_t *pool) {
    pthread_attr_t attr;
    pthread_t tid;
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret == 0) {
        if (pool->stacksize)
            pthread_attr_setstacksize(&attr, pool->stacksize);
        while (pool->nthreads < nthreads) {
            ret = pthread_create(&tid, &attr, __thrdpool_routine, pool);
            if (ret == 0) 
                pool->nthreads++;
            else 
                break;
        }
        pthread_attr_destroy(&attr);
        if (pool->nthreads == nthreads) return 0;

        __thrdpool_terminate(pool);
    }

    errno = ret;
    return -1;
}

thrdpool_t *thrdpool_create(size_t nthreads, size_t stacksize)
{
    thrdpool_t *pool;
    int ret;

    pool = (struct __thrdpool*)malloc(sizeof(struct __thrdpool));
    if (pool) {
        if (__thrdpool_init_locks(pool) >= 0) {
            ret = pthread_key_create(&pool->key, NULL);
            if (ret == 0) {
                INIT_LIST_HEAD(&pool->task_queue);
                pool->stacksize = stacksize;
                pool->nthreads = 0;
                memset(&pool->tid, 0, sizeof(pthread_t));
                pool->terminate = NULL;
                if (__thrdpool_create_threads(nthreads, pool) >= 0)
                    return pool;
                pthread_key_delete(pool->key);
            } else
                errno = ret;
            
            __thrdpool_destroy_locks(pool);
        }
        free(pool);
    }
    return NULL;
}

void __thrdpool_schedule(const struct thrdpool_task *task, void *buf, thrdpool_t *pool)
{
    struct __thrdpool_task_entry *entry = (struct __thrdpool_task_entry*)buf;
    entry->task = *task;
    pthread_mutex_lock(&pool->mutex);
    list_add_tail(&entry->list, &pool->task_queue);
    pthread_mutex_unlock(&pool->mutex);
    pthread_cond_signal(&pool->cond);
}

int thrdpool_schedule(const struct thrdpool_task *task, thrdpool_t *pool) {
    void *buf = malloc(sizeof(struct __thrdpool_task_entry));
    if (buf) {
        __thrdpool_schedule(task, buf, pool);
        return 0;
    }
    return -1;
}

int thrdpool_increase(thrdpool_t *pool)
{
    pthread_attr_t attr;
    pthread_t tid;
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret == 0) {
        if (pool->stacksize)
            pthread_attr_setstacksize(&attr, pool->stacksize);
        pthread_mutex_lock(&pool->mutex);
        ret = pthread_create(&tid, &attr, __thrdpool_routine, pool);
        if (ret == 0)
            pool->nthreads++;
        pthread_mutex_unlock(&pool->mutex);
        pthread_attr_destroy(&attr);
        if (ret == 0) return 0;
    }
    errno = ret;
    return -1;
}

int thrdpool_in_pool(thrdpool_t *pool)
{
    return pthread_getspecific(pool->key) == (void*)pool;
}

void thrdpool_destroy(void (*pending)(const struct thrdpool_task *), thrdpool_t *pool)
{
    struct __thrdpool_task_entry *entry;
    struct list_head *pos, *tmp;

    __thrdpool_terminate(pool);
    list_for_each_safe(pos, tmp, &pool->task_queue)
    {
        entry = list_entry(pos, struct __thrdpool_task_entry, list);
        list_del(pos);
        if (pending)
            pending(&entry->task);
        free(entry);
    }
    pthread_key_delete(pool->key);
    __thrdpool_destroy_locks(pool);
    free(pool);
}