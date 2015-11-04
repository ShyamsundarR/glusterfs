/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef __ZFSTORE_H__
#define __ZFSTORE_H__

#include "glusterfs.h"
#include "defaults.h"
#include "common-utils.h"
#include "compat-errno.h"
#include "syscall.h"

#include "posix2.h"
#include "super.h"

#define ZFSTORE_ENTRY_HANDLE_FMT  "%s/%02x/%02x/%s"

void *zfstore_ctor (xlator_t *, const char *);
int zfstore_dtor (xlator_t *, void *);

/**
 * ZFstore deals with name entry and it's inode separately and thereby
 * their respective metadata operations should be distinct. This ties
 * the store closely with the metadata store which should understand
 * filestores capabilities. Therefore, as a rule of thumb, metadata
 * store implementation should vary according to filestores capability.
 *
 * Got it? If not, read again.
 */
struct zfstore {
        gf_lock_t lock;

        char *exportdir;

        DIR *mountlock;

        struct md_namei_ops *ni;
};

struct xlator_fops zfstore_fops;

/* store operations */
int32_t zfstore_lookup (call_frame_t *, xlator_t *, loc_t *, dict_t *);
int32_t zfstore_create (call_frame_t *, xlator_t *,
                        loc_t *, int32_t, mode_t, mode_t, fd_t *, dict_t *);
int32_t zfstore_flush (call_frame_t *, xlator_t *, fd_t *, dict_t *);
int32_t zfstore_setattr (call_frame_t *, xlator_t *,
                         loc_t *, struct iatt *, int32_t, dict_t *);
int32_t zfstore_stat (call_frame_t *, xlator_t *, loc_t *, dict_t *);

static inline struct mdoperations *zf_get_nameops (struct zfstore *zf)
{
        if (!zf || !zf->ni || !zf->ni->nameops)
                return NULL;
        return zf->ni->nameops;
}

static inline struct mdoperations *zf_get_inodeops (struct zfstore *zf)
{
        if (!zf || !zf->ni || !zf->ni->inodeops)
                return NULL;
        return zf->ni->inodeops;
}

#endif /* __ZFSTORE_H__ */
