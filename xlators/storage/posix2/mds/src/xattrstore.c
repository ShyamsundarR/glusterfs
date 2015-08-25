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
#include "xattrstore.h"

/**
 * Extended Attribute Store (EAS) maintains metadata in inode extended attribute
 * (or xattrs) for each filesystem object. Furthermore each filesystem object is
 * represented by (entry, inode) tuple as typical filesystem entries: files and
 * directories. Object placement is controlled via upper layer(s).
 */

#define POSIX2_XATTR_TEST_VAL  "okie-dokie"
#define POSIX2_XATTR_TEST_KEY  "trusted.glusterfs.xa-test"

static int32_t
xattrstore_test_xa (xlator_t *this, const char *export)
{
        int32_t ret = 0;

        ret = sys_lsetxattr (export,
                             POSIX2_XATTR_TEST_KEY, POSIX2_XATTR_TEST_VAL,
                             (sizeof (POSIX2_XATTR_TEST_VAL) - 1), 0);
        if (ret == 0) {
                (void) sys_lremovexattr (export, POSIX2_XATTR_TEST_KEY);
        } else {
                gf_msg (this->name, GF_LOG_CRITICAL, 0,
                        POSIX2_MSG_EXPORT_NO_XASUP, "Export [%s] does not "
                        "support extended attributes", export);
        }

        return (ret != 0) ? -1 : 0;
}

/**
 * Given a export, check the validity of it's volume-id by comparing
 * the on-disk volume-id with the volume-id present in the volfile.
 */
static int32_t
xattrstore_validate_volume_id (xlator_t *this, const char *export)
{
        int32_t ret = 0;
        ssize_t size = 0;
        data_t *volumeid = NULL;
        uuid_t volumeuuid = {0,};
        uuid_t diskuuid = {0,};

        volumeid = dict_get (this->options, "volume-id");
        if (!volumeid)
                return 0;

        ret = gf_uuid_parse (volumeid->data, volumeuuid);
        if (ret < 0) {
                gf_msg (this->name, GF_LOG_ERROR,
                        0, POSIX2_MSG_EXPORT_INVAL_VOLID,
                        "Invalid volume-id set in volfile.");
                goto error_return;
        }

        size = sys_lgetxattr (export,
                              GF_XATTR_VOL_ID_KEY, diskuuid, sizeof (uuid_t));
        if (size == sizeof (uuid_t)) {
                if (gf_uuid_compare (diskuuid, volumeuuid)) {
                        gf_msg (this->name, GF_LOG_ERROR, 0,
                                POSIX2_MSG_EXPORT_INVAL_VOLID, "mismatching "
                                "volume-id. Volume may be already a part of "
                                "volume [%s]", uuid_utoa (diskuuid));
                        goto error_return;
                }
        } else if ((size == -1) && (errno == ENODATA || errno == ENOATTR)) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        POSIX2_MSG_EXPORT_MISSING_VOLID,
                        "extended attribute [%s] missing on export [%s]",
                        GF_XATTR_VOL_ID_KEY, export);
                goto error_return;
        } else {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        POSIX2_MSG_EXPORT_VOLID_FAILURE, "Failed to fetch "
                        "volume id from export [%s]", export);
                goto error_return;
        }

        return 0;

 error_return:
        return -1;
}

/**
 * Prepare metadata store: lock the export (by taking an active reference
 * on it, so that it doesn't go away in mid flight), setup locks, etc.
 */
static struct xattrstore *
xattrstore_prepare_store (xlator_t *this, const char *export)
{
        struct xattrstore *xas = NULL;

        xas = GF_CALLOC (1, sizeof (*xas), gf_posix2_mt_xattrstore_t);
        if (!xas)
                goto error_return;

        xas->exportdir = gf_strdup (export);
        if (!xas->exportdir)
                goto dealloc_1;

        xas->mountlock = opendir (xas->exportdir);
        if (!xas->mountlock)
                goto dealloc_2;
        LOCK_INIT (&xas->lock);

        return xas;

 dealloc_2:
        GF_FREE (xas->exportdir);
 dealloc_1:
        GF_FREE (xas);
 error_return:
        return NULL;
}

void *
xattrstore_ctor (xlator_t *this, const char *export)
{
        int32_t ret = 0;
        struct stat stbuf = {0,};
        struct xattrstore *xas = NULL;

        ret = stat (export, &stbuf);
        if ((ret != 0) || !S_ISDIR (stbuf.st_mode)) {
                gf_msg (this->name, GF_LOG_ERROR, 0, POSIX2_MSG_EXPORT_NOTDIR,
                        "Export [%s] is %s.", export,
                        (ret == 0) ? "not a directory" : "unusable");
                goto error_return;
        }

        /* validations.. */
        ret = xattrstore_test_xa (this, export);
        if (ret)
                goto error_return;
        ret = xattrstore_validate_volume_id (this, export);
        if (ret)
                goto error_return;

        /* setup up store */
        xas = xattrstore_prepare_store (this, export);
        if (!xas)
                goto error_return;
        return xas;

 error_return:
        return NULL;
}

int
xattrstore_dtor (xlator_t *this, void *store)
{
        struct xattrstore *xas = store;

        /**
         * release active reference on the export. we may need to wait
         * for pending IOs to finish up (TODO later).
         */
        (void) closedir (xas->mountlock);

        GF_FREE (xas->exportdir);
        LOCK_DESTROY (&xas->lock);

        GF_FREE (xas);
        return 0;
}

struct xlator_fops xattrstore_fops = {
        .lookup = xattrstore_lookup,
};
