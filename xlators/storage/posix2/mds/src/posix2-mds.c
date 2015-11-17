/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

/**
 *     ######   #######   #####   ###  #     #   ###
 *     #     #  #     #  #     #   #    #   #   #   #
 *     #     #  #     #  #         #     # #    #   #
 *     ######   #     #   #####    #      #        #
 *     #        #     #        #   #     # #      #
 *     #        #     #  #     #   #    #   #    #
 *     #        #######   #####   ###  #     #  #####
 *
 * This is the metadata side of things, commonly known as metadata server
 * or simply MDS. Variable names used here are _same_ as what's used in the
 * functional spec, therefore, one can read the spec and code side-by-side
 * and.. laugh :)
 */

#include "posix2-mds.h"
#include "posix2-messages.h"
#include "posix2-mem-types.h"

#include "zfstore.h"

int32_t
mem_acct_init (xlator_t *this)
{
        int     ret = -1;

        GF_VALIDATE_OR_GOTO ("posix2", this, out);

        ret = xlator_mem_acct_init (this, gf_posix2_mt_end + 1);

        if (ret != 0) {
                gf_msg (this->name, GF_LOG_ERROR, errno, 0,
                        "Memory accounting init failed");
                return ret;
        }
 out:
        return ret;
}

static struct posix2_mdstore_handler posix2_mdslist[] = {
        {
                .ctor     = zfstore_ctor,
                .dtor     = zfstore_dtor,
                .storeops = &zfstore_fops,
        },
};

static struct posix2_mdstore_handler *
posix2_find_mds_handler (xlator_t *this)
{
        return &posix2_mdslist[0];
}

static int
posix2_fill_mds (xlator_t *this, struct posix2_mds *mds, const char *export)
{
        int32_t ret = -1;
        void *store = NULL;
        struct posix2_mdstore_handler *handler = NULL;

        mds->hostname = GF_CALLOC (256, sizeof (char), gf_common_mt_char);
        if (!mds->hostname)
                goto error_return;
        ret = gethostname (mds->hostname, 256);
        if (ret < 0)
                goto dealloc_hostmem;

        /* find the appropriate MDS handler */
        handler = posix2_find_mds_handler (this);
        if (!handler)
                goto dealloc_hostmem;
        /* don't tolerate nuisance */
        if (!handler->storeops)
                goto dealloc_hostmem;

        /* Invoke constructor and set store */
        store = handler->ctor (this, export);
        if (!store)
                goto dealloc_hostmem;

        posix2_set_mds_store (mds, store);
        posix2_cache_mds_handle (mds, handler);

        return 0;

 dealloc_hostmem:
        GF_FREE (mds->hostname);
        mds->hostname = NULL;
 error_return:
        return -1;
}

int
posix2_init (xlator_t *this)
{
        int32_t            ret    = 0;
        data_t            *export = NULL;
        struct posix2_mds *mds    = NULL;

        if (this->children) {
                gf_msg (this->name, GF_LOG_CRITICAL, 0, POSIX2_MSG_SUBVOL_ERR,
                        "FATAL: storage/posix2 cannot have subvolumes");
                goto error_return;
        }

        if (!this->parents) {
                gf_msg (this->name, GF_LOG_CRITICAL, 0, POSIX2_MSG_DANGLING_VOL,
                        "Volume file is dangling, bailing out..");
                goto error_return;
        }

        export = dict_get (this->options, "directory");
        if (!export) {
                gf_msg (this->name, GF_LOG_CRITICAL,
                        0, POSIX2_MSG_EXPORT_MISSING,
                        "Export not specified in volume file");
                goto error_return;
        }

        mds = GF_CALLOC (1, sizeof (*mds), gf_posix2_mt_private_t);
        if (!mds)
                goto error_return;
        ret = posix2_fill_mds (this, mds, (const char *)(export->data));
        if (ret)
                goto dealloc_mds;

        this->private = mds;
        return 0;

 dealloc_mds:
        GF_FREE (mds);
 error_return:
        return -1;
}

void
posix2_fini (xlator_t *this)
{
        return;
}

/**
 * Regular file operations: someone should really write a code generator
 * for this since this is pretty much bunch of invocations to the meta
 * store. Don't look here for anything interesting.
 */
int32_t
posix2_lookup (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xdata)
{
        struct posix2_mdstore_handler *handle = NULL;

        handle = posix2_get_cached_handle (this);
        if (handle->storeops->lookup)
                return handle->storeops->lookup (frame, this, loc, xdata);

        STACK_UNWIND_STRICT (lookup, frame, -1,
                             ENOTSUP, NULL, NULL, NULL, NULL);
        return 0;
}

int32_t
posix2_open (call_frame_t *frame, xlator_t *this,
            loc_t *loc, int32_t flags, fd_t *fd, dict_t *xdata)
{
        struct posix2_mdstore_handler *handle = NULL;

        handle = posix2_get_cached_handle (this);
        if (handle->storeops->open)
                return handle->storeops->open (frame, this, loc, flags, fd, xdata);

        STACK_UNWIND_STRICT (open, frame, -1, ENOTSUP, NULL, NULL);

        return 0;
}

int32_t
posix2_create (call_frame_t *frame,
                xlator_t *this, loc_t *loc, int32_t flags,
                mode_t mode, mode_t umask, fd_t *fd, dict_t *xdata)
{
        struct posix2_mdstore_handler *handle = NULL;

        handle = posix2_get_cached_handle (this);
        if (handle->storeops->create)
                return handle->storeops->create (frame, this, loc,
                                                 flags, mode, umask, fd, xdata);

        STACK_UNWIND_STRICT (create, frame, -1,
                             ENOTSUP, NULL, NULL, NULL, NULL, NULL, NULL);
        return 0;
}

int32_t
posix2_flush (call_frame_t *frame, xlator_t *this, fd_t *fd, dict_t *xdata)
{
        struct posix2_mdstore_handler *handle = NULL;

        handle = posix2_get_cached_handle (this);
        if (handle->storeops->flush)
                return handle->storeops->flush (frame, this, fd, xdata);

        STACK_UNWIND_STRICT (flush, frame, -1, ENOTSUP, NULL);
        return 0;
}

int32_t
posix2_setattr (call_frame_t *frame, xlator_t *this,
                loc_t *loc, struct iatt *stbuf, int32_t valid, dict_t *xdata)
{
        struct posix2_mdstore_handler *handle = NULL;

        handle = posix2_get_cached_handle (this);
        if (handle->storeops->setattr)
                return handle->storeops->setattr (frame, this,
                                                  loc, stbuf, valid, xdata);

        STACK_UNWIND_STRICT (setattr, frame, -1, ENOTSUP, NULL, NULL, NULL);
        return 0;
}

int32_t
posix2_stat (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xdata)
{
        struct posix2_mdstore_handler *handle = NULL;

        handle = posix2_get_cached_handle (this);
        if (handle->storeops->stat)
                return handle->storeops->stat (frame, this, loc, xdata);

        STACK_UNWIND_STRICT (stat, frame, -1, ENOTSUP, NULL, NULL);
        return 0;

}

int32_t
posix2_icreate (call_frame_t *frame,
                xlator_t *this, loc_t *loc, mode_t mode, dict_t *xdata)
{
        struct posix2_mdstore_handler *handle = NULL;

        handle = posix2_get_cached_handle (this);
        if (handle->storeops->icreate)
                return handle->storeops->icreate (frame, this,
                                                  loc, mode, xdata);

        STACK_UNWIND_STRICT (icreate, frame,
                             -1, ENOTSUP, NULL, NULL, NULL);
        return 0;
}

int32_t
posix2_namelink (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xdata)
{
        struct posix2_mdstore_handler *handle = NULL;

        handle = posix2_get_cached_handle (this);
        if (handle->storeops->namelink)
                return handle->storeops->namelink (frame, this, loc, xdata);

        STACK_UNWIND_STRICT (namelink, frame,
                             -1, ENOTSUP, NULL, NULL, NULL);
        return 0;
}

class_methods_t class_methods = {
        .init = posix2_init,
        .fini = posix2_fini,
};

struct xlator_fops fops = {
        .lookup   = posix2_lookup,
        .create   = posix2_create,
        .open     = posix2_open,
        .icreate  = posix2_icreate,
        .namelink = posix2_namelink,
        .flush    = posix2_flush,
        .setattr  = posix2_setattr,
        .stat     = posix2_stat,
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
