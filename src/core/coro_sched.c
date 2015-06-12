/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pub.h"
#include "list.h"
#include "util.h"
#include "rbtree.h"
#include "memcache.h"
#include "coro_switch.h"
#include "netevent.h"

#include "coro_sched.h"

struct timer_node
{
    struct rb_node node;        //用于连接inactive
    struct rb_root root;        //相同超时时间的协程的根
    long long timeout;          //超时时间(毫秒)
};

struct coroutine
{
    struct list_head list;      //挂在coro_schedule中的free_pool或者active链表上
    struct rb_node node;        //协程休眠时, 挂在timer_node的root上, 提高网络事件来临时的查找效率
    int coro_id;                //协程号
    struct context ctx;         //本协程栈顶指针描述
    struct coro_stack stack;    //协程栈描述
    coro_func func;             //具体执行函数
    void *args;                 //执行函数参数

    long long timeout;          //未来某个时间(毫秒), 用于跟踪网络事件超时
    int active_by_timeout;      //超时唤醒的(1), 事件到来唤醒的(非1)
};

/* sched policy, if timeout_milliseconds == -1 sched policy can wait forever */
typedef void (*sched_policy)(int timeout_milliseconds);

struct coro_schedule
{
    size_t min_coro_size;       //协程最少个数
    size_t max_coro_size;       //协程最大个数
    size_t curr_coro_size;      //当前协程数

    int next_coro_id;           //下一个协程号

    size_t stack_bytes;         //协程的栈大小, 字节单位
    struct coroutine main_coro; //主协程
    struct coroutine *current;  //当前协程

    struct list_head idle;      //空闲的协程列表
    struct list_head active;    //活跃的协程列表
    struct rb_root inactive;    //处于等待中的协程
    struct memcache *cache;     //timer_node的cache

    sched_policy policy;        //调度策略
};

static struct coro_schedule g_sched;

static struct coroutine *find_in_timer(struct timer_node *tm_node, int coro_id)
{
    struct rb_node *each = tm_node->root.rb_node;

    while (each)
    {
        struct coroutine *coro = container_of(each, struct coroutine, node);

        int result = coro_id - coro->coro_id;
        if (result < 0)
            each = each->rb_left;
        else if (result > 0)
            each = each->rb_right;
        else
            return coro;
    }

    return NULL;
}

static inline void remove_from_timer_node(struct timer_node *tm_node,
                                                  struct coroutine *coro)
{
    if (NULL == find_in_timer(tm_node, coro->coro_id))
        return;

    rb_erase(&coro->node, &tm_node->root);
}

static struct timer_node *find_timer_node(long long timeout)
{
    struct rb_node *each = g_sched.inactive.rb_node;

    while (each)
    {
        struct timer_node *tm_node = container_of(each, struct timer_node, node);
        long long timespan = timeout - tm_node->timeout;
        if (timespan < 0)
            each = each->rb_left;
        else if (timespan > 0)
            each = each->rb_right;
        else
            return tm_node;
    }

    return NULL;
}

static inline void remove_from_inactive_tree(struct coroutine *coro)
{
    struct timer_node *tm_node = find_timer_node(coro->timeout);
    if (NULL == tm_node)
        return;

    remove_from_timer_node(tm_node, coro);

    if (RB_EMPTY_ROOT(&tm_node->root))
    {
        rb_erase(&tm_node->node, &g_sched.inactive);
        memcache_free(g_sched.cache, tm_node);
    }
}

static void add_to_timer_node(struct timer_node *tm_node, struct coroutine *coro)
{
    struct rb_node **newer = &tm_node->root.rb_node, *parent = NULL;

    while (*newer)
    {
        struct coroutine *each = container_of(*newer, struct coroutine, node);
        int result = coro->coro_id - each->coro_id;
        parent = *newer;

        if (result < 0)
            newer = &(*newer)->rb_left;
        else
            newer = &(*newer)->rb_right;
    }

    rb_link_node(&coro->node, parent, newer);
    rb_insert_color(&coro->node, &tm_node->root);
}

static void move_to_inactive_tree(struct coroutine *coro)
{
    int timespan;
    struct timer_node *tm_node;
    struct rb_node **newer = &g_sched.inactive.rb_node;
    struct rb_node *parent = NULL;

    while (*newer)
    {
        tm_node = container_of(*newer, struct timer_node, node);
        timespan = coro->timeout - tm_node->timeout;

        parent = *newer;
        if (timespan < 0)
            newer = &(*newer)->rb_left;
        else if (timespan > 0)
            newer = &(*newer)->rb_right;
        else
        {
            add_to_timer_node(tm_node, coro);
            return;
        }
    }

    struct timer_node *tmp = memcache_alloc(g_sched.cache);
    if (unlikely(NULL == tmp))  //still in active list
        return;

    tmp->timeout = coro->timeout;
    add_to_timer_node(tmp, coro);
    rb_link_node(&tmp->node, parent, newer);
    rb_insert_color(&tmp->node, &g_sched.inactive);
}

static inline void move_to_idle_list_direct(struct coroutine *coro)
{
    list_add_tail(&coro->list, &g_sched.idle);
}

static inline void move_to_active_list_tail_direct(struct coroutine *coro)
{
    list_add_tail(&coro->list, &g_sched.active);
}

static inline void move_to_active_list_tail(struct coroutine *coro)
{
    remove_from_inactive_tree(coro);
    list_add_tail(&coro->list, &g_sched.active);
}

static inline void move_to_active_list_head(struct coroutine *coro)
{
    remove_from_inactive_tree(coro);
    list_add(&coro->list, &g_sched.active);
}

static inline void coroutine_init(struct coroutine *coro)
{
    INIT_LIST_HEAD(&coro->list);
    RB_CLEAR_NODE(&coro->node);
    coro->timeout = 0;
    coro->active_by_timeout = -1;
}

static struct coroutine *create_coroutine()
{
    struct coroutine *coro;

    if (unlikely(g_sched.curr_coro_size == g_sched.max_coro_size))
        return NULL;

    coro = (struct coroutine *)malloc(sizeof(struct coroutine));
    if (unlikely(NULL == coro))
        return NULL;

    if (coro_stack_alloc(&coro->stack, g_sched.stack_bytes))
    {
        free(coro);
        return NULL;
    }

    coro->coro_id = ++g_sched.next_coro_id;
    g_sched.curr_coro_size++;

    return coro;
}

#if 0
static void free_coroutine(struct coroutine *coro)
{
    list_del(&coro->list);
    remove_from_inactive_tree(coro);
    coro_stack_free(&coro->stack);
    free(coro);

    g_sched.curr_coro_size--;
}
#endif

static struct coroutine *get_coroutine()
{
    struct coroutine *coro;

    if (!list_empty(&g_sched.idle))
    {
        coro = list_first_entry(&g_sched.idle, struct coroutine, list);
        list_del(&coro->list);
        coroutine_init(coro);

        return coro;
    }

    coro = create_coroutine();
    if (NULL != coro)
        coroutine_init(coro);

    return coro;
}

static void coroutine_switch(struct coroutine *curr, struct coroutine *next)
{
    g_sched.current = next;
    context_switch(&curr->ctx, &next->ctx);
}

static inline struct coroutine *get_active_coroutine()
{
    struct coroutine *coro;

    if (list_empty(&g_sched.active))
        return NULL;

    coro = list_entry(g_sched.active.next, struct coroutine, list);
    list_del_init(&coro->list);

    return coro;
}

static inline void run_active_coroutine()
{
    struct coroutine *coro;

    while (NULL != (coro = get_active_coroutine()))
        coroutine_switch(&g_sched.main_coro, coro);
}

static inline struct timer_node *get_recent_timer_node()
{
    struct rb_node *recent = rb_first(&g_sched.inactive);
    if (NULL == recent)
        return NULL;

    return container_of(recent, struct timer_node, node);
}

static inline void timeout_coroutine_handler(struct timer_node *node)
{
    struct rb_node *recent;

    while (NULL != (recent = node->root.rb_node))
    {
        struct coroutine *coro = container_of(recent, struct coroutine, node);
        rb_erase(recent, &node->root);
        coro->active_by_timeout = 1;
        move_to_active_list_tail_direct(coro);
    }
}

static void check_timeout_coroutine()
{
    struct timer_node *node;
    long long now = get_curr_mseconds();

    while (NULL != (node = get_recent_timer_node()))
    {
        if (now < node->timeout)
            return;

        rb_erase(&node->node, &g_sched.inactive);
        timeout_coroutine_handler(node);
        memcache_free(g_sched.cache, node);
    }
}

static inline int get_recent_timespan()
{
    int timespan;
    struct timer_node *node = get_recent_timer_node();
    if (NULL == node)
        return 10 * 1000;  //at least 10 seconds

    timespan = node->timeout - get_curr_mseconds();

    return (timespan < 0) ? 0 : timespan;
}

void schedule_cycle()
{
    int timeout_milliseconds;

    for (;;)
    {
        check_timeout_coroutine();
        run_active_coroutine();

        timeout_milliseconds = get_recent_timespan();
        g_sched.policy(timeout_milliseconds);
        run_active_coroutine();
    }
}

static __attribute__((__regparm__(1))) void coro_routine_proxy(void *args)
{
    struct coroutine *coro = args;

    coro->func(coro->args);
    move_to_idle_list_direct(coro);
    coroutine_switch(g_sched.current, &g_sched.main_coro);
}

int dispatch_coro(coro_func func, void *args)
{
    struct coroutine *coro = get_coroutine();
    if (unlikely(NULL == coro))
        return -1;

    coro->func = func;
    coro->args = args;

    coro_stack_init(&coro->ctx, &coro->stack, coro_routine_proxy, coro);
    move_to_active_list_tail_direct(coro);

    return 0;
}

void yield()
{
    move_to_active_list_tail_direct(g_sched.current);
    coroutine_switch(g_sched.current, &g_sched.main_coro);
}

void schedule_timeout(int milliseconds)
{
    struct coroutine *coro = g_sched.current;

    coro->timeout = get_curr_mseconds() + milliseconds;
    move_to_inactive_tree(coro);
    coroutine_switch(g_sched.current, &g_sched.main_coro);
}

int is_wakeup_by_timeout()
{
    int result = g_sched.current->active_by_timeout;
    g_sched.current->active_by_timeout = -1;

    return result == 1;
}

void wakeup_coro(void *args)
{
    struct coroutine *coro = args;

    coro->active_by_timeout = -1;
    move_to_active_list_tail(coro);
}

void wakeup_coro_priority(void *args)
{
    struct coroutine *coro = args;

    coro->active_by_timeout = -1;
    move_to_active_list_head(coro);
}

void *current_coro()
{
    return g_sched.current;
}

/*
    @stack_bytes:   协程栈大小, KB单位, 且PAGE_SIZE对齐.
    @max_coro_size: 协程最大个数, 推荐为网络服务的最大连接数
*/
void schedule_init(size_t stack_kbytes, size_t max_coro_size)
{
    assert(max_coro_size >= 2);

    g_sched.max_coro_size = max_coro_size;
    g_sched.curr_coro_size = 0;
    g_sched.next_coro_id = 0;

    g_sched.stack_bytes = stack_kbytes * 1024;
    g_sched.current = NULL;

    INIT_LIST_HEAD(&g_sched.idle);
    INIT_LIST_HEAD(&g_sched.active);
    g_sched.inactive = RB_ROOT;
    g_sched.cache = memcache_create(sizeof(struct timer_node), max_coro_size / 2);
    g_sched.policy = event_cycle;
    if (NULL == g_sched.cache)
    {
        printf("Failed to create mem cache for schedule timer node\n");
        exit(0);
    }
}

