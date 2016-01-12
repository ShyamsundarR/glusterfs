/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "posix2-mds.h"
#include "posix2-messages.h"
#include "posix2-mem-types.h"
#include "zfstore.h"

#include "zfxattr.h"

static struct mdstore zf_mdstore[] = {
        {
                .mdinit = zfxattr_init,
                .mdfini = zfxattr_fini,
        },
};

static struct mdstore *
zfstore_find_metadata_store (xlator_t *this)
{
        return &zf_mdstore[0];
}

/**
 * Prepare metadata store: lock the export (by taking an active reference
 * on it, so that it doesn't go away in mid flight), setup locks, etc.
 */
static struct zfstore *
zfstore_prepare_store (xlator_t *this,
                       const char *export, struct md_namei_ops *ni)
{
        struct zfstore *zf = NULL;

        zf = GF_CALLOC (1, sizeof (*zf), gf_posix2_mt_zfstore_t);
        if (!zf)
                goto error_return;

        zf->exportdir = gf_strdup (export);
        if (!zf->exportdir)
                goto dealloc_1;

        zf->mountlock = opendir (zf->exportdir);
        if (!zf->mountlock)
                goto dealloc_2;
        LOCK_INIT (&zf->lock);

        zf->ni = ni;
        return zf;

 dealloc_2:
        GF_FREE (zf->exportdir);
 dealloc_1:
        GF_FREE (zf);
 error_return:
        return NULL;
}

void *
zfstore_ctor (xlator_t *this, const char *export)
{
        int32_t ret = 0;
        struct stat stbuf = {0,};
        struct zfstore *zf = NULL;
        struct mdstore *md = NULL;
        struct md_namei_ops *ni = NULL;

        ret = stat (export, &stbuf);
        if ((ret != 0) || !S_ISDIR (stbuf.st_mode)) {
                gf_msg (this->name, GF_LOG_ERROR, 0, POSIX2_MSG_EXPORT_NOTDIR,
                        "Export [%s] is %s.", export,
                        (ret == 0) ? "not a directory" : "unusable");
                goto error_return;
        }

        md = zfstore_find_metadata_store (this);
        if (!md)
                goto error_return;
        ni = GF_CALLOC (1, sizeof (*ni), gf_posix2_mt_nameiops_t);
        if (!ni)
                goto error_return;
        if (md->mdinit) {
                ret = md->mdinit (this, export, ni);
                if (ret)
                        goto free_nameiops;
        }

        /* setup up store */
        zf = zfstore_prepare_store (this, export, ni);
        if (!zf)
                goto free_nameiops;
        return zf;

 free_nameiops:
        GF_FREE (ni);
 error_return:
        return NULL;
}

int
zfstore_dtor (xlator_t *this, void *store)
{
        struct zfstore *zf = store;

        /**
         * release active reference on the export. we may need to wait
         * for pending IOs to finish up (TODO later).
         */
        (void) closedir (zf->mountlock);

        GF_FREE (zf->exportdir);
        LOCK_DESTROY (&zf->lock);

        GF_FREE (zf);
        return 0;
}

struct xlator_fops zfstore_fops = {
        .lookup   = zfstore_lookup,
        .create   = zfstore_create,
        .open     = zfstore_open,
        .icreate  = zfstore_icreate,
        .namelink = zfstore_namelink,
        .flush    = zfstore_flush,
        .setattr  = zfstore_setattr,
        .stat     = zfstore_stat,
        .fstat    = zfstore_fstat,
};
