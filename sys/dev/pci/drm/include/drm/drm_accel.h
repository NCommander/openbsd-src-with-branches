/* Public domain. */

#ifndef _DRM_ACCEL_H
#define _DRM_ACCEL_H

#include <drm/drm_file.h>

static inline int
accel_minor_alloc(void)
{
	return -ENOSYS;
}

static inline void
accel_minor_remove(int i)
{
}

static inline void
accel_minor_replace(struct drm_minor *m, int i)
{
}

static inline int
accel_core_init(void)
{
	return 0;
}

static inline void
accel_core_exit(void)
{
}

#endif
