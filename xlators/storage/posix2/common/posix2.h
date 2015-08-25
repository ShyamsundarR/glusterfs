/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef __POSIX2_H__
#define __POSIX2_H__

#include "xlator.h"
#include "uuid.h"

struct posix2_fd {
        int fd;
        int32_t flags;
};

int32_t posix2_lookup_is_nameless (loc_t *);

#endif /* __POSIX2_H__ */
