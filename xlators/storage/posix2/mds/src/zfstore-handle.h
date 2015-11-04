/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef _ZFSTORE_HANDLE_H_
#define _ZFSTORE_HANDLE_H_

#include <libgen.h>
#include "zfstore.h"
#include "posix2-mds.h"

int zfstore_handle_length (char *);
int zfstore_make_handle (xlator_t *, char *, uuid_t, char *, size_t);
int32_t zfstore_resolve_inodeptr (xlator_t *,
                                  struct zfstore *, uuid_t,
                                  char *, struct iatt *, gf_boolean_t);
int32_t zfstore_resolve_inode (xlator_t *,
                               struct zfstore *,
                               uuid_t, struct iatt *, gf_boolean_t);
int32_t zfstore_resolve_entry (xlator_t *,
                               struct zfstore *, char *, const char *, uuid_t);
int32_t zfstore_handle_entry (xlator_t *, struct zfstore *,
                              char *, const char *, struct iatt *);
int32_t zfstore_create_dir_hashes (xlator_t *, char *);

int32_t zfstore_create_inode (xlator_t *,
                              struct zfstore *, char *, int32_t, mode_t);
int32_t zfstore_link_inode (xlator_t *,
                            struct zfstore *, char *, const char *, uuid_t);

#endif /* _ZFSTORE_HANDLE_H_ */
