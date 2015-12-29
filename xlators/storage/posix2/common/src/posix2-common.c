/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "posix2.h"
#include "posix2-mem-types.h"

/**
 * Set of common (helper) routines for metadata & data implementation.
 * Make sure whatever ends up here is "really really" common. No hacks
 * please.
 */

/**
 * Given an export and a basename, calculate the entry handle length.
 * "parent" length is the length of canonical form of UUID.
 */
int
posix2_handle_length (char *basepath)
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
posix2_make_handle (xlator_t *this, char *export,
                     uuid_t gfid, char *handle, size_t handlesize)
{
        return snprintf (handle, handlesize, ZFSTORE_ENTRY_HANDLE_FMT,
                         export, gfid[0], gfid[1], uuid_utoa (gfid));
}

int32_t
posix2_create_dir_hashes (xlator_t *this, char *entry)
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
posix2_create_inode (xlator_t *this,
                      char *entry, int32_t flags, mode_t mode)
{
        int fd = -1;
        int32_t ret = 0;
        gf_boolean_t isdir = S_ISDIR (mode);

        ret = posix2_create_dir_hashes (this, entry);
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
posix2_lookup_is_nameless (loc_t *loc)
{
        return (gf_uuid_is_null (loc->pargfid) && !loc->name);
}

void
posix2_fill_ino_from_gfid (xlator_t *this, struct iatt *buf)
{
        uint64_t temp_ino = 0;
        int j = 0;
        int i = 0;

        /* consider least significant 8 bytes of value out of gfid */
        if (gf_uuid_is_null (buf->ia_gfid)) {
                buf->ia_ino = -1;
                goto out;
        }
        for (i = 15; i > (15 - 8); i--) {
                temp_ino += (uint64_t)(buf->ia_gfid[i]) << j;
                j += 8;
        }
        buf->ia_ino = temp_ino;
out:
        return;
}

int
posix2_fdstat (xlator_t *this, int fd, uuid_t gfid, struct iatt *stbuf_p)
{
        int                    ret     = 0;
        struct stat            fstatbuf = {0, };
        struct iatt            stbuf = {0, };

        ret = fstat (fd, &fstatbuf);
        if (ret == -1)
                goto out;

        iatt_from_stat (&stbuf, &fstatbuf);
        gf_uuid_copy (stbuf.ia_gfid, gfid);
        posix2_fill_ino_from_gfid (this, &stbuf);

        if (stbuf_p)
                *stbuf_p = stbuf;

out:
        return ret;
}

int32_t
posix2_save_openfd (xlator_t *this, fd_t *fd, int openfd, int32_t flags)
{
        int32_t ret = 0;
        struct posix2_fd *pfd = NULL;

        pfd = GF_CALLOC (1, sizeof (*pfd), gf_posix2_mt_posix2_fd_t);
        if (!pfd)
                return -1;

        pfd->fd = openfd;
        pfd->flags = flags;

        ret = fd_ctx_set (fd, this, (uint64_t)(long)pfd);
        if (ret) {
                GF_FREE (pfd);
                pfd = NULL;
        }
        return ret;
}
