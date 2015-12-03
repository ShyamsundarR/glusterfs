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
        if (flags & O_DIRECTORY) {
                pfd->dirfd = fdopendir (openfd);
                if (pfd->dirfd == NULL) {
                        GF_FREE (pfd);
                        return -1;
                }
        }

        ret = fd_ctx_set (fd, this, (uint64_t)(long)pfd);
        return ret;
}

int32_t
posix2_fetch_openfd (xlator_t *this, fd_t *fd, struct posix2_raw_fd *retfd)
{
        int32_t           ret = 0;
        uint64_t          pfd_ctx = 0;
        struct posix2_fd *pfd = NULL;

        ret = fd_ctx_get (fd, this, &pfd_ctx);
        if (ret)
                goto out;

        pfd = (struct posix2_fd *)pfd_ctx;
        if (pfd) {
                if (pfd->dirfd) {
                        retfd->fd.dirfd = pfd->dirfd;
                        retfd->type = POSIX2_DIR_FD;
                }
                else {
                        retfd->fd.filefd = pfd->fd;
                        retfd->type = POSIX2_FILE_FD;
                }
        } else
                ret = -1;

out:
        return ret;
}

int32_t
posix2_release_openfd (xlator_t *this, fd_t *fd)
{
        int32_t           ret = 0;
        uint64_t          pfd_ctx = 0;
        struct posix2_fd *pfd = NULL;

        ret = fd_ctx_del (fd, this, &pfd_ctx);
        if (ret)
                goto out;

        pfd = (struct posix2_fd *)pfd_ctx;
        if (pfd) {
                if (pfd->dirfd)
                        closedir (pfd->dirfd);
                else
                        close (pfd->fd);
        }
out:
        return ret;
}
