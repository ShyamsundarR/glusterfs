/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-dir.c
 * DHT2 directory operations
 */

#include "dht2.h"

static uint32_t
dht2_gfid_to_bucket (uuid_t gfid)
{
        uint32_t bucket = 0;

        bucket += (((uint32_t)gfid[15]) << 8);
        bucket += (uint32_t)gfid[14];

        return bucket;
}

static xlator_t *
dht2_find_metdata_server_subvol (struct dht2_conf *conf, uuid_t gfid)
{
        int                        gotmds        = 0;
        uint32_t                   mdsbucket     = 0;
        struct dht2_subvol        *subvol        = NULL;
        struct dht2_layouthandler *layouthandler = NULL;

        layouthandler = conf->layouthandler;

        mdsbucket = dht2_gfid_to_bucket (gfid);
        for_each_mds_entry (subvol, conf) {
                if (layouthandler->layoutsearch
                                   (&subvol->layout, (void *)&mdsbucket)) {
                        gotmds = 1;
                        break;
                }
        }

        return gotmds ? subvol->this : NULL;
}

int32_t
dht2_lookup_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                 int32_t op_ret, int32_t op_errno, inode_t *inode,
                 struct iatt *buf, dict_t *xdata, struct iatt *postparent)
{
        STACK_UNWIND_STRICT (lookup, frame, op_ret,
                             op_errno, inode, buf, xdata, postparent);
        return 0;
}

int32_t
dht2_lookup (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xattr_req)
{
        int                        op_errno      = EINVAL;
        uuid_t                     gfid          = {0,};
        xlator_t                  *mdsubvol      = NULL;
        struct dht2_conf          *conf          = NULL;

        conf = this->private;

        /* named lookup() */
        if (!gf_uuid_is_null (loc->pargfid))
                gf_uuid_copy (gfid, loc->pargfid);
        else if (loc->parent)
                gf_uuid_copy (gfid, loc->parent->gfid);
        else {
                /* nameless lookup() */
                if (!gf_uuid_is_null (loc->gfid))
                        gf_uuid_copy (gfid, loc->gfid);
                else if (__is_root_gfid (loc->inode->gfid))
                        gf_uuid_copy (gfid, loc->inode->gfid);
                else {
                        op_errno = ENOTSUP;
                        goto unwind;
                }
        }

        mdsubvol = dht2_find_metdata_server_subvol (conf, gfid);
        if (!mdsubvol) {
                op_errno = EINVAL;
                goto unwind;
        }

        gf_msg (this->name, GF_LOG_INFO, 0, 0, "GFID [%s] -> MDS Subvolume: %s",
                uuid_utoa (gfid), mdsubvol->name);

        STACK_WIND (frame, dht2_lookup_cbk,
                    mdsubvol, mdsubvol->fops->lookup, loc, xattr_req);
        return 0;

 unwind:
        STACK_UNWIND_STRICT (lookup, frame,
                             -1, op_errno, NULL, NULL, NULL, NULL);
        return 0;
}
