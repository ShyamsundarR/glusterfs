/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef __POSIX2_DS_HELPERS_H__
#define __POSIX2_DS_HELPERS_H__

#include <sys/types.h>
#include "xlator.h"
#include "posix2.h"

int32_t
posix2_ds_stat_handle (xlator_t *, uuid_t, char *, struct iatt *);
int32_t
posix2_ds_resolve_inodeptr (xlator_t *, uuid_t, char *, struct iatt *);
int
posix2_fd_ctx_get (fd_t *fd, xlator_t *this, struct posix2_fd **pfd);
int
posix2_fdstat (xlator_t *, int, struct iatt *);
#endif /*__POSIX2_DS_HELPERS_H__*/
