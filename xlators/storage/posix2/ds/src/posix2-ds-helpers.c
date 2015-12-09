/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "posix2-ds-helpers.h"
#include "posix2.h"
#include "posix2-ds.h"
#include "posix2-mem-types.h"

int32_t
posix2_ds_stat_handle (xlator_t *this,
                       uuid_t gfid, char *path, struct iatt *stbuf)
{
        int32_t      ret      = 0;
        struct stat  lstatbuf      = {0,};

        ret = lstat (path, &lstatbuf);
        if (ret)
                return -1;

        if (stbuf) {
                iatt_from_stat (stbuf, &lstatbuf);
                gf_uuid_copy (stbuf->ia_gfid, gfid);
                posix2_fill_ino_from_gfid (this, stbuf);
        }
        return 0;
}

int32_t
posix2_ds_resolve_inodeptr (xlator_t *this,
                          uuid_t tgtuuid, char *ihandle,
                          struct iatt *stbuf)
{
        return posix2_ds_stat_handle (this, tgtuuid,
                                     ihandle, stbuf);
}
