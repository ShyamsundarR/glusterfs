/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef _ZFINODE_H_
#define _ZFINODE_H_

#include "super.h"

void zfstore_ifill (struct mdinode *, mode_t,
                       uint32_t, uint32_t, uint64_t, uint64_t);
void zfstore_ifill_default (struct mdinode *, mode_t);

int32_t
zfstore_dialloc (xlator_t *,
                 struct mdoperations *,
                 char *, int32_t, mode_t, void *);
int32_t
zfstore_name_dialloc (xlator_t *,
                      struct mdoperations *,
                      char *, int32_t, mode_t, void *);

/* inode updation flags */

#define ZF_INODE_TYPE   (1<<0)
#define ZF_INODE_NLINK  (1<<1)
#define ZF_INODE_RDEV   (1<<2)
#define ZF_INODE_SIZE   (1<<3)
#define ZF_INODE_BLOCKS (1<<4)

#define ZF_INODE_ALL                                                    \
        (ZF_INODE_MODE | ZF_INODE_NLINK | ZF_INODE_UID     |            \
         ZF_INODE_GID | ZF_INODE_RDEV | ZF_INODE_SIZE      |            \
         ZF_INODE_BLOCKS | ZF_INODE_ATIME |ZF_INODE_CTIME  |            \
         ZF_INODE_MTIME)

#endif /* _ZFINODE_H_ */
