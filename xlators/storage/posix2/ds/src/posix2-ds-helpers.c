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
        char              *real_path = NULL;
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
                goto out;
        }

        entrylen = posix2_handle_length (export);
        entry = alloca (entrylen);
        entrylen = posix2_make_handle (this, export, tgtuuid, entry, entrylen);
        if (entrylen <= 0)
                goto out;
        ret = posix2_ds_resolve_inodeptr
                       (this, tgtuuid, entry, &buf);
        if (ret < 0)
                goto out;

        if (!fd_is_anonymous(fd)) {
                gf_log (this->name, GF_LOG_ERROR,
                        "Failed to get fd context for a non-anonymous fd, "
                        "file: %s, gfid: %s", real_path,
                        uuid_utoa (fd->inode->gfid));
                goto out;
        }

        pfd = GF_CALLOC (1, sizeof (*pfd), gf_posix2_ds_mt_posix2_fd_t);
        if (!pfd) {
                goto out;
        }
        pfd->fd = -1;

        /* Using fd->flags in case we choose to have anonymous
         * fds with different flags some day. As of today it
         * would be GF_ANON_FD_FLAGS and nothing else.
         */
        if (fd->inode->ia_type == IA_IFREG) {
                _fd = open (entry, fd->flags);
                if (_fd == -1) {
                        GF_FREE (pfd);
                        pfd = NULL;
                        goto out;
                }
        }

        pfd->fd = _fd;

        ret = __fd_ctx_set (fd, this, (uint64_t) (long) pfd);
        if (ret != 0) {
                if (_fd != -1)
                        close (_fd);
                GF_FREE (pfd);
                pfd = NULL;
                goto out;
        }

        ret = 0;
out:
        if (pfd_p)
                *pfd_p = pfd;
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

int
posix2_fdstat (xlator_t *this, int fd, struct iatt *stbuf_p)
{
        int                    ret     = 0;
        struct stat            fstatbuf = {0, };
        struct iatt            stbuf = {0, };

        ret = fstat (fd, &fstatbuf);
        if (ret == -1)
                goto out;

        if (fstatbuf.st_nlink && !S_ISDIR (fstatbuf.st_mode))
                fstatbuf.st_nlink--;

        iatt_from_stat (&stbuf, &fstatbuf);

/*
        ret = posix_fill_gfid_fd (this, fd, &stbuf);
        if (ret)
                gf_log_callingfn (this->name, GF_LOG_DEBUG, "failed to get gfid");
*/
        posix2_fill_ino_from_gfid (this, &stbuf);

        if (stbuf_p)
                *stbuf_p = stbuf;

out:
        return ret;
}
