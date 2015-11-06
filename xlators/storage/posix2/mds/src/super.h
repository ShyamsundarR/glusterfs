/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef _SUPER_H_
#define _SUPER_H_

#include "xlator.h"

/**
 * Control write (updation) of metadata.
 */
struct writecontrol {
        gf_boolean_t buffered;
        gf_boolean_t create;
};

struct __attribute__ ((__packed__)) mdname {
        uuid_t uuid;
};

/**
 * FIXME: have our *own* stat structure
 */
struct __attribute__ ((__packed__)) mdinode {
        struct stat stbuf;
};

/**
 * This structure is similar to a stripped version of "struct super_operations"
 * that one would find on a traditional block based file system.
 */
struct mdoperations {
        int32_t (*mdread) (xlator_t *this, void *handle, void *md);
        int32_t (*mdwrite) (xlator_t *this,
                            void *handle, void *md, struct writecontrol *wc);

        /**
         * If the metadata store supports operations using file descriptors,
         * use these routines. The file descrptor should point to the correct
         * inode for which the metadata needs to be updated. When the filestore
         * needs to update metadata and has a file descriptor ready for use,
         * non null ->fmdread, ->fmdwrite routines would be invoked, falling
         * back to handle based operation (->mdread, ->mdwrite).
         */
        int32_t (*fmdread) (xlator_t *this, int fd, void *md);
        int32_t (*fmdwrite) (xlator_t *this,
                             int fd, void *md, struct writecontrol *wc);
};

struct md_namei_ops {
        struct mdoperations *nameops;
        struct mdoperations *inodeops;
};

/* the store */
struct mdstore {
        int32_t (*mdinit) (xlator_t *this,
                           const char *handle, struct md_namei_ops *ni);
        int32_t (*mdfini) (xlator_t *this, const char *handle);
};

static inline void s_mdname_to_gfid (uuid_t uuid, struct mdname *mdn)
{
        gf_uuid_copy (uuid, mdn->uuid);
}

static inline void s_gfid_to_mdname (uuid_t uuid, struct mdname *mdn)
{
        gf_uuid_copy (mdn->uuid, uuid);
}

static inline void s_mdinode_to_stat (struct stat **stbuf, struct mdinode *mdi)
{
        *stbuf = &mdi->stbuf;
}

#endif /* _SUPER_H_ */
