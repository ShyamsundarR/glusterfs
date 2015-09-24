/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef __XATTRSTORE_H__
#define __XATTRSTORE_H__

#include "posix2.h"

#define XAS_ENTRY_HANDLE_FMT  "%s/%02x/%02x/%s"

void *xattrstore_ctor (xlator_t *, const char *);
int xattrstore_dtor (xlator_t *, void *);

struct xlator_fops xattrstore_fops;

struct xattrstore {
        gf_lock_t lock;

        char *exportdir;

        DIR *mountlock;
};

/* store operations */

int32_t xattrstore_lookup (call_frame_t *, xlator_t *, loc_t *, dict_t *);

struct xlator_fops xattrstore_fops = {
        .lookup = xattrstore_lookup,
};

#endif /* __XATTRSTORE_H__ */
