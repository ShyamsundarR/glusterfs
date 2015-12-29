/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef __POSIX_DS_H__
#define __POSIX_DS_H__

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "xlator.h"

struct posix2_ds {
        char *hostname;
        char *exportdir;
        gf_lock_t lock;

        DIR *mountlock;

        int64_t write_value;
};

#endif /* __POSIX_DS_H__ */
