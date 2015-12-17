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

#include "crc32c.h"

/**
 * Extended Attribute Store (EAS) maintains metadata in inode extended attribute
 * (or xattrs) for each filesystem object. Furthermore each filesystem object is
 * represented by (entry, inode) tuple as typical filesystem entries: files and
 * directories. Object placement is controlled via upper layer(s).
 */

#define ZF_XATTR_TEST_VAL  "okie-dokie"
#define ZF_XATTR_TEST_KEY  "trusted.glusterfs.xa-test"

#define ZF_XATTR_INODE_KEY "trusted.inode"

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

static int32_t
zfxattr_verify_crc32 (xlator_t *this, void *data, size_t size, uint32_t crcdisk)
{
        uint32_t crcmem = 0;

        crcmem = crc32cSlicingBy8 (0, data, size);
        if (crcmem != crcdisk) {
                gf_msg (this->name, GF_LOG_CRITICAL, 0, 0, "Metadata checksum "
                        "mismatch [Disk: %u, CRC: %u]", crcdisk, crcmem);
                return -1;
        }

        return 0;
}

static int32_t
zfxattr_get_xa (void *hdl, char *key,
                void *value, size_t size, gf_boolean_t isfd)
{
        ssize_t rsize = 0;

        if (isfd)
                rsize = sys_fgetxattr (*(int *)hdl, key, value, size);
        else
                rsize = sys_lgetxattr ((char *)hdl, key, value, size);

        if (rsize == -1)
                return -1;
        if (rsize != size) {
                errno = EINVAL;
                return -1;
        }

        return 0;
}

static int32_t
zfxattr_set_xa (void *hdl, char *key, void *value,
                size_t size, struct writecontrol *wc, gf_boolean_t isfd)
{
        if (isfd)
                return sys_fsetxattr (*(int *)hdl, key, value, size, 0);
        else
                return sys_lsetxattr ((char *)hdl, key, value, size, 0);
}

static void
zfxattr_mdname_to_zfmdname (struct mdname *mdn, struct zfxattr_mdname *zmdn)
{
        memcpy (&zmdn->mdn, mdn, sizeof (*mdn));
        zmdn->crc = crc32cSlicingBy8 (0, zmdn, sizeof (*zmdn));
}

static void
zfxattr_mdinode_to_zfmdinode (struct mdinode *mdi, struct zfxattr_mdinode *zmdi)
{
        memcpy (&zmdi->mdi, mdi, sizeof (*mdi));
        zmdi->crc = crc32cSlicingBy8 (0, zmdi, sizeof (*zmdi));
}

static int32_t
zfxattr_zfmdname_to_mdname (xlator_t *this,
                            struct mdname *mdn, struct zfxattr_mdname *zmdn)
{
        uint32_t crcdisk = zmdn->crc;

        zmdn->crc = 0;
        if (zfxattr_verify_crc32 (this, zmdn, sizeof (*zmdn), crcdisk) < 0) {
                errno = EIO;
                return -1;
        }

        memcpy (mdn, &zmdn->mdn, sizeof (*mdn));
        return 0;
}

static int32_t
zfxattr_zfmdinode_to_stat (xlator_t *this,
                           struct zfxattr_mdinode *zmdi, struct stat *stbuf)
{
        uint32_t crcdisk = zmdi->crc;
        struct mdinode *mdi = NULL;

        zmdi->crc = 0;
        if (zfxattr_verify_crc32 (this, zmdi, sizeof (*zmdi), crcdisk) < 0) {
                errno = EIO;
                return -1;
        }

        mdi = &zmdi->mdi;

        /* fixup inode type */
        stbuf->st_mode = ((stbuf->st_mode & ~S_IFMT) | mdi->type);

        /* and the rest.. */
        stbuf->st_nlink  = mdi->nlink;
        stbuf->st_rdev   = mdi->rdev;
        stbuf->st_size   = mdi->size;
        stbuf->st_blocks = mdi->blocks;

        return 0;
}

/* name entry metadata operations */
int32_t
zfxattr_name_mdread (xlator_t *this, void *handle, void *md)
{
        int32_t ret = 0;
        struct mdname *mdn = md;
        struct zfxattr_mdname zmdn = {0, };

        ret = zfxattr_get_xa
                     (handle, GFID_XATTR_KEY, &zmdn, sizeof (zmdn), _gf_false);
        if (ret)
                return -1;

        return zfxattr_zfmdname_to_mdname (this, mdn, &zmdn);
}

int32_t
zfxattr_name_mdwrite (xlator_t *this,
                      void *handle, void *md, struct writecontrol *wc)
{
        struct mdname *mdn = md;
        struct zfxattr_mdname zmdn = {0,};

        zfxattr_mdname_to_zfmdname (mdn, &zmdn);
        return zfxattr_set_xa
                  (handle, GFID_XATTR_KEY, &zmdn, sizeof (zmdn), wc, _gf_false);
}

int32_t
zfxattr_name_fmdread (xlator_t *this, int fd, void *md)
{
        int32_t ret = 0;
        struct mdname *mdn = md;
        struct zfxattr_mdname zmdn = {0,};

        ret = zfxattr_get_xa
                     (&fd, GFID_XATTR_KEY, &zmdn, sizeof (zmdn), _gf_true);
        if (ret)
                return -1;

        return zfxattr_zfmdname_to_mdname (this, mdn, &zmdn);
}

int32_t
zfxattr_name_fmdwrite (xlator_t *this,
                       int fd, void *md, struct writecontrol *wc)
{
        struct mdname *mdn = md;
        struct zfxattr_mdname zmdn = {0,};

        zfxattr_mdname_to_zfmdname (mdn, &zmdn);
        return zfxattr_set_xa
                   (&fd, GFID_XATTR_KEY, &zmdn, sizeof (zmdn), wc, _gf_true);
}

/* inode metadata operations */

int32_t
zfxattr_inode_mdread (xlator_t *this, void *handle, void *md)
{
        int32_t ret = 0;
        struct stat *stbuf = md;
        struct zfxattr_mdinode zmdi = {0,};

        ret = sys_lstat ((char *)handle, stbuf);
        if (ret)
                return -1;
        if (S_ISDIR (stbuf->st_mode))
                return 0;

        ret = zfxattr_get_xa
                  (handle, ZF_XATTR_INODE_KEY, &zmdi, sizeof (zmdi), _gf_false);
        if (ret)
                return -1;

        return zfxattr_zfmdinode_to_stat (this, &zmdi, stbuf);
}

int32_t
zfxattr_inode_mdwrite (xlator_t *this,
                       void *handle, void *md, struct writecontrol *wc)
{
        struct mdinode *mdi = md;
        struct zfxattr_mdinode zmdi = {0,};

        if (mdi->type == S_IFDIR)
                return 0;

        zfxattr_mdinode_to_zfmdinode (mdi, &zmdi);
        return zfxattr_set_xa (handle, ZF_XATTR_INODE_KEY,
                               &zmdi, sizeof (zmdi), wc, _gf_false);
}

int32_t
zfxattr_inode_fmdread (xlator_t *this, int fd, void *md)
{
        int32_t ret = 0;
        struct stat *stbuf = md;
        struct zfxattr_mdinode zmdi = {0,};

        ret = sys_fstat (fd, stbuf);
        if (ret)
                return -1;
        if (S_ISDIR (stbuf->st_mode))
                return 0;

        ret = zfxattr_get_xa
                     (&fd, ZF_XATTR_INODE_KEY, &zmdi, sizeof (zmdi), _gf_true);
        if (ret)
                return -1;

        return zfxattr_zfmdinode_to_stat (this, &zmdi, stbuf);
}

int32_t
zfxattr_inode_fmdwrite (xlator_t *this,
                        int fd, void *md, struct writecontrol *wc)
{
        struct mdinode *mdi = md;
        struct zfxattr_mdinode zmdi = {0,};

        if (mdi->type == S_IFDIR)
                return 0;

        zfxattr_mdinode_to_zfmdinode (mdi, &zmdi);
        return zfxattr_set_xa
                (&fd, ZF_XATTR_INODE_KEY, &zmdi, sizeof (zmdi), wc, _gf_true);
}

struct mdoperations zfxattr_nameops = {
        .dialloc  = zfstore_name_dialloc, /* default inode allocation */
        .mdread   = zfxattr_name_mdread,
        .mdwrite  = zfxattr_name_mdwrite,
        .fmdread  = zfxattr_name_fmdread,
        .fmdwrite = zfxattr_name_fmdwrite,
};

struct mdoperations zfxattr_inodeops = {
        .dialloc  = zfstore_dialloc, /* default inode allocation */
        .mdread   = zfxattr_inode_mdread,
        .mdwrite  = zfxattr_inode_mdwrite,
        .fmdread  = zfxattr_inode_fmdread,
        .fmdwrite = zfxattr_inode_fmdwrite,
};
