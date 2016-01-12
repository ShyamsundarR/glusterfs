/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

int
zfstore_fd_ctx_get (fd_t *, xlator_t *, struct posix2_fd **);
int
zfstore_fdstat (xlator_t *, int, uuid_t, struct iatt *);
