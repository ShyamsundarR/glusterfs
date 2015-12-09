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

#include <libgen.h>
#include "xlator.h"
#include "uuid.h"
#include "syscall.h"

#define ZFSTORE_ENTRY_HANDLE_FMT  "%s/%02x/%02x/%s"

/* TODO: DIR *, O_DIRECT */
struct posix2_fd {
        int fd;
        int32_t flags;
};

int32_t posix2_lookup_is_nameless (loc_t *);
void posix2_fill_ino_from_gfid (xlator_t *, struct iatt *);
int32_t posix2_save_openfd (xlator_t *, fd_t *, int, int32_t);

int posix2_handle_length (char *);
int posix2_make_handle (xlator_t *, char *, uuid_t, char *, size_t);
int32_t posix2_create_dir_hashes (xlator_t *, char *);
int32_t posix2_create_inode (xlator_t *, char *, int32_t, mode_t);
#endif /* __POSIX2_H__ */
