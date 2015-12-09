/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "zfstore-handle.h"

/**
 * Given a handle patch (constructed from GFID), fill in @stbuf with
 * inode attributes. @dircheck guarantees stat information only for
 * directories.
 */
static int32_t
zfstore_stat_handle (xlator_t *this,
                     struct zfstore *zf, uuid_t gfid,
                     char *path, struct iatt *stbuf, gf_boolean_t dircheck)
{
        int32_t ret = 0;
        struct stat *lstatbuf = NULL;
        struct mdinode mdi = {0,};
        struct mdoperations *md = NULL;

        md = zf_get_inodeops (zf);
        if (!md)
                return -1;
        ret = md->mdread (this, path, &mdi);
        if (ret)
                return -1;

        s_mdinode_to_stat (&lstatbuf, &mdi);
        if (dircheck && !S_ISDIR (lstatbuf->st_mode)) {
                errno = ENOTDIR;
                return -1;
        }
        if (stbuf) {
                iatt_from_stat (stbuf, lstatbuf);
                gf_uuid_copy (stbuf->ia_gfid, gfid);
                posix2_fill_ino_from_gfid (this, stbuf);
        }
        return 0;
}

int32_t
zfstore_resolve_inodeptr (xlator_t *this,
                          struct zfstore *zf,
                          uuid_t tgtuuid, char *ihandle,
                          struct iatt *stbuf, gf_boolean_t dircheck)
{
        return zfstore_stat_handle (this, zf, tgtuuid,
                                     ihandle, stbuf, dircheck);
}

int32_t
zfstore_resolve_inode (xlator_t *this,
                       struct zfstore *zf, uuid_t tgtuuid,
                       struct iatt *stbuf, gf_boolean_t dircheck)
{
        int entrylen = 0;
        char *entry = NULL;
        char *export = NULL;

        export = zf->exportdir;

        entrylen = posix2_handle_length (export);
        entry = alloca (entrylen);

        errno = EINVAL;
        entrylen = posix2_make_handle (this, export, tgtuuid, entry, entrylen);
        if (entrylen <= 0)
                goto error_return;

        return zfstore_resolve_inodeptr (this, zf, tgtuuid,
                                         entry, stbuf, dircheck);

 error_return:
        return -1;
}

int32_t
zfstore_resolve_entry (xlator_t *this,
                       struct zfstore *zf,
                       char *parpath, const char *basename, uuid_t gfid)
{
        int32_t ret = 0;
        char realpath[PATH_MAX] = {0,};
        struct mdname mdn = {0, };
        struct mdoperations *md = NULL;

        errno = EINVAL;
        md = zf_get_nameops (zf);
        if (!md)
                return -1;

        (void) snprintf (realpath, PATH_MAX, "%s/%s", parpath, basename);
        ret = md->mdread (this, realpath, &mdn);
        if (ret)
                return -1;

        s_mdname_to_gfid (gfid, &mdn);
        return 0;
}

int32_t
zfstore_handle_entry (xlator_t *this,
                      struct zfstore *zf,
                      char *parpath, const char *basename, struct iatt *stbuf)
{
        int32_t ret = 0;
        uuid_t tgtuuid = {0,};

        ret = zfstore_resolve_entry (this, zf, parpath, basename, tgtuuid);
        if (ret)
                goto error_return;

        ret = zfstore_resolve_inode (this, zf, tgtuuid, stbuf, _gf_false);
        if (ret < 0) {
                if (errno == ENOENT) {
                        gf_uuid_copy (stbuf->ia_gfid, tgtuuid);
                        errno = EREMOTE;
                }
                goto error_return;
        }

        return 0;

 error_return:
        return -1;
}

int32_t
zfstore_link_inode (xlator_t *this,
                    struct zfstore *zf,
                    char *parpath, const char *basename, uuid_t gfid)
{
        int32_t ret = -1;
        int fd = -1;
        char realpath[PATH_MAX] = {0,};
        struct mdname mdn = {0,};
        struct mdoperations *md = NULL;

        errno = EINVAL;
        md = zf_get_nameops (zf);
        if (!md)
                goto error_return;

        (void) snprintf (realpath, PATH_MAX, "%s/%s", parpath, basename);
        fd = open (realpath, O_CREAT | O_EXCL | O_WRONLY, 0700);
        if (fd < 0)
                goto error_return;

        s_gfid_to_mdname (gfid, &mdn);
        if (md->fmdwrite)
                ret = md->fmdwrite (this, fd, &mdn, NULL);
        else
                ret = md->mdwrite (this, realpath, &mdn, NULL);

        close (fd);

 error_return:
        return (ret < 0) ? -1 : 0;
}
