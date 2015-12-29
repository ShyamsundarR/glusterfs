/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "posix2-ds-helpers.h"
#include "posix2.h"
#include "posix2-ds.h"
#include "posix2-mem-types.h"

int32_t
posix2_ds_stat_handle (xlator_t *this,
                       uuid_t gfid, char *path, struct iatt *stbuf)
{
        int32_t      ret      = 0;
        struct stat  lstatbuf      = {0,};

        ret = lstat (path, &lstatbuf);
        if (ret)
                return -1;

        if (stbuf) {
                iatt_from_stat (stbuf, &lstatbuf);
                gf_uuid_copy (stbuf->ia_gfid, gfid);
                posix2_fill_ino_from_gfid (this, stbuf);
        }
        return 0;
}

int32_t
posix2_ds_resolve_inodeptr (xlator_t *this,
                          uuid_t tgtuuid, char *ihandle,
                          struct iatt *stbuf)
{
        return posix2_ds_stat_handle (this, tgtuuid,
                                     ihandle, stbuf);
}

static int
__posix2_fd_ctx_get (fd_t *fd, xlator_t *this, struct posix2_fd **pfd_p)
{
        uint64_t           tmp_pfd   = 0;
        struct posix2_fd  *pfd       = NULL;
        int                ret       = -1;
        int                _fd       = -1;
        char              *export    = NULL;
        struct posix2_ds  *ds        = NULL;
        struct iatt        buf       = {0,};
        uuid_t             tgtuuid   = {0,};
        int                entrylen  = 0;
        char              *entry     = NULL;

        ds = this->private;
        export = ds->exportdir;

        ret = __fd_ctx_get (fd, this, &tmp_pfd);
        if (ret == 0) {
                pfd = (void *)(long) tmp_pfd;
                ret = 0;
                goto out;
        }

        if (fd->inode) {
                gf_uuid_copy (tgtuuid, fd->inode->gfid);
        }
        else {
                ret = -1;
                goto err;
        }

        entrylen = posix2_handle_length (export);
        entry = alloca (entrylen);
        entrylen = posix2_make_handle (this, export, tgtuuid, entry, entrylen);
        if (entrylen <= 0)
                goto err;
        ret = posix2_ds_resolve_inodeptr
                       (this, tgtuuid, entry, &buf);
        if (ret < 0)
                goto err;

        if (!fd_is_anonymous(fd)) {
                gf_log (this->name, GF_LOG_ERROR,
                        "Failed to get fd context for a non-anonymous fd, "
                        "file: %s, gfid: %s", entry,
                        uuid_utoa (fd->inode->gfid));
                goto err;
        }

        pfd = GF_CALLOC (1, sizeof (*pfd), gf_posix2_mt_posix2_fd_t);
        if (!pfd) {
                goto err;
        }
        pfd->fd = -1;

        /* 1. Using fd->flags in case we choose to have anonymous
         *    fds with different flags some day. As of today it
         *    would be GF_ANON_FD_FLAGS and nothing else.
         * 2. Assuming only regular files on DS side, go ahead and open.
         */
        _fd = open (entry, fd->flags);
        if (_fd == -1) {
                ret = -1;
                goto free_pfd;
        }
        pfd->fd = _fd;

        ret = __fd_ctx_set (fd, this, (uint64_t) (long) pfd);
        if (ret != 0)
                goto free_pfd;

        ret = 0;

out:
        if (pfd_p)
                *pfd_p = pfd;
        return ret;

free_pfd:
        GF_FREE (pfd);
        pfd = NULL;
        if (_fd != -1)
                close (_fd);
err:
        return ret;
}

int
posix2_fd_ctx_get (fd_t *fd, xlator_t *this, struct posix2_fd **pfd)
{
        int   ret;

        LOCK (&fd->inode->lock);
        {
                ret = __posix2_fd_ctx_get (fd, this, pfd);
        }
        UNLOCK (&fd->inode->lock);

        return ret;
}
