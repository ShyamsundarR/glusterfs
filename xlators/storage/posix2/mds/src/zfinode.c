/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "xlator.h"
#include "zfinode.h"
#include "posix2.h"

void
zfstore_ifill (struct mdinode *mdi, mode_t mode,
               uint32_t nlink, uint32_t rdev, uint64_t size, uint64_t blocks)
{
        mdi->type   = (mode & S_IFMT);
        mdi->nlink  = nlink;
        mdi->rdev   = rdev;
        mdi->size   = size;
        mdi->blocks = blocks;
}

void
zfstore_ifill_default (struct mdinode *mdi, mode_t mode)
{
        zfstore_ifill (mdi, mode, 1, 0, 0, 0);
}

static int32_t
zfstore_dialloc_reg (xlator_t *this,
                     struct mdoperations *md,
                     char *entry, int32_t flags, mode_t mode, void *meta)
{
        int fd = 0;
        int32_t ret = 0;

        if (!flags)
                flags = (O_CREAT | O_RDWR | O_EXCL);
        else
                flags |= O_CREAT;

        fd = open (entry, flags, mode);
        if (fd < 0)
                return -1;

        if (md->fmdwrite)
                ret = md->fmdwrite (this, fd, meta, NULL);
        else
                ret = md->mdwrite (this, entry, meta, NULL);

        close (fd);
        return ret;
}

static int32_t
zfstore_dialloc_dir (xlator_t *this,
                     struct mdoperations *md,
                     char *entry, mode_t mode, void *meta)
{
        int32_t ret = 0;

        ret = mkdir (entry, mode);
        if (ret)
                return -1;

        return md->mdwrite (this, entry, meta, NULL);
}

/**
 * Allocate an on-disk inode for the inode pointer.
 *
 * OK, some details about inode types here - directory inodes are created as
 * directories and the rest of the (six) types are just plain regular files
 * with the inode metadata specifying the type (and other bits of information)
 * about the inode.
 */
int32_t
zfstore_dialloc (xlator_t *this,
                 struct mdoperations *md,
                 char *entry, int32_t flags, mode_t mode, void *meta)
{
        int32_t ret = 0;

        ret = posix2_create_dir_hashes (this, entry);
        if (ret < 0)
                return -1;

        if (S_ISDIR (mode))
                ret = zfstore_dialloc_dir (this, md, entry, mode, meta);
        else
                ret = zfstore_dialloc_reg (this, md, entry, flags, mode, meta);

        return ret;
}

int32_t
zfstore_name_dialloc (xlator_t *this,
                      struct mdoperations *md,
                      char *entry, int32_t flags, mode_t mode, void *meta)
{
        int fd = 0;
        int32_t ret = 0;

        fd = open (entry, flags, mode);
        if (fd < 0)
                return -1;

        if (md->fmdwrite)
                ret = md->fmdwrite (this, fd, meta, NULL);
        else
                ret = md->mdwrite (this, entry, meta, NULL);

        close (fd);
        return ret;
}
