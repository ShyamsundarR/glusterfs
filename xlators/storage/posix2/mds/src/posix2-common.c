/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "posix2.h"
#include "posix2-mem-types.h"

/**
 * Set of common (helper) routines for metadata & data implementation.
 * Make sure whatever ends up here is "really really" common. No hacks
 * please.
 */

int32_t
posix2_lookup_is_nameless (loc_t *loc)
{
        return (gf_uuid_is_null (loc->pargfid) && !loc->name);
}

void
posix2_fill_ino_from_gfid (xlator_t *this, struct iatt *buf)
{
        uint64_t temp_ino = 0;
        int j = 0;
        int i = 0;

        /* consider least significant 8 bytes of value out of gfid */
        if (gf_uuid_is_null (buf->ia_gfid)) {
                buf->ia_ino = -1;
                goto out;
        }
        for (i = 15; i > (15 - 8); i--) {
                temp_ino += (uint64_t)(buf->ia_gfid[i]) << j;
                j += 8;
        }
        buf->ia_ino = temp_ino;
out:
        return;
}

int32_t
posix2_save_openfd (xlator_t *this, fd_t *fd, int openfd, int32_t flags)
{
        int32_t ret = 0;
        struct posix2_fd *pfd = NULL;

        pfd = GF_CALLOC (1, sizeof (*pfd), gf_posix2_mt_posix2_fd_t);
        if (!pfd)
                return -1;

        pfd->fd = openfd;
        pfd->flags = flags;

        ret = fd_ctx_set (fd, this, (uint64_t)(long)pfd);
        return ret;
}
