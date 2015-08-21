/*
   Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/
#define __XOPEN_SOURCE 500

#include <string.h>
#include <stdio.h>

#ifndef GF_BSD_HOST_OS
#include <alloca.h>
#endif /* GF_BSD_HOST_OS */

#ifdef HAVE_LINKAT
#include <fcntl.h>
#endif /* HAVE_LINKAT */

#include "posix2.h"
#include "xlator.h"
#include "uuid.h"
#include "common-utils.h"
#include "fd.h"

extern char *marker_xattrs[];
#define ALIGN_SIZE 4096

#undef HAVE_SET_FSID
#ifdef HAVE_SET_FSID

#define DECLARE_OLD_FS_ID_VAR uid_t old_fsuid; gid_t old_fsgid;

#define SET_FS_ID(uid, gid) do {                \
                old_fsuid = setfsuid (uid);     \
                old_fsgid = setfsgid (gid);     \
        } while (0)

#define SET_TO_OLD_FS_ID() do {                 \
                setfsuid (old_fsuid);           \
                setfsgid (old_fsgid);           \
        } while (0)

#else

#define DECLARE_OLD_FS_ID_VAR
#define SET_FS_ID(uid, gid)
#define SET_TO_OLD_FS_ID()

#endif

#define SLEN(str) (sizeof(str) - 1)

int
posix_construct_gfid_path (xlator_t *this, uuid_t gfid, const char *basename,
                        char *buf, size_t buflen)
{
        struct posix_private *priv = NULL;
        char                 *uuid_str = NULL;
        int                   len = 0;

        priv = this->private;

        len = priv->base_path_length  /* option directory "/export" */
                + POSIX_HANDLE_LENGTH;

        if (basename) {
                len += (strlen (basename) + 1);
        }

        if ((buflen < len) || !buf)
                return len;

        uuid_str = uuid_utoa (gfid);

        if (basename) {
                len = snprintf (buf, buflen, "%s/%02x/%02x/%s/%s",
                                priv->base_path, gfid[0],
                                gfid[1], uuid_str, basename);
        } else {
                len = snprintf (buf, buflen, "%s/%02x/%02x/%s",
                                priv->base_path, gfid[0],
                                gfid[1], uuid_str);
        }
out:
        return len;
}

int
posix_fill_gfid_from_path (xlator_t *this, const char *path, struct iatt *iatt)
{
        int ret = 0;
        ssize_t size = 0;

        if (!iatt)
                return 0;

        size = sys_lgetxattr (path, GFID_XATTR_KEY, iatt->ia_gfid, 16);
        /* Return value of getxattr */
        if ((size != 16) || (size == -1))
                ret = -1;
        else
                ret = 0;

        return ret;
}

void
posix_fill_ino_from_gfid (xlator_t *this, struct iatt *buf)
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

static int
__posix_fd_ctx_get (fd_t *fd, xlator_t *this, struct posix_fd **pfd_p)
{
        uint64_t          tmp_pfd = 0;
        struct posix_fd  *pfd = NULL;
        int               ret = -1;
        char             *real_path = NULL;
        int               _fd = -1;
        DIR              *dir = NULL;

        ret = __fd_ctx_get (fd, this, &tmp_pfd);
        if (ret == 0) {
                pfd = (void *)(long) tmp_pfd;
                ret = 0;
                goto out;
        }

        if (!fd_is_anonymous(fd)) {
                gf_log (this->name, GF_LOG_ERROR,
                        "Failed to get fd context for a non-anonymous fd, "
                        "gfid: %s", uuid_utoa (fd->inode->gfid));
                goto out;
        }

        MAKE_HANDLE_PATH (real_path, this, fd->inode->gfid, NULL);
        if (!real_path) {
                gf_msg (this->name, GF_LOG_ERROR, 0,
                        P_MSG_HANDLE_PATH_CREATE_FAILED,
                        "Failed to create handle path (%s)",
                        uuid_utoa (fd->inode->gfid));
                ret = -1;
                goto out;
        }

        pfd = GF_CALLOC (1, sizeof (*pfd), gf_posix_mt_posix_fd);
        if (!pfd) {
                goto out;
        }
        pfd->fd = -1;

        if (fd->inode->ia_type == IA_IFDIR) {
                dir = opendir (real_path);
                if (!dir) {
                        GF_FREE (pfd);
                        pfd = NULL;
                        goto out;
                }
                _fd = dirfd (dir);
        }

        /* Using fd->flags in case we choose to have anonymous
         * fds with different flags some day. As of today it
         * would be GF_ANON_FD_FLAGS and nothing else.
         */
        if (fd->inode->ia_type == IA_IFREG) {
                _fd = open (real_path, fd->flags);
                if (_fd == -1) {
                        GF_FREE (pfd);
                        pfd = NULL;
                        goto out;
                }
        }

        pfd->fd = _fd;
        pfd->dir = dir;

        ret = __fd_ctx_set (fd, this, (uint64_t) (long) pfd);
        if (ret != 0) {
                if (_fd != -1)
                        close (_fd);
                if (dir)
                        closedir (dir);
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
posix_fd_ctx_get (fd_t *fd, xlator_t *this, struct posix_fd **pfd)
{
        int   ret;

        LOCK (&fd->inode->lock);
        {
                ret = __posix_fd_ctx_get (fd, this, pfd);
        }
        UNLOCK (&fd->inode->lock);

        return ret;
}
