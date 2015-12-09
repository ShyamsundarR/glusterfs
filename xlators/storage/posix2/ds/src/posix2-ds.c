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
#include "posix2-ds-messages.h"
#include "posix2-mem-types.h"

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

        ds->hostname = GF_CALLOC (256, sizeof (char), gf_common_mt_char);
        if (!ds->hostname)
                goto error_return;
        ret = gethostname (ds->hostname, 256);
        if (ret < 0)
                goto dealloc_hostmem;

        return 0;

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

class_methods_t class_methods = {
        .init = posix2_ds_init,
        .fini = posix2_ds_fini,
};

struct xlator_fops fops = {
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
