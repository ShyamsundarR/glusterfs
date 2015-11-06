/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef _ZFXATTR_H_
#define _ZFXATTR_H_

#include "xlator.h"
#include "super.h"

/**
 * Excapsulate the name metadata into our own structure and checksum it
 * with crc32c, which is verified when metadata is read back.
 */
struct __attribute__ ((__packed__)) zfxattr_mdname {
        struct mdname mdn;
        uint32_t crc;
};

int32_t zfxattr_init (xlator_t *, const char *, struct md_namei_ops *);
int32_t zfxattr_fini (xlator_t *, const char *);

#endif /* _ZFXATTR_H_ */
