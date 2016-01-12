/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

/**
 *     ######   #######   #####   ###  #     #   ###        ######    #####
 *     #     #  #     #  #     #   #    #   #   #   #       #     #  #     #
 *     #     #  #     #  #         #     # #    #   #       #     #  #
 *     ######   #     #   #####    #      #        #        #     #   #####
 *     #        #     #        #   #     # #      #         #     #        #
 *     #        #     #  #     #   #    #   #    #          #     #  #     #
 *     #        #######   #####   ###  #     #  #####       ######    #####
 *
 * This is the data side of things, commonly known as data server
 * or simply DS.
 */

#include "posix2-ds.h"
#include "posix2.h"
#include "posix2-ds-messages.h"
#include "posix2-mem-types.h"
#include "posix2-ds-helpers.h"

#define ALIGN_SIZE 4096
int32_t
mem_acct_init (xlator_t *this)
{
        int     ret = -1;

        GF_VALIDATE_OR_GOTO ("posix2-ds", this, out);

        ret = xlator_mem_acct_init (this, gf_posix2_ds_mt_end + 1);

        if (ret != 0) {
                gf_msg (this->name, GF_LOG_ERROR, errno, 0,
                        "Memory accounting init failed");
                return ret;
        }
 out:
        return ret;
}

static int
posix2_fill_ds (xlator_t *this, struct posix2_ds *ds, const char *export)
{
        int32_t               ret           = -1;
        data_t               *tmp_data      = NULL;
        struct stat           stbuf         = {0,};

        ds->hostname = GF_CALLOC (256, sizeof (char), gf_common_mt_char);
        if (!ds->hostname)
                goto error_return;
        ret = gethostname (ds->hostname, 256);
        if (ret < 0)
                goto dealloc_hostmem;

        ret = stat (export, &stbuf);
        if ((ret != 0) || !S_ISDIR (stbuf.st_mode)) {
                gf_msg (this->name, GF_LOG_ERROR, 0, POSIX2_DS_MSG_EXPORT_NOTDIR,
                        "Export [%s] is %s.", export,
                        (ret == 0) ? "not a directory" : "unusable");
                goto dealloc_hostmem;
        }

        ds->exportdir = gf_strdup (export);
        if (!ds->exportdir)
                goto dealloc_hostmem;

        ds->mountlock = opendir (ds->exportdir);
        if (!ds->mountlock)
                goto dealloc_export;

        LOCK_INIT (&ds->lock);

        ds->export_statfs = 1;
        tmp_data = dict_get (this->options, "export-statfs-size");
        if (tmp_data) {
                if (gf_string2boolean (tmp_data->data,
                                       &ds->export_statfs) == -1) {
                        ret = -1;
                        gf_msg (this->name, GF_LOG_ERROR, 0,
                                POSIX2_DS_MSG_INVALID_OPTION_VAL,
                                "'export-statfs-size' takes only boolean "
                                "options");
                        goto dealloc_export;
                }
                if (!ds->export_statfs)
                        gf_msg_debug (this->name, 0,
                                "'statfs()' returns dummy size");
        }


        return 0;

 dealloc_export:
        GF_FREE (ds->exportdir);
        ds->exportdir = NULL;
 dealloc_hostmem:
        GF_FREE (ds->hostname);
        ds->hostname = NULL;
 error_return:
        return -1;
}

int
posix2_ds_init (xlator_t *this)
{
	int32_t            ret    = 0;
	data_t            *export = NULL;
	struct posix2_ds  *ds    = NULL;

	if (this->children) {
		gf_msg (this->name, GF_LOG_CRITICAL, 0,
                        POSIX2_DS_MSG_SUBVOL_ERR,
			"FATAL: storage/posix2 cannot have subvolumes");
		goto error_return;
	}

	if (!this->parents) {
		gf_msg (this->name, GF_LOG_CRITICAL, 0,
                        POSIX2_DS_MSG_DANGLING_VOL,
			"Volume file is dangling, bailing out..");
		goto error_return;
	}

	export = dict_get (this->options, "directory");
	if (!export) {
		gf_msg (this->name, GF_LOG_CRITICAL,
			0, POSIX2_DS_MSG_EXPORT_MISSING,
			"Export not specified in volume file");
		goto error_return;
	}

	ds = GF_CALLOC (1, sizeof (*ds), gf_posix2_ds_mt_private_t);
	if (!ds)
		goto error_return;
	ret = posix2_fill_ds (this, ds, (const char *)(export->data));
	if (ret)
		goto dealloc_mds;

	this->private = ds;
	return 0;

 dealloc_mds:
	GF_FREE (ds);
 error_return:
	return -1;
}

void
posix2_ds_fini (xlator_t *this)
{
        return;
}

int32_t
posix2_lookup (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xdata)
{
        int32_t             ret      = 0;
        int                 entrylen = 0;
        char               *entry    = NULL;
        char               *export   = NULL;
        struct posix2_ds   *ds       = NULL;
        struct iatt         buf      = {0,};
        uuid_t              tgtuuid  = {0,};

        ds = this->private;
        export = ds->exportdir;

        if (gf_uuid_is_null (loc->gfid)) {
                gf_msg (this->name, GF_LOG_ERROR,
                        0, POSIX2_DS_MSG_NULL_GFID,
                        "loc->gfid is NULL");
                goto unwind_err;
        }

        gf_uuid_copy (tgtuuid, loc->gfid);

        if (!posix2_lookup_is_nameless (loc)) {
                gf_msg (this->name, GF_LOG_ERROR,
                        0, POSIX2_DS_MSG_NAMED_LOOKUP,
                        "DS server got a named lookup!");
                goto unwind_err;
        }

        entrylen = posix2_handle_length (export);
        entry = alloca (entrylen);

        errno = EINVAL;
        entrylen = posix2_make_handle (this, export, tgtuuid, entry, entrylen);
        if (entrylen <= 0)
                goto unwind_err;

        ret = posix2_ds_resolve_inodeptr
                       (this, tgtuuid, entry, &buf);
        if (ret < 0) {
                if (errno != ENOENT)
                        goto unwind_err;
                if (errno == ENOENT) {
                        ret = posix2_create_inode (this, entry, 0, 0600);
                        if (ret)
                                goto unwind_err;
                        else {
                                ret = posix2_ds_resolve_inodeptr
                                               (this, tgtuuid, entry, &buf);
                                if (ret)
                                        goto unwind_err;
                        }
                }
        }
        STACK_UNWIND_STRICT (lookup, frame, 0, 0, loc->inode, &buf, NULL, NULL);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (lookup, frame, -1,
                             EINVAL, NULL, NULL, NULL, NULL);
        return 0;
}

int32_t
posix2_open (call_frame_t *frame, xlator_t *this,
            loc_t *loc, int32_t flags, fd_t *fd, dict_t *xdata)
{
        int32_t               ret          = -1;
        int32_t               op_ret       = -1;
        int32_t               op_errno     = 0;
        int                   entrylen     = 0;
        char                 *entry        = NULL;
        char                 *export       = NULL;
        int32_t               _fd          = -1;
        struct posix2_fd     *pfd          = NULL;
        struct posix2_ds     *priv         = NULL;
        struct iatt           stbuf        = {0, };
        uuid_t                tgtuuid      = {0, };

        /* DECLARE_OLD_FS_ID_VAR; */

        VALIDATE_OR_GOTO (frame, out);
        VALIDATE_OR_GOTO (this, out);
        VALIDATE_OR_GOTO (this->private, out);
        VALIDATE_OR_GOTO (loc, out);
        VALIDATE_OR_GOTO (fd, out);

        priv = this->private;
        export = priv->exportdir;

        if (gf_uuid_is_null (loc->gfid)) {
                gf_msg (this->name, GF_LOG_ERROR,
                        0, POSIX2_DS_MSG_NULL_GFID,
                        "loc->gfid is NULL");
                goto out;
        }
        gf_uuid_copy (tgtuuid, loc->gfid);

        entrylen = posix2_handle_length (export);
        entry = alloca (entrylen);

        errno = EINVAL;
        entrylen = posix2_make_handle (this, export, tgtuuid, entry, entrylen);
        if (entrylen <= 0) {
                op_ret = -1;
                op_errno = errno;
                goto out;
        }

        ret = posix2_ds_resolve_inodeptr
                       (this, tgtuuid, entry, &stbuf);

        if (ret < 0) {
                op_ret = -1;
                op_errno = ESTALE; /*TODO ESTALE or EINVAL */
                goto out;
        }

        if (IA_ISLNK (stbuf.ia_type)) {
                op_ret = -1;
                op_errno = ELOOP;
                goto out;
        }

        op_ret = -1;

        /* SET_FS_ID (frame->root->uid, frame->root->gid);

        if (priv->o_direct)
                flags |= O_DIRECT;

         */

        _fd = open (entry, flags, 0);
        if (_fd == -1) {
                op_ret   = -1;
                op_errno = errno;
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        POSIX2_DS_MSG_FILE_OP_FAILED,
                        "open on %s, flags: %d", entry, flags);
                goto out;
        }

        pfd = GF_CALLOC (1, sizeof (*pfd), gf_posix2_ds_mt_posix2_fd_t);
        if (!pfd) {
                op_errno = errno;
                goto out;
        }

        pfd->flags = flags;
        pfd->fd    = _fd;

        op_ret = fd_ctx_set (fd, this, (uint64_t)(long)pfd);
        if (op_ret)
                gf_msg (this->name, GF_LOG_WARNING, 0,
                        POSIX2_DS_MSG_FD_PATH_SETTING_FAILED,
                        "failed to set the fd context path=%s fd=%p",
                        entry, fd);

        /* TODO: Add later
        LOCK (&priv->lock);
        {
                priv->nr_files++;
        }
        UNLOCK (&priv->lock);
        */

        op_ret = 0;

out:
        if (op_ret == -1) {
                if (_fd != -1) {
                        close (_fd);
                }
        }

        /* SET_TO_OLD_FS_ID (); */

        STACK_UNWIND_STRICT (open, frame, op_ret, op_errno, fd, NULL);

        return 0;
}

char*
_page_aligned_alloc (size_t size, char **aligned_buf)
{
        char            *alloc_buf = NULL;
        char            *buf = NULL;

        alloc_buf = GF_CALLOC (1, (size + ALIGN_SIZE), gf_posix2_ds_mt_char);
        if (!alloc_buf)
                goto out;
        /* page aligned buffer */
        buf = GF_ALIGN_BUF (alloc_buf, ALIGN_SIZE);
        *aligned_buf = buf;
out:
        return alloc_buf;
}

int32_t
__posix_pwritev (int fd, struct iovec *vector, int count, off_t offset)
{
        int32_t         op_ret = 0;
        int             idx = 0;
        int             retval = 0;
        off_t           internal_off = 0;

        if (!vector)
                return -EFAULT;

        internal_off = offset;
        for (idx = 0; idx < count; idx++) {
                retval = pwrite (fd, vector[idx].iov_base, vector[idx].iov_len,
                                 internal_off);
                if (retval == -1) {
                        op_ret = -errno;
                        goto err;
                }
                op_ret += retval;
                internal_off += retval;
        }

err:
        return op_ret;
}

int32_t
__posix_writev (int fd, struct iovec *vector, int count, off_t startoff,
                int odirect)
{
        int32_t         op_ret = 0;
        int             idx = 0;
        int             max_buf_size = 0;
        int             retval = 0;
        char            *buf = NULL;
        char            *alloc_buf = NULL;
        off_t           internal_off = 0;

        /* Check for the O_DIRECT flag during open() */
        if (!odirect)
                return __posix_pwritev (fd, vector, count, startoff);

        for (idx = 0; idx < count; idx++) {
                if (max_buf_size < vector[idx].iov_len)
                        max_buf_size = vector[idx].iov_len;
        }

        alloc_buf = _page_aligned_alloc (max_buf_size, &buf);
        if (!alloc_buf) {
                op_ret = -errno;
                goto err;
        }

        internal_off = startoff;
        for (idx = 0; idx < count; idx++) {
                memcpy (buf, vector[idx].iov_base, vector[idx].iov_len);

                /* not sure whether writev works on O_DIRECT'd fd */
                retval = pwrite (fd, buf, vector[idx].iov_len, internal_off);
                if (retval == -1) {
                        op_ret = -errno;
                        goto err;
                }

                op_ret += retval;
                internal_off += retval;
        }

err:
        GF_FREE (alloc_buf);

        return op_ret;
}

dict_t*
_fill_writev_xdata (fd_t *fd, dict_t *xdata, xlator_t *this, int is_append)
{
        dict_t  *rsp_xdata = NULL;
        int32_t ret = 0;
        inode_t *inode = NULL;

        if (fd)
                inode = fd->inode;

        if (!fd || !fd->inode || gf_uuid_is_null (fd->inode->gfid)) {
                gf_log_callingfn (this->name, GF_LOG_ERROR, "Invalid Args: "
                                  "fd: %p inode: %p gfid:%s", fd, inode?inode:0,
                                  inode?uuid_utoa(inode->gfid):"N/A");
                goto out;
        }

        if (!xdata || !dict_get (xdata, GLUSTERFS_OPEN_FD_COUNT))
                goto out;

        rsp_xdata = dict_new();
        if (!rsp_xdata)
                goto out;

        ret = dict_set_uint32 (rsp_xdata, GLUSTERFS_OPEN_FD_COUNT,
                               fd->inode->fd_count);
        if (ret < 0) {
                gf_msg (this->name, GF_LOG_WARNING, 0, POSIX2_DS_MSG_DICT_SET_FAILED,
                        "%s: Failed to set dictionary value for %s",
                        uuid_utoa (fd->inode->gfid), GLUSTERFS_OPEN_FD_COUNT);
        }

        ret = dict_set_uint32 (rsp_xdata, GLUSTERFS_WRITE_IS_APPEND,
                               is_append);
        if (ret < 0) {
                gf_msg (this->name, GF_LOG_WARNING, 0, POSIX2_DS_MSG_DICT_SET_FAILED,
                        "%s: Failed to set dictionary value for %s",
                        uuid_utoa (fd->inode->gfid),
                        GLUSTERFS_WRITE_IS_APPEND);
        }
out:
        return rsp_xdata;
}

int32_t
posix2_writev (call_frame_t *frame, xlator_t *this, fd_t *fd,
              struct iovec *vector, int32_t count, off_t offset,
              uint32_t flags, struct iobref *iobref, dict_t *xdata)
{
        int32_t                    op_ret           = -1;
        int32_t                    op_errno         = 0;
        int                        _fd              = -1;
        struct posix2_ds          *priv             = NULL;
        struct posix2_fd          *pfd              = NULL;
        struct iatt                preop            = {0,};
        struct iatt                postop           = {0,};
        int                        ret              = -1;
        dict_t                    *rsp_xdata        = NULL;
        int                        is_append        = 0;
        gf_boolean_t               locked           = _gf_false;
        gf_boolean_t               write_append     = _gf_false;
        gf_boolean_t               update_atomic    = _gf_false;

        VALIDATE_OR_GOTO (frame, out);
        VALIDATE_OR_GOTO (this, out);
        VALIDATE_OR_GOTO (fd, out);
        VALIDATE_OR_GOTO (vector, out);
        VALIDATE_OR_GOTO (this->private, out);

        priv = this->private;

        VALIDATE_OR_GOTO (priv, out);

        ret = posix2_fd_ctx_get (fd, this, &pfd);
        if (ret < 0) {
                gf_msg (this->name, GF_LOG_WARNING, ret, POSIX2_DS_MSG_PFD_NULL,
                        "pfd is NULL from fd=%p", fd);
                op_errno = -ret;
                goto out;
        }

        _fd = pfd->fd;

        if (xdata) {
                if (dict_get (xdata, GLUSTERFS_WRITE_IS_APPEND))
                        write_append = _gf_true;
                if (dict_get (xdata, GLUSTERFS_WRITE_UPDATE_ATOMIC))
                        update_atomic = _gf_true;
        }

        /* The write_is_append check and write must happen
           atomically. Else another write can overtake this
           write after the check and get written earlier.

           So lock before preop-stat and unlock after write.
        */

        /*
         * The update_atomic option is to instruct posix to do prestat,
         * write and poststat atomically. This is to prevent any modification to
         * ia_size and ia_blocks until poststat and the diff in their values
         * between pre and poststat could be of use for some translators (shard
         * as of today).
         */

        if (write_append || update_atomic) {
                locked = _gf_true;
                LOCK(&fd->inode->lock);
        }

        op_ret = posix2_fdstat (this, _fd, &preop);
        if (op_ret == -1) {
                op_errno = errno;
                gf_msg (this->name, GF_LOG_ERROR, errno, POSIX2_DS_MSG_FSTAT_FAILED,
                        "pre-operation fstat failed on fd=%p", fd);
                goto out;
        }

        if (locked && write_append) {
                if (preop.ia_size == offset || (fd->flags & O_APPEND))
                        is_append = 1;
        }

        op_ret = __posix_writev (_fd, vector, count, offset,
                                 (pfd->flags & O_DIRECT));

        if (locked && (!update_atomic)) {
                UNLOCK (&fd->inode->lock);
                locked = _gf_false;
        }

        if (op_ret < 0) {
                op_errno = -op_ret;
                op_ret = -1;
                gf_msg (this->name, GF_LOG_ERROR, op_errno,
                        POSIX2_DS_MSG_WRITEV_FAILED,
                        "write failed: offset %"PRIu64
                        ",", offset);
                goto out;
        }

        rsp_xdata = _fill_writev_xdata (fd, xdata, this, is_append);
        /* writev successful, we also need to get the stat of
         * the file we wrote to
         */

        ret = posix2_fdstat (this, _fd, &postop);
        if (ret == -1) {
                op_ret = -1;
                op_errno = errno;
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        POSIX2_DS_MSG_FSTAT_FAILED,
                        "post-operation fstat failed on fd=%p",
                        fd);
                goto out;
        }

        if (locked) {
                UNLOCK (&fd->inode->lock);
                locked = _gf_false;
        }

        if (flags & (O_SYNC|O_DSYNC)) {
                ret = fsync (_fd);
                if (ret) {
                        gf_msg (this->name, GF_LOG_ERROR, errno,
                                POSIX2_DS_MSG_WRITEV_FAILED,
                                "fsync() in writev on fd %d failed",
                                _fd);
                        op_ret = -1;
                        op_errno = errno;
                        goto out;
                }
        }

        LOCK (&(priv->lock));
        {
                priv->write_value    += op_ret;
        }
        UNLOCK (&(priv->lock));

out:

        if (locked) {
                UNLOCK (&fd->inode->lock);
                locked = _gf_false;
        }

        STACK_UNWIND_STRICT (writev, frame, op_ret, op_errno, &preop, &postop,
                             rsp_xdata);

        if (rsp_xdata)
                dict_unref (rsp_xdata);
        return 0;
}

int32_t
posix2_readv (call_frame_t *frame, xlator_t *this,
             fd_t *fd, size_t size, off_t offset, uint32_t flags, dict_t *xdata)
{
        int32_t                op_ret     = -1;
        int32_t                op_errno   = 0;
        int                    _fd        = -1;
        struct iobuf          *iobuf      = NULL;
        struct iobref         *iobref     = NULL;
        struct iovec           vec        = {0,};
        struct posix2_fd *     pfd        = NULL;
        struct iatt            stbuf      = {0,};
        int                    ret        = -1;

        VALIDATE_OR_GOTO (frame, out);
        VALIDATE_OR_GOTO (this, out);
        VALIDATE_OR_GOTO (fd, out);

        ret = posix2_fd_ctx_get (fd, this, &pfd);
        if (ret < 0) {
                op_errno = -ret;
                gf_msg (this->name, GF_LOG_WARNING, op_errno,
                        POSIX2_DS_MSG_PFD_NULL,
                        "pfd is NULL from fd=%p", fd);
                goto out;
        }

        if (!size) {
                op_errno = EINVAL;
                gf_msg (this->name, GF_LOG_WARNING, EINVAL,
                        POSIX2_DS_MSG_INVALID_ARGUMENT,
                        "size=%"GF_PRI_SIZET, size);
                goto out;
        }

        iobuf = iobuf_get2 (this->ctx->iobuf_pool, size);
        if (!iobuf) {
                op_errno = ENOMEM;
                goto out;
        }

        _fd = pfd->fd;
        op_ret = pread (_fd, iobuf->ptr, size, offset);
        if (op_ret == -1) {
                op_errno = errno;
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        POSIX2_DS_MSG_READ_FAILED,
                        "read failed on fd=%p", fd);
                goto out;
        }

        /*
        LOCK (&priv->lock);
        {
                priv->read_value    += op_ret;
        }
        UNLOCK (&priv->lock);
        */

        vec.iov_base = iobuf->ptr;
        vec.iov_len  = op_ret;

        iobref = iobref_new ();

        iobref_add (iobref, iobuf);

        /*
         *  readv successful, and we need to get the stat of the file
         *  we read from
         */

        op_ret = posix2_fdstat (this, _fd, &stbuf);
        if (op_ret == -1) {
                op_errno = errno;
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        POSIX2_DS_MSG_FSTAT_FAILED,
                        "fstat failed on fd=%p", fd);
                goto out;
        }

        /* Hack to notify higher layers of EOF. */
        if (!stbuf.ia_size || (offset + vec.iov_len) >= stbuf.ia_size)
                op_errno = ENOENT;

        op_ret = vec.iov_len;
out:

        STACK_UNWIND_STRICT (readv, frame, op_ret, op_errno,
                             &vec, 1, &stbuf, iobref, NULL);

        if (iobref)
                iobref_unref (iobref);
        if (iobuf)
                iobuf_unref (iobuf);

        return 0;
}

int32_t
posix2_stat (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xdata)
{
        int32_t ret = 0;
        int entrylen = 0;
        char *entry = NULL;
        struct iatt buf = {0,};
        struct posix2_ds *ds = NULL;

        ds = this->private;

        entrylen = posix2_handle_length (ds->exportdir);
        entry = alloca (entrylen);

        errno = EINVAL;
        entrylen = posix2_make_handle (this, ds->exportdir,
                                        loc->gfid, entry, entrylen);
        if (entrylen <= 0)
                goto unwind_err;

        ret = posix2_ds_resolve_inodeptr
                       (this, loc->gfid, entry, &buf);
        if (ret)
                goto unwind_err;

        STACK_UNWIND_STRICT (stat, frame, 0, 0, &buf, NULL);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (stat, frame, -1, errno, NULL, NULL);
        return 0;
}

int32_t
posix2_flush (call_frame_t *frame, xlator_t *this, fd_t *fd, dict_t *dict)
{
        int32_t                    op_ret           = -1;
        int32_t                    op_errno         = 0;
        struct posix2_fd          *pfd              = NULL;
        int                        ret              = -1;

        VALIDATE_OR_GOTO (frame, out);
        VALIDATE_OR_GOTO (this, out);
        VALIDATE_OR_GOTO (fd, out);

        ret = posix2_fd_ctx_get (fd, this, &pfd);
        if (ret < 0) {
                op_errno = -ret;
                gf_msg (this->name, GF_LOG_WARNING, ret, POSIX2_DS_MSG_PFD_NULL,
                        "pfd is NULL from fd=%p", fd);
                goto out;
        }
        op_ret = 0;

out:
        STACK_UNWIND_STRICT (flush, frame, op_ret, op_errno, NULL);
        return 0;
}

int32_t
posix2_fsync (call_frame_t *frame, xlator_t *this,
             fd_t *fd, int32_t datasync, dict_t *xdata)
{
        int32_t           op_ret   = -1;
        int32_t           op_errno = 0;
        int               _fd      = -1;
        struct posix2_fd *pfd      = NULL;
        int               ret      = -1;
        struct iatt       preop    = {0,};
        struct iatt       postop   = {0,};

        VALIDATE_OR_GOTO (frame, out);
        VALIDATE_OR_GOTO (this, out);
        VALIDATE_OR_GOTO (fd, out);

        ret = posix2_fd_ctx_get (fd, this, &pfd);
        if (ret < 0) {
                gf_msg (this->name, GF_LOG_WARNING, ret, POSIX2_DS_MSG_PFD_NULL,
                        "pfd is NULL from fd=%p", fd);
                op_errno = -ret;
                goto out;
        }

        _fd = pfd->fd;

        op_ret = posix2_fdstat (this, _fd, &preop);
        if (op_ret == -1) {
                op_errno = errno;
                gf_msg (this->name, GF_LOG_ERROR, errno, POSIX2_DS_MSG_FSTAT_FAILED,
                        "pre-operation fstat failed on fd=%p", fd);
                goto out;
        }

        if (datasync) {
                op_ret = sys_fdatasync (_fd);
                if (op_ret == -1) {
                        op_errno = errno;
                        gf_msg (this->name, GF_LOG_ERROR, errno,
                                POSIX2_DS_MSG_FSYNC_FAILED, "fdatasync on fd=%p"
                                "failed:", fd);
                        goto out;
                }
        } else {
                op_ret = sys_fsync (_fd);
                if (op_ret == -1) {
                        op_errno = errno;
                        gf_msg (this->name, GF_LOG_ERROR, errno,
                                POSIX2_DS_MSG_FSYNC_FAILED, "fsync on fd=%p "
                                "failed", fd);
                        goto out;
                }
        }

        op_ret = posix2_fdstat (this, _fd, &postop);
        if (op_ret == -1) {
                op_errno = errno;
                gf_msg (this->name, GF_LOG_ERROR, errno, POSIX2_DS_MSG_FSTAT_FAILED,
                        "pre-operation fstat failed on fd=%p", fd);
                goto out;
        }

        op_ret = 0;

out:
        STACK_UNWIND_STRICT (fsync, frame, op_ret, op_errno, &preop, &postop,
                             NULL);
        return 0;
}

int32_t
posix2_statfs (call_frame_t *frame, xlator_t *this,
              loc_t *loc, dict_t *xdata)
{
        int32_t                ret       = -1;
        int32_t                op_ret    = -1;
        int32_t                op_errno  = 0;
        int                    entrylen  = 0;
        char                  *entry     = NULL;
        char                  *export    = NULL;
        struct statvfs         buf       = {0, };
        struct iatt            stbuf     = {0, };
        struct posix2_ds      *priv      = NULL;
        uuid_t                 tgtuuid   = {0, };

        VALIDATE_OR_GOTO (frame, out);
        VALIDATE_OR_GOTO (this, out);
        VALIDATE_OR_GOTO (loc, out);
        VALIDATE_OR_GOTO (this->private, out);

        priv = this->private;
        export = priv->exportdir;

        if (gf_uuid_is_null (loc->gfid)) {
                gf_msg (this->name, GF_LOG_ERROR,
                        0, POSIX2_DS_MSG_NULL_GFID,
                        "loc->gfid is NULL");
                goto out;
        }
        gf_uuid_copy (tgtuuid, loc->gfid);

        entrylen = posix2_handle_length (export);
        entry = alloca (entrylen);

        errno = EINVAL;
        entrylen = posix2_make_handle (this, export, tgtuuid, entry, entrylen);
        if (entrylen <= 0) {
                op_ret = -1;
                op_errno = errno;
                goto out;
        }

        ret = posix2_ds_resolve_inodeptr
                       (this, tgtuuid, entry, &stbuf);

        if (ret < 0) {
                op_ret = -1;
                op_errno = ESTALE; /*TODO ESTALE or EINVAL */
                goto out;
        }

        op_ret = statvfs (entry, &buf);

        if (op_ret == -1) {
                op_errno = errno;
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        POSIX2_DS_MSG_STATVFS_FAILED,
                        "statvfs failed on %s", entry);
                goto out;
        }

        if (!priv->export_statfs) {
                buf.f_blocks = 0;
                buf.f_bfree  = 0;
                buf.f_bavail = 0;
                buf.f_files  = 0;
                buf.f_ffree  = 0;
                buf.f_favail = 0;
        }

        op_ret = 0;

out:
        STACK_UNWIND_STRICT (statfs, frame, op_ret, op_errno, &buf, NULL);
        return 0;
}

int32_t
posix2_fsetxattr (call_frame_t *frame, xlator_t *this,
                 fd_t *fd, dict_t *dict, int flags, dict_t *xdata)
{
        int32_t            op_ret         = 0;
        int32_t            op_errno       = 0;
        dict_t            *xattr          = NULL;

        gf_msg (this->name, GF_LOG_CRITICAL, 0, POSIX2_DS_MSG_GOT_MDS_OP,
		"FATAL: storage/posix2_ds: Got MDS op: fsetxattr");
        STACK_UNWIND_STRICT (fsetxattr, frame, op_ret, op_errno, xattr);
        return 0;
}

int32_t
posix2_setxattr (call_frame_t *frame, xlator_t *this,
                loc_t *loc, dict_t *dict, int flags, dict_t *xdata)
{
        int32_t       op_ret                  = 0;
        int32_t       op_errno                = 0;
        dict_t       *xattr                   = NULL;

        gf_msg (this->name, GF_LOG_CRITICAL, 0, POSIX2_DS_MSG_GOT_MDS_OP,
		"FATAL: storage/posix2_ds: Got MDS op: setxattr");
        STACK_UNWIND_STRICT (setxattr, frame, op_ret, op_errno, xattr);
        return 0;
}

class_methods_t class_methods = {
        .init = posix2_ds_init,
        .fini = posix2_ds_fini,
};

struct xlator_fops fops = {
        .lookup = posix2_lookup,
        .open   = posix2_open,

        .writev      = posix2_writev,
        .readv       = posix2_readv,

        .stat        = posix2_stat,
        .statfs      = posix2_statfs,
        .flush       = posix2_flush,
        .fsync       = posix2_fsync,
        .setxattr    = posix2_setxattr,
        .fsetxattr   = posix2_fsetxattr,
};

struct xlator_cbks cbks = {
};

struct volume_options options[] = {
        {
                .key = {"export"},
                .type = GF_OPTION_TYPE_PATH,
        },
        {
                .key = {"volume-id"},
                .type = GF_OPTION_TYPE_ANY,
        },
        {       .key  = {"export-statfs-size"},
                .type = GF_OPTION_TYPE_BOOL,
        },
        {
                .key = {NULL},
        }
};
