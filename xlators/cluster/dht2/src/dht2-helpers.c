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
#include "dht2-layout.h"

dht2_local_t *
dht2_local_init (call_frame_t *frame, dht2_conf_t *conf, loc_t *loc, fd_t *fd,
                 glusterfs_fop_t fop)
{
        dht2_local_t *local = NULL;
        int           ret   = 0;

        local = mem_get0 (conf->d2cnf_localpool);
        if (!local)
                goto err;

        if (loc) {
                ret = loc_copy (&local->d2local_loc, loc);
                if (ret)
                        goto free_local;
        }

        if (fd) {
                local->fd = fd_ref (fd);
                if (fd->inode) {
                        local->cached_subvol = dht2_find_subvol_for_gfid (conf,
                                               fd->inode->gfid, DHT2_DS_LAYOUT);
                }
        }

        local->d2local_fop = fop;
        frame->local = local;
        return local;
free_local:
        mem_put (local);
err:
        return NULL;
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

/* function extracts the TOP 2 bytes of the given UUID to return a bucket
 * number for use */
uint32_t
gfid_to_bucket (uuid_t gfid)
{
        uint32_t  bucket = 0;

        bucket += (uint32_t)(gfid[0]) << 8;
        bucket += (uint32_t)(gfid[1]);

        return bucket;
}

/* This function takes in a loc, and generates a nameless loc
 * that can be used for only inode related operations. The intention is to
 * cleanly mask any name information that the loc may carry so that other
 * xlators in the stack do not assume anything from the same and attempt any
 * functionality */
int
dht2_generate_nameless_loc (loc_t *dst, loc_t *src)
{
        int ret = -1;

        GF_VALIDATE_OR_GOTO ("xlator", dst, err);
        GF_VALIDATE_OR_GOTO ("xlator", src, err);

        gf_uuid_copy (dst->gfid, src->gfid);

        if (src->inode)
                dst->inode = inode_ref (src->inode);

        ret = 0;
err:
        return ret;
}

/**
 * There need not be much difference between the source loc here. so, this
 * routine just abstracts out loc_copy(). pass in a named loc (what you'd
 * get from mkdir, creat, etc..) and get a ref'd copy. Later, in case name
 * loc begs to be different that source, patch up things here..
 */
int
dht2_generate_name_loc (loc_t *dst, loc_t *src)
{
        return loc_copy (dst, src);
}

/**
 * prepare a "loc" that can be used by the icreate() file operation. This
 * loc structure is special with the parent being root
 */
int32_t
dht2_prepare_inode_loc (loc_t *dst, loc_t *src, uuid_t gfid)
{
        int ret = 0;

        GF_VALIDATE_OR_GOTO ("xlator", dst, err);
        GF_VALIDATE_OR_GOTO ("xlator", src, err);

        ret = dht2_generate_nameless_loc (dst, src);
        if (ret)
                goto err;

        gf_uuid_copy (dst->gfid, gfid);  /* use this gfid */
        return 0;

 err:
        return -1;
}
