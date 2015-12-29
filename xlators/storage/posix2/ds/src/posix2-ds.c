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
        int32_t ret = -1;
        struct stat stbuf = {0,};

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

class_methods_t class_methods = {
        .init = posix2_ds_init,
        .fini = posix2_ds_fini,
};

struct xlator_fops fops = {
        .lookup      = posix2_lookup,
        .open        = posix2_open,
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
        {
                .key = {NULL},
        }
};
