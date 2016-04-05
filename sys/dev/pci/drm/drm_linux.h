/*	$OpenBSD: drm_linux.h,v 1.45 2016/02/05 15:51:10 kettenis Exp $	*/
/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/atomic.h>
#include <sys/task.h>

typedef int irqreturn_t;
#define IRQ_NONE	0
#define IRQ_HANDLED	1

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

typedef int32_t s32;
typedef int64_t s64;

typedef uint64_t __u64;

typedef uint16_t __le16;
typedef uint16_t __be16;
typedef uint32_t __le32;
typedef uint32_t __be32;

typedef bus_addr_t dma_addr_t;
typedef bus_addr_t phys_addr_t;

typedef off_t loff_t;

#define __force
#define __always_unused	__unused
#define __read_mostly
#define __iomem
#define __must_check
#define __init

#define barrier()		__asm __volatile("" : : : "memory");

#define uninitialized_var(x) x

#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN
#else
#define __LITTLE_ENDIAN
#endif

#define le16_to_cpu(x) letoh16(x)
#define le32_to_cpu(x) letoh32(x)
#define cpu_to_le16(x) htole16(x)
#define cpu_to_le32(x) htole32(x)

#define be32_to_cpup(x) betoh32(*x)

#define lower_32_bits(n)	((u32)(n))
#define upper_32_bits(_val)	((u32)(((_val) >> 16) >> 16))
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : (1ULL<<(n)) -1)
#define BIT(x)			(1 << x)
#define BITS_TO_LONGS(x)	howmany((x), 8 * sizeof(long))

#define ACCESS_ONCE(x)		(x)

#define EXPORT_SYMBOL(x)

#define IS_ENABLED(x) x - 0

#define MODULE_FIRMWARE(x)
#define MODULE_PARM_DESC(parm, desc)
#define module_param_named(name, value, type, perm)

#define ARRAY_SIZE nitems

#define ERESTARTSYS	EINTR
#define ETIME		ETIMEDOUT
#define EREMOTEIO	EIO
#define EPROTO		EIO
#define ENOTSUPP	ENOTSUP

#define KERN_INFO
#define KERN_WARNING
#define KERN_NOTICE
#define KERN_DEBUG
#define KERN_CRIT
#define KERN_ERR

#define KBUILD_MODNAME "drm"

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define printk_once(fmt, arg...) ({		\
	static int __warned;			\
	if (!__warned) {			\
		printf(fmt, ## arg);		\
		__warned = 1;			\
	}					\
})

#define printk(fmt, arg...)	printf(fmt, ## arg)
#define pr_warn(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_warn_once(fmt, arg...)	printk_once(pr_fmt(fmt), ## arg)
#define pr_notice(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_crit(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_err(fmt, arg...)	printf(pr_fmt(fmt), ## arg)

#ifdef DRMDEBUG
#define pr_info(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_info_once(fmt, arg...)	printk_once(pr_fmt(fmt), ## arg)
#define pr_debug(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#else
#define pr_info(fmt, arg...)	do { } while(0)
#define pr_info_once(fmt, arg...)	do { } while(0)
#define pr_debug(fmt, arg...)	do { } while(0)
#endif

#define dev_warn(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *WARNING* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)
#define dev_notice(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *NOTICE* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)
#define dev_crit(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)
#define dev_err(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)

#ifdef DRMDEBUG
#define dev_info(dev, fmt, arg...)				\
	printf("drm: " fmt, ## arg)
#define dev_debug(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *DEBUG* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)
#else
#define dev_info(dev, fmt, arg...) 				\
	    do { } while(0)
#define dev_debug(dev, fmt, arg...) 				\
	    do { } while(0)
#endif

#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)

#define BUG()								\
do {									\
	panic("BUG at %s:%d", __FILE__, __LINE__);			\
} while (0)

#define BUG_ON(x) KASSERT(!(x))

#define BUILD_BUG_ON(x) CTASSERT(!(x))
#define BUILD_BUG_ON_NOT_POWER_OF_2(x)

#define WARN(condition, fmt...) ({ 					\
	int __ret = !!(condition);					\
	if (__ret)							\
		printf(fmt);						\
	unlikely(__ret);						\
})

#define WARN_ONCE(condition, fmt...) ({					\
	static int __warned;						\
	int __ret = !!(condition);					\
	if (__ret && !__warned) {					\
		printf(fmt);						\
		__warned = 1;						\
	}								\
	unlikely(__ret);						\
})

#define _WARN_STR(x) #x

#define WARN_ON(condition) ({						\
	int __ret = !!(condition);					\
	if (__ret)							\
		printf("WARNING %s failed at %s:%d\n",			\
		    _WARN_STR(condition), __FILE__, __LINE__);		\
	unlikely(__ret);						\
})

#define WARN_ON_ONCE(condition) ({					\
	static int __warned;						\
	int __ret = !!(condition);					\
	if (__ret && !__warned) {					\
		printf("WARNING %s failed at %s:%d\n",			\
		    _WARN_STR(condition), __FILE__, __LINE__);		\
		__warned = 1;						\
	}								\
	unlikely(__ret);						\
})

#define TP_PROTO(x...) x

#define DEFINE_EVENT(template, name, proto, args) \
static inline void trace_##name(proto) {}

#define TRACE_EVENT(name, proto, args, tstruct, assign, print) \
static inline void trace_##name(proto) {}

#define TRACE_EVENT_CONDITION(name, proto, args, cond, tstruct, assign, print) \
static inline void trace_##name(proto) {}

#define DECLARE_EVENT_CLASS(name, proto, args, tstruct, assign, print) \
static inline void trace_##name(proto) {}

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-ELAST)

static inline void *
ERR_PTR(long error)
{
	return (void *) error;
}

static inline long
PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

static inline long
IS_ERR(const void *ptr)
{
        return IS_ERR_VALUE((unsigned long)ptr);
}

static inline long
IS_ERR_OR_NULL(const void *ptr)
{
        return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}

#define container_of(ptr, type, member) ({                      \
	__typeof( ((type *)0)->member ) *__mptr = (ptr);        \
	(type *)( (char *)__mptr - offsetof(type,member) );})

#ifndef __DECONST
#define __DECONST(type, var)    ((type)(__uintptr_t)(const void *)(var))
#endif

typedef struct rwlock rwlock_t;
typedef struct mutex spinlock_t;
#define DEFINE_SPINLOCK(x)	struct mutex x

static inline void
spin_lock_irqsave(struct mutex *mtxp, __unused unsigned long flags)
{
	mtx_enter(mtxp);
}
static inline void
spin_unlock_irqrestore(struct mutex *mtxp, __unused unsigned long flags)
{
	mtx_leave(mtxp);
}
#define spin_lock(mtxp)			mtx_enter(mtxp)
#define spin_unlock(mtxp)		mtx_leave(mtxp)
#define spin_lock_irq(mtxp)		mtx_enter(mtxp)
#define spin_unlock_irq(mtxp)		mtx_leave(mtxp)
#define assert_spin_locked(mtxp)	MUTEX_ASSERT_LOCKED(mtxp)
#define mutex_lock_interruptible(rwl)	-rw_enter(rwl, RW_WRITE | RW_INTR)
#define mutex_lock(rwl)			rw_enter_write(rwl)
#define mutex_lock_nest_lock(rwl, sub)	rw_enter_write(rwl)
#define mutex_trylock(rwl)		(rw_enter(rwl, RW_WRITE | RW_NOSLEEP) == 0)
#define mutex_unlock(rwl)		rw_exit_write(rwl)
#define mutex_is_locked(rwl)		(rw_status(rwl) == RW_WRITE)
#define down_read(rwl)			rw_enter_read(rwl)
#define up_read(rwl)			rw_exit_read(rwl)
#define down_write(rwl)			rw_enter_write(rwl)
#define up_write(rwl)			rw_exit_write(rwl)
#define read_lock(rwl)			rw_enter_read(rwl)
#define read_unlock(rwl)		rw_exit_read(rwl)
#define write_lock(rwl)			rw_enter_write(rwl)
#define write_unlock(rwl)		rw_exit_write(rwl)

#define local_irq_save(x)		(x) = splhigh()
#define local_irq_restore(x)		splx((x))

struct wait_queue_head {
	struct mutex lock;
	unsigned int count;
};
typedef struct wait_queue_head wait_queue_head_t;

static inline void
init_waitqueue_head(wait_queue_head_t *wq)
{
	mtx_init(&wq->lock, IPL_NONE);
	wq->count = 0;
}

#define wait_event(wq, condition) \
do {						\
	struct sleep_state sls;			\
						\
	if (condition)				\
		break;				\
	atomic_inc_int(&(wq).count);		\
	sleep_setup(&sls, &wq, 0, "drmwe");	\
	sleep_finish(&sls, !(condition));	\
	atomic_dec_int(&(wq).count);		\
} while (!(condition))

#define __wait_event_timeout(wq, condition, ret) \
do {						\
	struct sleep_state sls;			\
	int deadline, __error;			\
						\
	atomic_inc_int(&(wq).count);		\
	sleep_setup(&sls, &wq, 0, "drmwet");	\
	sleep_setup_timeout(&sls, ret);		\
	deadline = ticks + ret;			\
	sleep_finish(&sls, !(condition));	\
	ret = deadline - ticks;			\
	__error = sleep_finish_timeout(&sls);	\
	atomic_dec_int(&(wq).count);		\
	if (ret < 0 || __error == EWOULDBLOCK)	\
		ret = 0;			\
	if (ret == 0 && (condition)) {		\
		ret = 1;			\
		break;				\
	}					\
} while (ret > 0 && !(condition))

#define wait_event_timeout(wq, condition, timo)	\
({						\
	long __ret = timo;			\
	if (!(condition))			\
		__wait_event_timeout(wq, condition, __ret); \
	__ret;					\
})

#define __wait_event_interruptible_timeout(wq, condition, ret) \
do {						\
	struct sleep_state sls;			\
	int deadline, __error, __error1;		\
						\
	atomic_inc_int(&(wq).count);		\
	sleep_setup(&sls, &wq, PCATCH, "drmweti"); \
	sleep_setup_timeout(&sls, ret);		\
	sleep_setup_signal(&sls, PCATCH);	\
	deadline = ticks + ret;			\
	sleep_finish(&sls, !(condition));	\
	ret = deadline - ticks;			\
	__error1 = sleep_finish_timeout(&sls);	\
	__error = sleep_finish_signal(&sls);	\
	atomic_dec_int(&(wq).count);		\
	if (ret < 0 || __error1 == EWOULDBLOCK)	\
		ret = 0;			\
	if (__error == ERESTART)			\
		ret = -ERESTARTSYS;		\
	else if (__error)				\
		ret = -__error;			\
	if (ret == 0 && (condition)) {		\
		ret = 1;			\
		break;				\
	}					\
} while (ret > 0 && !(condition))

#define wait_event_interruptible_timeout(wq, condition, timo) \
({						\
	long __ret = timo;			\
	if (!(condition))			\
		__wait_event_interruptible_timeout(wq, condition, __ret); \
	__ret;					\
})

#define wake_up(x)			wakeup(x)
#define wake_up_all(x)			wakeup(x)
#define wake_up_all_locked(x)		wakeup(x)
#define wake_up_interruptible(x)	wakeup(x)

#define waitqueue_active(wq)		((wq)->count > 0)

struct completion {
	u_int done;
	wait_queue_head_t wait;
};

#define INIT_COMPLETION(x) ((x).done = 0)

static inline void
init_completion(struct completion *x)
{
	x->done = 0;
	mtx_init(&x->wait.lock, IPL_NONE);
}

static inline u_long
wait_for_completion_interruptible_timeout(struct completion *x, u_long timo)
{
	int ret;

	mtx_enter(&x->wait.lock);
	while (x->done == 0) {
		ret = msleep(x, &x->wait.lock, PCATCH, "wfcit", timo);
		if (ret) {
			mtx_leave(&x->wait.lock);
			return (ret == EWOULDBLOCK) ? 0 : -ret;
		}
	}

	return 1;
}

static inline void
complete_all(struct completion *x)
{
	mtx_enter(&x->wait.lock);
	x->done = 1;
	mtx_leave(&x->wait.lock);
	wakeup(x);
}

struct workqueue_struct;

static inline struct workqueue_struct *
alloc_ordered_workqueue(const char *name, int flags)
{
	struct taskq *tq = taskq_create(name, 1, IPL_TTY, 0);
	return (struct workqueue_struct *)tq;
}

static inline void
destroy_workqueue(struct workqueue_struct *wq)
{
	taskq_destroy((struct taskq *)wq);
}

struct work_struct {
	struct task task;
	struct taskq *tq;
};

typedef void (*work_func_t)(struct work_struct *);

static inline void
INIT_WORK(struct work_struct *work, work_func_t func)
{
	work->tq = systq;
	task_set(&work->task, (void (*)(void *))func, work);
}

static inline bool
queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	work->tq = (struct taskq *)wq;
	return task_add(work->tq, &work->task);
}

static inline void
cancel_work_sync(struct work_struct *work)
{
	task_del(work->tq, &work->task);
}

struct delayed_work {
	struct work_struct work;
	struct timeout to;
	struct taskq *tq;
};

static inline struct delayed_work *
to_delayed_work(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}

static void
__delayed_work_tick(void *arg)
{
	struct delayed_work *dwork = arg;

	task_add(dwork->tq, &dwork->work.task);
}

static inline void
INIT_DELAYED_WORK(struct delayed_work *dwork, work_func_t func)
{
	INIT_WORK(&dwork->work, func);
	timeout_set(&dwork->to, __delayed_work_tick, &dwork->work);
}

static inline bool
schedule_work(struct work_struct *work)
{
	return task_add(work->tq, &work->task);
}

static inline bool
schedule_delayed_work(struct delayed_work *dwork, int jiffies)
{
	dwork->tq = systq;
	return timeout_add(&dwork->to, jiffies);
}

static inline bool
queue_delayed_work(struct workqueue_struct *wq,
    struct delayed_work *dwork, int jiffies)
{
	dwork->tq = (struct taskq *)wq;
	return timeout_add(&dwork->to, jiffies);
}

static inline bool
mod_delayed_work(struct workqueue_struct *wq,
    struct delayed_work *dwork, int jiffies)
{
	dwork->tq = (struct taskq *)wq;
	return (timeout_add(&dwork->to, jiffies) == 0);
}

static inline bool
cancel_delayed_work(struct delayed_work *dwork)
{
	if (timeout_del(&dwork->to))
		return true;
	return task_del(dwork->tq, &dwork->work.task);
}

static inline bool
cancel_delayed_work_sync(struct delayed_work *dwork)
{
	if (timeout_del(&dwork->to))
		return true;
	return task_del(dwork->tq, &dwork->work.task);
}

#define flush_workqueue(x)
#define flush_scheduled_work(x)
#define flush_delayed_work(x) (void)(x)

#define setup_timer(x, y, z)	timeout_set((x), (void (*)(void *))(y), (void *)(z))
#define mod_timer(x, y)		timeout_add((x), (y - jiffies))
#define del_timer_sync(x)	timeout_del((x))

#define NSEC_PER_USEC	1000L
#define NSEC_PER_SEC	1000000000L
#define KHZ2PICOS(a)	(1000000000UL/(a))

extern struct timespec ns_to_timespec(const int64_t);
extern int64_t timeval_to_ns(const struct timeval *);
extern struct timeval ns_to_timeval(const int64_t);

static inline struct timespec
timespec_sub(struct timespec t1, struct timespec t2)
{
	struct timespec diff;

	timespecsub(&t1, &t2, &diff);
	return diff;
}

#define time_in_range(x, min, max) ((x) >= (min) && (x) <= (max))

extern int ticks;
#define jiffies ticks
#undef HZ
#define HZ	hz

#define MAX_JIFFY_OFFSET	((INT_MAX >> 1) - 1)

static inline unsigned long
round_jiffies_up(unsigned long j)
{
	return roundup(j, hz);
}

static inline unsigned long
round_jiffies_up_relative(unsigned long j)
{
	return roundup(j, hz);
}

#define jiffies_to_msecs(x)	(((int64_t)(x)) * 1000 / hz)
#define msecs_to_jiffies(x)	(((int64_t)(x)) * hz / 1000)
#define time_after(a,b)		((long)(b) - (long)(a) < 0)
#define time_after_eq(a,b)	((long)(b) - (long)(a) <= 0)
#define get_seconds()		time_second
#define getrawmonotonic(x)	nanouptime(x)

static inline void
set_normalized_timespec(struct timespec *ts, time_t sec, int64_t nsec)
{
	while (nsec > NSEC_PER_SEC) {
		nsec -= NSEC_PER_SEC;
		sec++;
	}

	ts->tv_sec = sec;
	ts->tv_nsec = nsec;
}

static inline int64_t
timespec_to_ns(const struct timespec *ts)
{
	return ((ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec);
}

static inline int
timespec_to_jiffies(const struct timespec *ts)
{
	long long to_ticks;

	to_ticks = (long long)hz * ts->tv_sec + ts->tv_nsec / (tick * 1000);
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;

	return ((int)to_ticks);
}

static inline int
timespec_valid(const struct timespec *ts)
{
	if (ts->tv_sec < 0 || ts->tv_sec > 100000000 ||
	    ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000)
		return (0);
	return (1);
}

typedef struct timeval ktime_t;

static inline struct timeval
ktime_get(void)
{
	struct timeval tv;
	
	getmicrouptime(&tv);
	return tv;
}

static inline struct timeval
ktime_get_monotonic_offset(void)
{
	struct timeval tv = {0, 0};
	return tv;
}

static inline int64_t
ktime_to_ns(struct timeval tv)
{
	return timeval_to_ns(&tv);
}

#define ktime_to_timeval(tv) (tv)

static inline struct timeval
ktime_sub(struct timeval a, struct timeval b)
{
	struct timeval res;
	timersub(&a, &b, &res);
	return res;
}

static inline struct timeval
ktime_add_ns(struct timeval tv, int64_t ns)
{
	return ns_to_timeval(timeval_to_ns(&tv) + ns);
}

static inline struct timeval
ktime_sub_ns(struct timeval tv, int64_t ns)
{
	return ns_to_timeval(timeval_to_ns(&tv) - ns);
}

#define GFP_ATOMIC	M_NOWAIT
#define GFP_KERNEL	(M_WAITOK | M_CANFAIL)
#define GFP_TEMPORARY	(M_WAITOK | M_CANFAIL)
#define __GFP_NOWARN	0
#define __GFP_NORETRY	0

static inline void *
kmalloc(size_t size, int flags)
{
	return malloc(size, M_DRM, flags);
}

static inline void *
kmalloc_array(size_t n, size_t size, int flags)
{
	if (n == 0 || SIZE_MAX / n < size)
		return NULL;
	return malloc(n * size, M_DRM, flags);
}

static inline void *
kcalloc(size_t n, size_t size, int flags)
{
	if (n == 0 || SIZE_MAX / n < size)
		return NULL;
	return malloc(n * size, M_DRM, flags | M_ZERO);
}

static inline void *
kzalloc(size_t size, int flags)
{
	return malloc(size, M_DRM, flags | M_ZERO);
}

static inline void
kfree(void *objp)
{
	free(objp, M_DRM, 0);
}

static inline void *
kmemdup(const void *src, size_t len, int flags)
{
	void *p = malloc(len, M_DRM, flags);
	if (p)
		memcpy(p, src, len);
	return (p);
}

static inline void *
vzalloc(unsigned long size)
{
	return malloc(size, M_DRM, M_WAITOK | M_CANFAIL | M_ZERO);
}

static inline void
vfree(void *objp)
{
	free(objp, M_DRM, 0);
}

struct kref {
	uint32_t refcount;
};

static inline void
kref_init(struct kref *ref)
{
	ref->refcount = 1;
}

static inline void
kref_get(struct kref *ref)
{
	atomic_inc_int(&ref->refcount);
}

static inline int
kref_get_unless_zero(struct kref *ref)
{
	if (ref->refcount != 0) {
		atomic_inc_int(&ref->refcount);
		return (1);
	} else {
		return (0);
	}
}

static inline void
kref_put(struct kref *ref, void (*release)(struct kref *ref))
{
	if (atomic_dec_int_nv(&ref->refcount) == 0)
		release(ref);
}

static inline void
kref_sub(struct kref *ref, unsigned int v, void (*release)(struct kref *ref))
{
	if (atomic_sub_int_nv(&ref->refcount, v) == 0)
		release(ref);
}

struct kobject {
	struct kref kref;
	struct kobj_type *type;
};

struct kobj_type {
	void (*release)(struct kobject *);
};

static inline void
kobject_init(struct kobject *obj, struct kobj_type *type)
{
	kref_init(&obj->kref);
	obj->type = type;
}

static inline int
kobject_init_and_add(struct kobject *obj, struct kobj_type *type,
    struct kobject *parent, const char *fmt, ...)
{
	kobject_init(obj, type);
	return (0);
}

static inline struct kobject *
kobject_get(struct kobject *obj)
{
	if (obj != NULL)
		kref_get(&obj->kref);
	return (obj);
}

static inline void
kobject_release(struct kref *ref)
{
	struct kobject *obj = container_of(ref, struct kobject, kref);
	if (obj->type && obj->type->release)
		obj->type->release(obj);
}

static inline void
kobject_put(struct kobject *obj)
{
	if (obj != NULL)
		kref_put(&obj->kref, kobject_release);
}

static inline void
kobject_del(struct kobject *obj)
{
}

#define min_t(t, a, b) ({ \
	t __min_a = (a); \
	t __min_b = (b); \
	__min_a < __min_b ? __min_a : __min_b; })

#define max_t(t, a, b) ({ \
	t __max_a = (a); \
	t __max_b = (b); \
	__max_a > __max_b ? __max_a : __max_b; })

#define clamp_t(t, x, a, b) min_t(t, max_t(t, x, a), b)

#define do_div(n, base) \
	n = n / base

static inline uint64_t
div_u64(uint64_t x, uint32_t y)
{
	return (x / y);
}

static inline uint64_t
div64_u64(uint64_t x, uint64_t y)
{
	return (x / y);
}

static inline int64_t
div64_s64(int64_t x, int64_t y)
{
	return (x / y);
}

#define mult_frac(x, n, d) (((x) * (n)) / (d))

static inline int64_t
abs64(int64_t x)
{
	return (x < 0 ? -x : x);
}

static inline unsigned long
__copy_to_user(void *to, const void *from, unsigned len)
{
	if (copyout(from, to, len))
		return len;
	return 0;
}

static inline unsigned long
copy_to_user(void *to, const void *from, unsigned len)
{
	return __copy_to_user(to, from, len);
}

static inline unsigned long
__copy_from_user(void *to, const void *from, unsigned len)
{
	if (copyin(from, to, len))
		return len;
	return 0;
}

static inline unsigned long
copy_from_user(void *to, const void *from, unsigned len)
{
	return __copy_from_user(to, from, len);
}

#define get_user(x, ptr)	-copyin(ptr, &(x), sizeof(x))
#define put_user(x, ptr)	-copyout(&(x), ptr, sizeof(x))

static __inline uint16_t
hweight16(uint32_t x)
{
	x = (x & 0x5555) + ((x & 0xaaaa) >> 1);
	x = (x & 0x3333) + ((x & 0xcccc) >> 2);
	x = (x + (x >> 4)) & 0x0f0f;
	x = (x + (x >> 8)) & 0x00ff;
	return (x);
}

static inline uint32_t
hweight32(uint32_t x)
{
	x = (x & 0x55555555) + ((x & 0xaaaaaaaa) >> 1);
	x = (x & 0x33333333) + ((x & 0xcccccccc) >> 2);
	x = (x + (x >> 4)) & 0x0f0f0f0f;
	x = (x + (x >> 8));
	x = (x + (x >> 16)) & 0x000000ff;
	return x;
}

#define console_lock()
#define console_unlock()

#ifndef PCI_MEM_START
#define PCI_MEM_START	0
#endif

#ifndef PCI_MEM_END
#define PCI_MEM_END	0xffffffff
#endif

enum dmi_field {
        DMI_NONE,
        DMI_BIOS_VENDOR,
        DMI_BIOS_VERSION,
        DMI_BIOS_DATE,
        DMI_SYS_VENDOR,
        DMI_PRODUCT_NAME,
        DMI_PRODUCT_VERSION,
        DMI_PRODUCT_SERIAL,
        DMI_PRODUCT_UUID,
        DMI_BOARD_VENDOR,
        DMI_BOARD_NAME,
        DMI_BOARD_VERSION,
        DMI_BOARD_SERIAL,
        DMI_BOARD_ASSET_TAG,
        DMI_CHASSIS_VENDOR,
        DMI_CHASSIS_TYPE,
        DMI_CHASSIS_VERSION,
        DMI_CHASSIS_SERIAL,
        DMI_CHASSIS_ASSET_TAG,
        DMI_STRING_MAX,
};

struct dmi_strmatch {
	unsigned char slot;
	char substr[79];
};

struct dmi_system_id {
        int (*callback)(const struct dmi_system_id *);
        const char *ident;
        struct dmi_strmatch matches[4];
};
#define	DMI_MATCH(a, b) {(a), (b)}
#define	DMI_EXACT_MATCH(a, b) {(a), (b)}
int dmi_check_system(const struct dmi_system_id *);

struct resource {
	u_long	start;
};

struct pci_bus {
	unsigned char	number;
};

struct pci_dev {
	struct pci_bus	_bus;
	struct pci_bus	*bus;

	unsigned int	devfn;
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subsystem_vendor;
	uint16_t	subsystem_device;

	pci_chipset_tag_t pc;
	pcitag_t	tag;
	struct pci_softc *pci;
};
#define PCI_ANY_ID (uint16_t) (~0U)

#define PCI_VENDOR_ID_ASUSTEK	PCI_VENDOR_ASUSTEK
#define PCI_VENDOR_ID_ATI	PCI_VENDOR_ATI
#define PCI_VENDOR_ID_DELL	PCI_VENDOR_DELL
#define PCI_VENDOR_ID_HP	PCI_VENDOR_HP
#define PCI_VENDOR_ID_IBM	PCI_VENDOR_IBM
#define PCI_VENDOR_ID_INTEL	PCI_VENDOR_INTEL
#define PCI_VENDOR_ID_SONY	PCI_VENDOR_SONY
#define PCI_VENDOR_ID_VIA	PCI_VENDOR_VIATECH

#define PCI_DEVICE_ID_ATI_RADEON_QY	PCI_PRODUCT_ATI_RADEON_QY

#define PCI_DEVFN(slot, func)	((slot) << 3 | (func))
#define PCI_SLOT(devfn)		((devfn) >> 3)
#define PCI_FUNC(devfn)		((devfn) & 0x7)

static inline void
pci_read_config_dword(struct pci_dev *pdev, int reg, u32 *val)
{
	*val = pci_conf_read(pdev->pc, pdev->tag, reg);
} 

static inline void
pci_read_config_word(struct pci_dev *pdev, int reg, u16 *val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x2));
	*val = (v >> ((reg & 0x2) * 8));
} 

static inline void
pci_read_config_byte(struct pci_dev *pdev, int reg, u8 *val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x3));
	*val = (v >> ((reg & 0x3) * 8));
} 

static inline void
pci_write_config_dword(struct pci_dev *pdev, int reg, u32 val)
{
	pci_conf_write(pdev->pc, pdev->tag, reg, val);
} 

static inline void
pci_write_config_word(struct pci_dev *pdev, int reg, u16 val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x2));
	v &= ~(0xffff << ((reg & 0x2) * 8));
	v |= (val << ((reg & 0x2) * 8));
	pci_conf_write(pdev->pc, pdev->tag, (reg & ~0x2), v);
} 

static inline void
pci_write_config_byte(struct pci_dev *pdev, int reg, u8 val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x3));
	v &= ~(0xff << ((reg & 0x3) * 8));
	v |= (val << ((reg & 0x3) * 8));
	pci_conf_write(pdev->pc, pdev->tag, (reg & ~0x3), v);
}

typedef enum {
	PCI_D0,
	PCI_D1,
	PCI_D2,
	PCI_D3hot,
	PCI_D3cold
} pci_power_t;

#if defined(__amd64__) || defined(__i386__)

#define PCI_DMA_BIDIRECTIONAL	0

static inline dma_addr_t
pci_map_page(struct pci_dev *pdev, struct vm_page *page, unsigned long offset, size_t size, int direction)
{
	return VM_PAGE_TO_PHYS(page);
}

static inline void
pci_unmap_page(struct pci_dev *pdev, dma_addr_t dma_address, size_t size, int direction)
{
}

static inline int
pci_dma_mapping_error(struct pci_dev *pdev, dma_addr_t dma_addr)
{
	return 0;
}

#define VGA_RSRC_LEGACY_IO	0x01

void vga_get_uninterruptible(struct pci_dev *, int);
void vga_put(struct pci_dev *, int);

#endif

#define memcpy_toio(d, s, n)	memcpy(d, s, n)
#define memcpy_fromio(d, s, n)	memcpy(d, s, n)
#define memset_io(d, b, n)	memset(d, b, n)

static inline u32
ioread32(const volatile void __iomem *addr)
{
	return (*(volatile uint32_t *)addr);
}

static inline u64
ioread64(const volatile void __iomem *addr)
{
	return (*(volatile uint64_t *)addr);
}

static inline void
iowrite32(u32 val, volatile void __iomem *addr)
{
	*(volatile uint32_t *)addr = val;
}

#define readl(p) ioread32(p)
#define writel(v, p) iowrite32(v, p)
#define readq(p) ioread64(p)

#define page_to_phys(page)	(VM_PAGE_TO_PHYS(page))
#define page_to_pfn(pp)		(VM_PAGE_TO_PHYS(pp) / PAGE_SIZE)
#define offset_in_page(off)	((off) & PAGE_MASK)
#define set_page_dirty(page)	atomic_clearbits_int(&page->pg_flags, PG_CLEAN)

#define VERIFY_READ	0x1
#define VERIFY_WRITE	0x2
static inline int
access_ok(int type, const void *addr, unsigned long size)
{
	return true;
}

#define CAP_SYS_ADMIN	0x1
static inline int
capable(int cap)
{
	KASSERT(cap == CAP_SYS_ADMIN);
	return suser(curproc, 0);
}

typedef int pgprot_t;
#define pgprot_val(v)	(v)
#define PAGE_KERNEL	0

void	*kmap(struct vm_page *);
void	 kunmap(void *addr);
void	*vmap(struct vm_page **, unsigned int, unsigned long, pgprot_t);
void	 vunmap(void *, size_t);

#define round_up(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define round_down(x, y) (((x) / (y)) * (y))
#define roundup2(x, y) (((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */
#define DIV_ROUND_UP(x, y)	(((x) + ((y) - 1)) / (y))
#define DIV_ROUND_UP_ULL(x, y)	DIV_ROUND_UP(x, y)
#define DIV_ROUND_CLOSEST(x, y)	(((x) + ((y) / 2)) / (y))

static inline unsigned long
roundup_pow_of_two(unsigned long x)
{
	return (1UL << flsl(x - 1));
}

#define is_power_of_2(x)	(x != 0 && (((x) - 1) & (x)) == 0)

#define PAGE_ALIGN(addr)	(((addr) + PAGE_MASK) & ~PAGE_MASK)
#define IS_ALIGNED(x, y)	(((x) & ((y) - 1)) == 0)

static __inline void
udelay(unsigned long usecs)
{
	DELAY(usecs);
}

static __inline void
ndelay(unsigned long nsecs)
{
	DELAY(max(nsecs / 1000, 1));
}

static __inline void
usleep_range(unsigned long min, unsigned long max)
{
	DELAY(min);
}

static __inline void
mdelay(unsigned long msecs)
{
	int loops = msecs;
	while (loops--)
		DELAY(1000);
}

static inline uint32_t ror32(uint32_t word, unsigned int shift)
{
	return (word >> shift) | (word << (32 - shift));
}

static inline int
irqs_disabled(void)
{
	return (cold);
}

static inline int
in_dbg_master(void)
{
#ifdef DDB
	return (db_is_active);
#endif
	return (0);
}

#define oops_in_progress in_dbg_master()

static inline int
power_supply_is_system_supplied(void)
{
	/* XXX return 0 if on battery */
	return (1);
}

#define _U      0x01
#define _L      0x02
#define _N      0x04
#define _S      0x08
#define _P      0x10
#define _C      0x20
#define _X      0x40
#define _B      0x80

static inline int
isascii(int c)
{
	return ((unsigned int)c <= 0177);
}

static inline int
isprint(int c)
{
	if (c == -1)
		return (0);
	if ((unsigned char)c >= 040 && (unsigned char)c <= 0176)
		return (1);
	return (0);
}

#ifdef __macppc__
static __inline int
of_machine_is_compatible(const char *model)
{
	extern char *hw_prod;
	return (strcmp(model, hw_prod) == 0);
}
#endif

struct vm_page *alloc_pages(unsigned int, unsigned int);
void	__free_pages(struct vm_page *, unsigned int);

static inline struct vm_page *
alloc_page(unsigned int gfp_mask)
{
	return alloc_pages(gfp_mask, 0);
}

static inline void
__free_page(struct vm_page *page)
{
	return __free_pages(page, 0);
}

static inline unsigned int
get_order(size_t size)
{
	return flsl((size - 1) >> PAGE_SHIFT);
}

#if defined(__i386__) || defined(__amd64__)

static inline void
pagefault_disable(void)
{
	KASSERT(curcpu()->ci_inatomic == 0);
	curcpu()->ci_inatomic = 1;
}

static inline void
pagefault_enable(void)
{
	KASSERT(curcpu()->ci_inatomic == 1);
	curcpu()->ci_inatomic = 0;
}

static inline int
in_atomic(void)
{
	return curcpu()->ci_inatomic;
}

static inline void *
kmap_atomic(struct vm_page *pg)
{
	vaddr_t va;

#if defined (__HAVE_PMAP_DIRECT)
	va = pmap_map_direct(pg);
#else
	extern vaddr_t pmap_tmpmap_pa(paddr_t);
	va = pmap_tmpmap_pa(VM_PAGE_TO_PHYS(pg));
#endif
	return (void *)va;
}

static inline void
kunmap_atomic(void *addr)
{
#if defined (__HAVE_PMAP_DIRECT)
	pmap_unmap_direct((vaddr_t)addr);
#else
	extern void pmap_tmpunmap_pa(void);
	pmap_tmpunmap_pa();
#endif
}

static inline unsigned long
__copy_to_user_inatomic(void *to, const void *from, unsigned len)
{
	struct cpu_info *ci = curcpu();
	int inatomic = ci->ci_inatomic;
	int error;

	ci->ci_inatomic = 1;
	error = copyout(from, to, len);
	ci->ci_inatomic = inatomic;

	return (error ? len : 0);
}

static inline unsigned long
__copy_from_user_inatomic(void *to, const void *from, unsigned len)
{
	struct cpu_info *ci = curcpu();
	int inatomic = ci->ci_inatomic;
	int error;

	ci->ci_inatomic = 1;
	error = copyin(from, to, len);
	ci->ci_inatomic = inatomic;

	return (error ? len : 0);
}

static inline unsigned long
__copy_from_user_inatomic_nocache(void *to, const void *from, unsigned len)
{
	return __copy_from_user_inatomic(to, from, len);
}

#endif

struct fb_var_screeninfo {
	int pixclock;
};

struct fb_info {
	struct fb_var_screeninfo var;
	void *par;
};

#define framebuffer_alloc(flags, device) \
	kzalloc(sizeof(struct fb_info), GFP_KERNEL)

struct address_space;
#define unmap_mapping_range(mapping, holebegin, holeend, even_cows)

/*
 * ACPI types and interfaces.
 */

typedef size_t acpi_size;
typedef int acpi_status;

struct acpi_table_header;

#define ACPI_SUCCESS(x) ((x) == 0)

#define AE_NOT_FOUND	0x0005

acpi_status acpi_get_table_with_size(const char *, int, struct acpi_table_header **, acpi_size *);
