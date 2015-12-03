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

/* TODO: DIR *, O_DIRECT */
struct posix2_fd {
        int      fd;
        DIR     *dirfd;
        int32_t  flags;
};

enum posix2_raw_fd_type {
        POSIX2_DIR_FD = 1,
        POSIX2_FILE_FD,
};

struct posix2_raw_fd
{
        enum posix2_raw_fd_type type;
        union {
                DIR *dirfd;
                int  filefd;
        } fd;
};

int32_t posix2_lookup_is_nameless (loc_t *);
void posix2_fill_ino_from_gfid (xlator_t *, struct iatt *);
int32_t posix2_save_openfd (xlator_t *, fd_t *, int, int32_t);
int32_t posix2_release_openfd (xlator_t *this, fd_t *fd);
int32_t posix2_fetch_openfd (xlator_t *, fd_t *, struct posix2_raw_fd *);

#endif /* __POSIX2_H__ */
