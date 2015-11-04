/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "zfxattr.h"
#include "syscall.h"
#include "compat-errno.h"
#include "posix2-messages.h"

/**
 * Extended Attribute Store (EAS) maintains metadata in inode extended attribute
 * (or xattrs) for each filesystem object. Furthermore each filesystem object is
 * represented by (entry, inode) tuple as typical filesystem entries: files and
 * directories. Object placement is controlled via upper layer(s).
 */

#define ZF_XATTR_TEST_VAL  "okie-dokie"
#define ZF_XATTR_TEST_KEY  "trusted.glusterfs.xa-test"

static int32_t
zfxattr_test_xa (xlator_t *this, const char *export)
{
        int32_t ret = 0;

        ret = sys_lsetxattr (export,
                             ZF_XATTR_TEST_KEY, ZF_XATTR_TEST_VAL,
                             (sizeof (ZF_XATTR_TEST_VAL) - 1), 0);
        if (ret == 0) {
                (void) sys_lremovexattr (export, ZF_XATTR_TEST_KEY);
        } else {
                gf_msg (this->name, GF_LOG_CRITICAL, 0,
                        POSXI2_ZFXATTR_MSG_EXPORT_NO_XASUP, "Export [%s] does "
                        "not support extended attributes", export);
        }

        return (ret != 0) ? -1 : 0;
}

/**
 * Given a export, check the validity of it's volume-id by comparing
 * the on-disk volume-id with the volume-id present in the volfile.
 */
static int32_t
zfxattr_validate_volume_id (xlator_t *this, const char *export)
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
                        0, POSIX2_ZFXATTR_MSG_EXPORT_INVAL_VOLID,
                        "Invalid volume-id set in volfile.");
                goto error_return;
        }

        size = sys_lgetxattr (export,
                              GF_XATTR_VOL_ID_KEY, diskuuid, sizeof (uuid_t));
        if (size == sizeof (uuid_t)) {
                if (gf_uuid_compare (diskuuid, volumeuuid)) {
                        gf_msg (this->name, GF_LOG_ERROR, 0,
                                POSIX2_ZFXATTR_MSG_EXPORT_INVAL_VOLID,
                                "mismatching volume-id. Volume may be already "
                                "a part of volume [%s]", uuid_utoa (diskuuid));
                        goto error_return;
                }
        } else if ((size == -1) && (errno == ENODATA || errno == ENOATTR)) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        POSIX2_ZFXATTR_MSG_EXPORT_MISSING_VOLID,
                        "extended attribute [%s] missing on export [%s]",
                        GF_XATTR_VOL_ID_KEY, export);
                goto error_return;
        } else {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        POSIX2_ZFXATTR_MSG_EXPORT_VOLID_FAILURE, "Failed to "
                        "fetch volume id from export [%s]", export);
                goto error_return;
        }

        return 0;

 error_return:
        return -1;
}

struct mdoperations zfxattr_nameops;
struct mdoperations zfxattr_inodeops;

int32_t
zfxattr_init (xlator_t *this, const char *handle, struct md_namei_ops *ni)
{
        int32_t ret = 0;
        const char *export = handle;

        /* validations.. */
        ret = zfxattr_test_xa (this, export);
        if (ret)
                goto error_return;
        ret = zfxattr_validate_volume_id (this, export);
        if (ret)
                goto error_return;

        /* ok, export is usable. set metadata handlers */
        ni->nameops  = &zfxattr_nameops;
        ni->inodeops = &zfxattr_inodeops;

        return 0;

 error_return:
        return -1;
}

int32_t
zfxattr_fini (xlator_t *this, const char *handle)
{
        return 0;
}

/* name entry metadata operations */
int32_t
zfxattr_name_mdread (xlator_t *this, void *handle, void *md)
{
        size_t size = 0;
        char *path = handle;
        struct mdname *mdn = md;

        size = sys_lgetxattr (path, GFID_XATTR_KEY, mdn, sizeof (*mdn));
        if (size == -1)
                return -1;
        if (size != sizeof (struct mdname)) {
                errno = EINVAL;
                return -1;
        }

        return 0;
}

int32_t
zfxattr_name_mdwrite (xlator_t *this,
                      void *handle, void *md, struct writecontrol *wc)
{
        char *path = handle;
        struct mdname *mdn = md;

        return sys_lsetxattr (path, GFID_XATTR_KEY, mdn, sizeof (*mdn), 0);
}

int32_t
zfxattr_name_fmdread (xlator_t *this, int fd, void *md)
{
        return 0;
}

int32_t
zfxattr_name_fmdwrite (xlator_t *this,
                       int fd, void *md, struct writecontrol *wc)
{
        struct mdname *mdn = md;

        return sys_fsetxattr (fd, GFID_XATTR_KEY, mdn, sizeof (*mdn), 0);
}

/* inode metadata operations */

int32_t
zfxattr_inode_mdread (xlator_t *this, void *handle, void *md)
{
        char *path = handle;
        struct mdinode *mdi = md;

        return lstat (path, &mdi->stbuf);
}

int32_t
zfxattr_inode_mdwrite (xlator_t *this,
                       void *handle, void *md, struct writecontrol *wc)
{
        return 0;
}

int32_t
zfxattr_inode_fmdread (xlator_t *this, int fd, void *md)
{
        struct mdinode *mdi = md;

        return fstat (fd, &mdi->stbuf);
}

int32_t
zfxattr_inode_fmdwrite (xlator_t *this,
                        int fd, void *md, struct writecontrol *wc)
{
        return 0;
}

struct mdoperations zfxattr_nameops = {
        .mdread   = zfxattr_name_mdread,
        .mdwrite  = zfxattr_name_mdwrite,
        .fmdread  = zfxattr_name_fmdread,
        .fmdwrite = zfxattr_name_fmdwrite,
};

struct mdoperations zfxattr_inodeops = {
        .mdread   = zfxattr_inode_mdread,
        .mdwrite  = zfxattr_inode_mdwrite,
        .fmdread  = zfxattr_inode_fmdread,
        .fmdwrite = zfxattr_inode_fmdwrite,
};
