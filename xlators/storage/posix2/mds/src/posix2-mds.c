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

#include "xattrstore.h"

static struct posix2_mdstore_handler posix2_mdslist[] = {
        {
                .ctor     = xattrstore_ctor,
                .dtor     = xattrstore_dtor,
                .storeops = NULL,
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
init (xlator_t *this)
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

        export = dict_get (this->options, "export");
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
