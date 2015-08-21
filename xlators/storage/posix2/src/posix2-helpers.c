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

static char* posix_ignore_xattrs[] = {
        "gfid-req",
        GLUSTERFS_ENTRYLK_COUNT,
        GLUSTERFS_INODELK_COUNT,
        GLUSTERFS_POSIXLK_COUNT,
        GLUSTERFS_PARENT_ENTRYLK,
        GF_GFIDLESS_LOOKUP,
        GLUSTERFS_INODELK_DOM_COUNT,
        GLUSTERFS_INTERNAL_FOP_KEY,
        NULL
};

static char* list_xattr_ignore_xattrs[] = {
        GF_SELINUX_XATTR_KEY,
        GF_XATTR_VOL_ID_KEY,
        GFID_XATTR_KEY,
        NULL
};

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

int
posix_pstat (xlator_t *this, uuid_t gfid, const char *path,
             struct iatt *buf_p)
{
        struct stat  lstatbuf = {0, };
        struct iatt  stbuf = {0, };
        int          ret = 0;
        struct posix_private *priv = NULL;


        priv = this->private;

        ret = sys_lstat (path, &lstatbuf);

        if (ret != 0) {
                if (ret == -1) {
                        if (errno != ENOENT)
                                gf_msg (this->name, GF_LOG_WARNING, errno,
                                        P_MSG_LSTAT_FAILED,
                                        "lstat failed on %s",
                                        path);
                } else {
                        // may be some backend filesytem issue
                        gf_msg (this->name, GF_LOG_ERROR, 0, P_MSG_LSTAT_FAILED,
                                "lstat failed on %s and return value is %d "
                                "instead of -1. Please see dmesg output to "
                                "check whether the failure is due to backend "
                                "filesystem issue", path, ret);
                        ret = -1;
                }
                goto out;
        }

        if ((lstatbuf.st_ino == priv->handledir.st_ino) &&
            (lstatbuf.st_dev == priv->handledir.st_dev)) {
                errno = ENOENT;
                return -1;
        }

        if (!S_ISDIR (lstatbuf.st_mode))
                lstatbuf.st_nlink --;

        iatt_from_stat (&stbuf, &lstatbuf);

        if (gfid && !gf_uuid_is_null (gfid))
                gf_uuid_copy (stbuf.ia_gfid, gfid);
        else
                posix_fill_gfid_from_path (this, path, &stbuf);

        posix_fill_ino_from_gfid (this, &stbuf);

        if (buf_p)
                *buf_p = stbuf;
out:
        return ret;
}

int
posix_fdstat (xlator_t *this, int fd, struct iatt *stbuf_p)
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

        gf_uuid_copy (stbuf.ia_gfid, fd->inode->gfid);

        posix_fill_ino_from_gfid (this, &stbuf);

        if (stbuf_p)
                *stbuf_p = stbuf;

out:
        return ret;
}

static gf_boolean_t
_is_in_array (char **str_array, char *str)
{
        int i = 0;

        if (!str)
                return _gf_false;

        for (i = 0; str_array[i]; i++) {
                if (strcmp (str, str_array[i]) == 0)
                        return _gf_true;
        }
        return _gf_false;
}

static int
_posix_xattr_get_set_from_backend (posix_xattr_filler_t *filler, char *key)
{
        ssize_t  xattr_size = -1;
        int      ret        = 0;
        char    *value      = NULL;

        if (filler->real_path)
                xattr_size = sys_lgetxattr (filler->real_path, key, NULL, 0);
        else
                xattr_size = sys_fgetxattr (filler->fdnum, key, NULL, 0);

        if (xattr_size != -1) {
                value = GF_CALLOC (1, xattr_size + 1, gf_posix_mt_char);
                if (!value)
                        goto out;

                if (filler->real_path)
                        xattr_size = sys_lgetxattr (filler->real_path, key,
                                                    value, xattr_size);
                else
                        xattr_size = sys_fgetxattr (filler->fdnum, key, value,
                                                    xattr_size);
                if (xattr_size == -1) {
                        if (filler->real_path)
                                gf_msg (filler->this->name, GF_LOG_WARNING, 0,
                                        P_MSG_XATTR_FAILED,
                                        "getxattr failed. path: %s, key: %s",
                                        filler->real_path, key);
                        else
                                gf_msg (filler->this->name, GF_LOG_WARNING, 0,
                                        P_MSG_XATTR_FAILED,
                                        "getxattr failed. gfid: %s, key: %s",
                                        uuid_utoa (filler->fd->inode->gfid),
                                        key);
                        GF_FREE (value);
                        goto out;
                }

                value[xattr_size] = '\0';
                ret = dict_set_bin (filler->xattr, key, value, xattr_size);
                if (ret < 0) {
                        if (filler->real_path)
                                gf_msg_debug (filler->this->name, 0,
                                        "dict set failed. path: %s, key: %s",
                                        filler->real_path, key);
                        else
                                gf_msg_debug (filler->this->name, 0,
                                        "dict set failed. gfid: %s, key: %s",
                                        uuid_utoa (filler->fd->inode->gfid),
                                        key);
                        GF_FREE (value);
                        goto out;
                }
        }
        ret = 0;
out:
        return ret;
}

static void
_handle_list_xattr (dict_t *xattr_req, const char *real_path, int fdnum,
                    posix_xattr_filler_t *filler)
{
        ssize_t               size                  = 0;
        char                 *list                  = NULL;
        int32_t               list_offset           = 0;
        ssize_t               remaining_size        = 0;
        char                  *key                  = NULL;

        if ((!real_path) && (fdnum < 0))
                goto out;

        if (real_path)
                size = sys_llistxattr (real_path, NULL, 0);
        else
                size = sys_flistxattr (fdnum, NULL, 0);

        if (size <= 0)
                goto out;

        list = alloca (size);
        if (!list)
                goto out;

        if (real_path)
                remaining_size = sys_llistxattr (real_path, list, size);
        else
                remaining_size = sys_flistxattr (fdnum, list, size);

        if (remaining_size <= 0)
                goto out;

        list_offset = 0;
        while (remaining_size > 0) {
                key = list + list_offset;

                if (_is_in_array (list_xattr_ignore_xattrs, key))
                        goto next;

                if (posix_special_xattr (marker_xattrs, key))
                        goto next;

                if (dict_get (filler->xattr, key))
                        goto next;

                _posix_xattr_get_set_from_backend (filler, key);
next:
                remaining_size -= strlen (key) + 1;
                list_offset += strlen (key) + 1;

        } /* while (remaining_size > 0) */
out:
        return;
}

static gf_boolean_t
posix_xattr_ignorable (char *key)
{
        return _is_in_array (posix_ignore_xattrs, key);
}

static inode_t *
_get_filler_inode (posix_xattr_filler_t *filler)
{
        if (filler->fd)
                return filler->fd->inode;
        else if (filler->loc && filler->loc->inode)
                return filler->loc->inode;
        else
                return NULL;
}

static int
_posix_filler_get_openfd_count (posix_xattr_filler_t *filler, char *key)
{
        inode_t  *inode            = NULL;
        int      ret               = -1;

        inode = _get_filler_inode (filler);
        if (!inode || gf_uuid_is_null (inode->gfid))
                        goto out;

        ret = dict_set_uint32 (filler->xattr, key, inode->fd_count);
        if (ret < 0) {
                gf_msg (filler->this->name, GF_LOG_WARNING, 0,
                        P_MSG_DICT_SET_FAILED,
                        "Failed to set dictionary value for %s", key);
                goto out;
        }
out:
        return ret;
}

static int
_posix_xattr_get_set (dict_t *xattr_req, char *key, data_t *data,
                      void *xattrargs)
{
        posix_xattr_filler_t *filler = xattrargs;
        int       ret      = -1;
        char     *databuf  = NULL;
        int       _fd      = -1;
        loc_t    *loc      = NULL;
        ssize_t  req_size  = 0;


        if (posix_xattr_ignorable (key))
                goto out;
        /* should size be put into the data_t ? */
        if (!strcmp (key, GF_CONTENT_KEY)
            && IA_ISREG (filler->stbuf->ia_type)) {
                if (!filler->real_path)
                        goto out;

                /* file content request */
                req_size = data_to_uint64 (data);
                if (req_size >= filler->stbuf->ia_size) {
                        _fd = open (filler->real_path, O_RDONLY);
                        if (_fd == -1) {
                                gf_msg (filler->this->name, GF_LOG_ERROR, errno,
                                        P_MSG_XDATA_GETXATTR,
                                        "Opening file %s failed",
                                        filler->real_path);
                                goto err;
                        }

                        /*
                         * There could be a situation where the ia_size is
                         * zero. GF_CALLOC will return a pointer to the
                         * memory initialized by gf_mem_set_acct_info.
                         * This function adds a header and a footer to
                         * the allocated memory.  The returned pointer
                         * points to the memory just after the header, but
                         * when size is zero, there is no space for user
                         * data. The memory can be freed by calling GF_FREE.
                         */
                        databuf = GF_CALLOC (1, filler->stbuf->ia_size,
                                             gf_posix_mt_char);
                        if (!databuf) {
                                goto err;
                        }

                        ret = read (_fd, databuf, filler->stbuf->ia_size);
                        if (ret == -1) {
                                gf_msg (filler->this->name, GF_LOG_ERROR, errno,
                                         P_MSG_XDATA_GETXATTR,
                                        "Read on file %s failed",
                                        filler->real_path);
                                goto err;
                        }

                        ret = close (_fd);
                        _fd = -1;
                        if (ret == -1) {
                                gf_msg (filler->this->name, GF_LOG_ERROR, errno,
                                        P_MSG_XDATA_GETXATTR,
                                        "Close on file %s failed",
                                        filler->real_path);
                                goto err;
                        }

                        ret = dict_set_bin (filler->xattr, key,
                                            databuf, filler->stbuf->ia_size);
                        if (ret < 0) {
                                gf_msg (filler->this->name, GF_LOG_ERROR, 0,
                                        P_MSG_XDATA_GETXATTR,
                                        "failed to set dict value. key: %s,"
                                        "path: %s",
                                        key, filler->real_path);
                                goto err;
                        }

                        /* To avoid double free in cleanup below */
                        databuf = NULL;
                err:
                        if (_fd != -1)
                                close (_fd);
                        GF_FREE (databuf);
                }
        } else if (!strcmp (key, GLUSTERFS_OPEN_FD_COUNT)) {
                ret = _posix_filler_get_openfd_count (filler, key);
                loc = filler->loc;
                if (loc) {
                        ret = dict_set_uint32 (filler->xattr, key,
                                               loc->inode->fd_count);
                        if (ret < 0)
                                gf_msg (filler->this->name, GF_LOG_WARNING, 0,
                                        P_MSG_XDATA_GETXATTR,
                                        "Failed to set dictionary value for %s",
                                        key);
                }
        } else {
                ret = _posix_xattr_get_set_from_backend (filler, key);
        }
out:
        return 0;
}

dict_t *
posix_xattr_fill (xlator_t *this, const char *real_path, loc_t *loc, fd_t *fd,
                  int fdnum, dict_t *xattr_req, struct iatt *buf)
{
        dict_t     *xattr             = NULL;
        posix_xattr_filler_t filler   = {0, };
        gf_boolean_t    list          = _gf_false;

        if (dict_get (xattr_req, "list-xattr")) {
                dict_del (xattr_req, "list-xattr");
                list = _gf_true;
        }

        xattr = dict_new ();
        if (!xattr) {
                goto out;
        }

        filler.this      = this;
        filler.real_path = real_path;
        filler.xattr     = xattr;
        filler.stbuf     = buf;
        filler.loc       = loc;
        filler.fd        = fd;
        filler.fdnum    = fdnum;

        dict_foreach (xattr_req, _posix_xattr_get_set, &filler);
        if (list)
                _handle_list_xattr (xattr_req, real_path, fdnum, &filler);

out:
        return xattr;
}

static struct posix_fd *
janitor_get_next_fd (xlator_t *this)
{
        struct posix_private *priv = NULL;
        struct posix_fd *pfd = NULL;

        struct timespec timeout;

        priv = this->private;

        pthread_mutex_lock (&priv->janitor_lock);
        {
                if (list_empty (&priv->janitor_fds)) {
                        time (&timeout.tv_sec);
                        timeout.tv_sec += priv->janitor_sleep_duration;
                        timeout.tv_nsec = 0;

                        pthread_cond_timedwait (&priv->janitor_cond,
                                                &priv->janitor_lock,
                                                &timeout);
                        goto unlock;
                }

                pfd = list_entry (priv->janitor_fds.next, struct posix_fd,
                                  list);

                list_del (priv->janitor_fds.next);
        }
unlock:
        pthread_mutex_unlock (&priv->janitor_lock);

        return pfd;
}


static void *
posix_janitor_thread_proc (void *data)
{
        xlator_t *            this = NULL;
        struct posix_fd *pfd;

        this = data;

        THIS = this;

        while (1) {
                pfd = janitor_get_next_fd (this);
                if (pfd) {
                        if (pfd->dir == NULL) {
                                gf_msg_trace (this->name, 0,
                                        "janitor: closing file fd=%d", pfd->fd);
                                close (pfd->fd);
                        } else {
                                gf_msg_debug (this->name, 0, "janitor: closing"
                                              " dir fd=%p", pfd->dir);
                                closedir (pfd->dir);
                        }

                        GF_FREE (pfd);
                }
        }

        return NULL;
}


void
posix_spawn_janitor_thread (xlator_t *this)
{
        struct posix_private *priv = NULL;
        int ret = 0;

        priv = this->private;

        LOCK (&priv->lock);
        {
                if (!priv->janitor_present) {
                        ret = gf_thread_create (&priv->janitor, NULL,
                                                posix_janitor_thread_proc, this);

                        if (ret < 0) {
                                gf_msg (this->name, GF_LOG_ERROR, errno,
                                        P_MSG_THREAD_FAILED, "spawning janitor "
                                        "thread failed");
                                goto unlock;
                        }

                        priv->janitor_present = _gf_true;
                }
        }
unlock:
        UNLOCK (&priv->lock);
}

int
posix_fsyncer_pick (xlator_t *this, struct list_head *head)
{
        struct posix_private *priv = NULL;
        int count = 0;

        priv = this->private;
        pthread_mutex_lock (&priv->fsync_mutex);
        {
                while (list_empty (&priv->fsyncs))
                        pthread_cond_wait (&priv->fsync_cond,
                                           &priv->fsync_mutex);

                count = priv->fsync_queue_count;
                priv->fsync_queue_count = 0;
                list_splice_init (&priv->fsyncs, head);
        }
        pthread_mutex_unlock (&priv->fsync_mutex);

        return count;
}


void
posix_fsyncer_process (xlator_t *this, call_stub_t *stub, gf_boolean_t do_fsync)
{
        struct posix_fd *pfd = NULL;
        int ret = -1;

        ret = posix_fd_ctx_get (stub->args.fd, this, &pfd);
        if (ret < 0) {
                gf_msg (this->name, GF_LOG_ERROR, EINVAL,
                        P_MSG_GET_FDCTX_FAILED,
                        "could not get fdctx for fd(%s)",
                        uuid_utoa (stub->args.fd->inode->gfid));
                call_unwind_error (stub, -1, EINVAL);
                return;
        }

        if (do_fsync) {
                if (stub->args.datasync)
                        ret = sys_fdatasync (pfd->fd);
                else
                        ret = sys_fsync (pfd->fd);
        } else {
                ret = 0;
        }

        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno, P_MSG_FSTAT_FAILED,
                        "could not fstat fd(%s)",
                        uuid_utoa (stub->args.fd->inode->gfid));
                call_unwind_error (stub, -1, errno);
                return;
        }

        call_unwind_error (stub, 0, 0);
}


static void
posix_fsyncer_syncfs (xlator_t *this, struct list_head *head)
{
        call_stub_t *stub = NULL;
        struct posix_fd *pfd = NULL;
        int ret = -1;

        stub = list_entry (head->prev, call_stub_t, list);
        ret = posix_fd_ctx_get (stub->args.fd, this, &pfd);
        if (ret)
                return;

#ifdef GF_LINUX_HOST_OS
        /* syncfs() is not "declared" in RHEL's glibc even though
           the kernel has support.
        */
#include <sys/syscall.h>
#include <unistd.h>
#ifdef SYS_syncfs
        syscall (SYS_syncfs, pfd->fd);
#else
        sync();
#endif
#else
        sync();
#endif
}


void *
posix_fsyncer (void *d)
{
        xlator_t *this = d;
        struct posix_private *priv = NULL;
        call_stub_t *stub = NULL;
        call_stub_t *tmp = NULL;
        struct list_head list;
        int count = 0;
        gf_boolean_t do_fsync = _gf_true;

        priv = this->private;

        for (;;) {
                INIT_LIST_HEAD (&list);

                count = posix_fsyncer_pick (this, &list);

                usleep (priv->batch_fsync_delay_usec);

                gf_msg_debug (this->name, 0,
                        "picked %d fsyncs", count);

                switch (priv->batch_fsync_mode) {
                case BATCH_NONE:
                case BATCH_REVERSE_FSYNC:
                        break;
                case BATCH_SYNCFS:
                case BATCH_SYNCFS_SINGLE_FSYNC:
                case BATCH_SYNCFS_REVERSE_FSYNC:
                        posix_fsyncer_syncfs (this, &list);
                        break;
                }

                if (priv->batch_fsync_mode == BATCH_SYNCFS)
                        do_fsync = _gf_false;
                else
                        do_fsync = _gf_true;

                list_for_each_entry_safe_reverse (stub, tmp, &list, list) {
                        list_del_init (&stub->list);

                        posix_fsyncer_process (this, stub, do_fsync);

                        if (priv->batch_fsync_mode == BATCH_SYNCFS_SINGLE_FSYNC)
                                do_fsync = _gf_false;
                }
        }
}
