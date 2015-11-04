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
 * Given an export and a basename, calculate the entry handle length.
 * "parent" length is the length of canonical form of UUID.
 */
int
zfstore_handle_length (char *basepath)
{
        return strlen (basepath)        /*   ->exportdir   */
                + 1                     /*       /         */
                + 2                     /*   GFID[0.1]     */
                + 1                     /*       /         */
                + 2                     /*   GFID[2.3]     */
                + 1                     /*       /         */
                + 36                    /*   PARGFID       */
                + 1;                    /*       \0        */
}

int
zfstore_make_handle (xlator_t *this, char *export,
                     uuid_t gfid, char *handle, size_t handlesize)
{
        return snprintf (handle, handlesize, ZFSTORE_ENTRY_HANDLE_FMT,
                         export, gfid[0], gfid[1], uuid_utoa (gfid));
}

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

        entrylen = zfstore_handle_length (export);
        entry = alloca (entrylen);

        errno = EINVAL;
        entrylen = zfstore_make_handle (this, export, tgtuuid, entry, entrylen);
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
zfstore_create_dir_hashes (xlator_t *this, char *entry)
{
        int32_t ret = 0;
        char *duppath = NULL;
        char *parpath = NULL;

        duppath = strdupa (entry);

        /* twice.. so that we get to the end of first dir entry in the path */
        parpath = dirname (duppath);
        parpath = dirname (duppath);

        ret = mkdir (parpath, 0700);
        if ((ret == -1) && (errno != EEXIST)) {
                gf_msg (this->name, GF_LOG_ERROR, errno, 0,
                        "Error creating directory level #1 for [%s]", entry);
                goto error_return;
        }

        strcpy (duppath, entry);
        parpath = dirname (duppath);

        ret = mkdir (parpath, 0700);
        if ((ret == -1) && (errno != EEXIST)) {
                gf_msg (this->name, GF_LOG_ERROR, errno, 0,
                        "Error creating directory level #2 for [%s]", entry);
                goto error_return;
        }

        return 0;

 error_return:
        /* no point in rolling back */
        return -1;
}

/**
 * TODO: save separate inodeptr metadata.
 */
int32_t
zfstore_create_inode (xlator_t *this,
                      struct zfstore *zf,
                      char *entry, int32_t flags, mode_t mode)
{
        int fd = -1;
        int32_t ret = 0;
        gf_boolean_t isdir = S_ISDIR (mode);

        ret = zfstore_create_dir_hashes (this, entry);
        if (ret < 0)
                goto error_return;
        if (isdir)
                ret = mkdir (entry, mode);
        else {
                if (!flags)
                        flags = (O_CREAT | O_RDWR | O_EXCL);
                else
                        flags |= O_CREAT;

                fd = open (entry, flags, mode);
                if (fd < 0)
                        ret = -1;
                else
                        sys_close (fd);
        }

 error_return:
        return (ret < 0) ? -1 : 0;
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
