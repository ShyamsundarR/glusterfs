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

#define ZFSTORE_ENTRY_HANDLE_FMT  "%s/%02x/%02x/%s"

void *zfstore_ctor (xlator_t *, const char *);
int zfstore_dtor (xlator_t *, void *);

struct zfstore {
        gf_lock_t lock;

        char *exportdir;

        DIR *mountlock;
};

struct xlator_fops zfstore_fops;

/* store operations */
int32_t zfstore_lookup (call_frame_t *, xlator_t *, loc_t *, dict_t *);
int32_t zfstore_create (call_frame_t *, xlator_t *,
                        loc_t *, int32_t, mode_t, mode_t, fd_t *, dict_t *);

#endif /* __ZFSTORE_H__ */
