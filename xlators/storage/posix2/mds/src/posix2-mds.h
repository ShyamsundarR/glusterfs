/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef __POSIX_MDS_H__
#define __POSIX_MDS_H__

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include "xlator.h"
#include "inode.h"
#include "compat.h"
#include "locking.h"

#include "posix2.h"

#ifdef CONFIG_POSIX_MDS_STATS

struct posix2_mds_io_stats {
        gf_lock_t statlock;

        unsigned long long nr_objects;

        unsigned long long nr_opens;
        unsigned long long nr_anonfd_ops;

        unsigned long long read_count;
        unsigned long long write_count;
};

#endif

struct posix2_mdstore_handler {
        void * (*ctor)(xlator_t *, const char *);
        int (*dtor)(xlator_t *, void *);

        struct xlator_fops *storeops;
};

struct posix2_mds {
        char *hostname;

#ifdef CONFIG_POSIX_MDS_STATS
        struct posix2_mds_io_stats iostats;
#endif

        void *store;                            /* metadata store */
        struct posix2_mdstore_handler *chandle; /* cached handle of store */
};

static inline void posix2_set_mds_store (struct posix2_mds *mds, void *store)
{
        mds->store = store;
}

static inline void posix2_cache_mds_handle (struct posix2_mds *mds,
                                            struct posix2_mdstore_handler *hdl)
{
        mds->chandle = hdl;
}

/* when ->private is valid, use this to set mds store/handle */
static inline void posix2_set_store (xlator_t *this, void *store)
{
        struct posix2_mds *mds = this->private;

        mds->store = store;
}

static inline void posix2_cache_handle (xlator_t *this,
                                        struct posix2_mdstore_handler *handle)
{
        struct posix2_mds *mds = this->private;

        mds->chandle = handle;
}

/* fetch store/handle */
static inline void *posix2_get_store (xlator_t *this)
{
        return ((struct posix2_mds *)(this->private))->store;
}

static inline struct posix2_mdstore_handler *
                     posix2_get_cached_handle (xlator_t *this)
{
        return ((struct posix2_mds *)(this->private))->chandle;
}

#endif /* __POSIX_MDS_H__ */
