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
int32_t
zfstore_open (call_frame_t *frame, xlator_t *this,
              loc_t *loc, int32_t flags, fd_t *fd, dict_t *xdata);
int32_t zfstore_flush (call_frame_t *, xlator_t *, fd_t *, dict_t *);
int32_t zfstore_setattr (call_frame_t *, xlator_t *,
                         loc_t *, struct iatt *, int32_t, dict_t *);
int32_t zfstore_stat (call_frame_t *, xlator_t *, loc_t *, dict_t *);
int32_t
zfstore_do_namei (xlator_t *this,
                  char *parpath, loc_t *loc, fd_t *fd,
                  int32_t flags, mode_t mode, dict_t *xdata, struct iatt *stbuf);
int32_t
zfstore_open_inode (xlator_t *this,
                    char *export, uuid_t gfid, fd_t *fd, int32_t flags);

#endif /* __ZFSTORE_H__ */
