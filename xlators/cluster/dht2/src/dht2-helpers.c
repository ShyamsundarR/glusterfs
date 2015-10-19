/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-helpers.c
 * This file contains common helper functions used across DHT2.
 */

#include "glusterfs.h"
#include "xlator.h"
#include "logging.h"
#include "statedump.h"
#include "dht2-helpers.h"

dht2_local_t *
dht2_local_init (call_frame_t *frame, loc_t *loc, fd_t *fd, glusterfs_fop_t fop)
{
        dht2_local_t *local = NULL;
        int           ret   = 0;

        local = mem_get0 (THIS->local_pool);
        if (!local)
                goto out;

        if (loc) {
                ret = loc_copy (&local->d2local_loc, loc);
                if (ret)
                        goto out;
        }

        local->d2local_fop = fop;

        frame->local = local;
out:
        if (ret) {
                if (local)
                        mem_put (local);
                local = NULL;
        }
        return local;
}

void
dht2_local_wipe (xlator_t *this, dht2_local_t *local)
{
        if (!local)
                return;

        loc_wipe (&local->d2local_loc);

        if (local->d2local_xattr_req)
                dict_unref (local->d2local_xattr_req);

        mem_put (local);
}

int
dht2_iatt_merge (struct iatt *to, struct iatt *from)
{
        if (!from || !to)
                return 0;

        to->ia_dev      = from->ia_dev;

        gf_uuid_copy (to->ia_gfid, from->ia_gfid);

        to->ia_ino      = from->ia_ino;
        to->ia_prot     = from->ia_prot;
        to->ia_type     = from->ia_type;
        to->ia_nlink    = from->ia_nlink;
        to->ia_rdev     = from->ia_rdev;
        to->ia_size    += from->ia_size;
        to->ia_blksize  = from->ia_blksize;
        to->ia_blocks  += from->ia_blocks;

        set_if_greater (to->ia_uid, from->ia_uid);
        set_if_greater (to->ia_gid, from->ia_gid);

        set_if_greater_time(to->ia_atime, to->ia_atime_nsec,
                            from->ia_atime, from->ia_atime_nsec);
        set_if_greater_time (to->ia_mtime, to->ia_mtime_nsec,
                             from->ia_mtime, from->ia_mtime_nsec);
        set_if_greater_time (to->ia_ctime, to->ia_ctime_nsec,
                             from->ia_ctime, from->ia_ctime_nsec);

        return 0;
}

uint32_t
gfid_to_bucket (uuid_t gfid)
{
        uint32_t  bucket = 0;

        bucket += (uint32_t)(gfid[16]) << 8;
        bucket += (uint32_t)(gfid[15]);

        return bucket;
}
