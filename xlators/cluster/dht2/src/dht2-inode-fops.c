/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-inode-fops.c
 * This file contains the DHT2 inode based FOPs.
 */

#include "glusterfs.h"
#include "xlator.h"
#include "logging.h"
#include "statedump.h"
#include "dht2.h"
#include "dht2-helpers.h"
#include "dht2-layout.h"
#include "dht2-messages.h"

/* xlator inode FOP entry and cbk functions */
/* TODO: Functions here would move to a code generation based template as
 * they are pretty much common in structure */

int32_t
dht2_open_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
               int32_t op_ret, int32_t op_errno, fd_t *fd, dict_t *xdata)
{
        VALIDATE_OR_GOTO (frame, bail);

        /* TODO: Rebalance EREMOTE or equivalent errors need to be handled */
        DHT2_STACK_UNWIND (open, frame, op_ret, op_errno, fd, xdata);
bail:
        return 0;
}

/* Sample implementation for open, written to see if we could leverage part
 * of this in the create transaction, leaving it behind, as it would get into
 * code-gen parts as it closely follows stat implementation */
int32_t
dht2_open (call_frame_t *frame, xlator_t *this, loc_t *loc, int32_t flags,
           fd_t *fd, dict_t *xdata)
{
        dht2_conf_t     *conf = NULL;
        dht2_local_t    *local = NULL;
        int32_t          op_errno = 0;
        xlator_t        *wind_subvol = NULL;

        VALIDATE_OR_GOTO (frame, bail);
        VALIDATE_OR_GOTO (this, err);
        VALIDATE_OR_GOTO (loc, err);
        VALIDATE_OR_GOTO (loc->inode, err);

        conf = this->private;
        if (!conf)
                goto err;

        local = dht2_local_init (frame, conf, loc, NULL, GF_FOP_OPEN);
        if (!local) {
                op_errno = ENOMEM;
                goto err;
        }

        if (gf_uuid_is_null (loc->inode->gfid)) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_MISSING_GFID_IN_INODE,
                        "Missing GFID for inode %p",
                        loc->inode);
        }

        /* determine subvolume to wind stat to */
        wind_subvol = dht2_find_subvol_for_gfid (conf, loc->inode->gfid,
                                                 DHT2_MDS_LAYOUT);
        if (!wind_subvol) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s",
                        uuid_utoa (loc->inode->gfid));
                goto err;
        }

        /* wind to subvolume */
        STACK_WIND (frame, dht2_open_cbk,
                    wind_subvol, wind_subvol->fops->open,
                    loc, flags, fd, xdata);

        return 0;
err:
        DHT2_STACK_UNWIND (open, frame, -1, op_errno, NULL, NULL);
bail:
        return 0;
}

typedef int32_t (*fop_setattr_cbk_t) (call_frame_t *frame,
                                      void *cookie,
                                      xlator_t *this,
                                      int32_t op_ret,
                                      int32_t op_errno,
                                      struct iatt *preop_stbuf,
                                      struct iatt *postop_stbuf, dict_t *xdata);
int32_t
dht2_setattr_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                  int32_t op_ret, int32_t op_errno, struct iatt *preop_stbuf,
                  struct iatt *postop_stbuf, dict_t *xdata)
{
        VALIDATE_OR_GOTO (frame, bail);

        /* TODO: Rebalance EREMOTE or equivalent errors need to be handled */
        DHT2_STACK_UNWIND (setattr, frame, op_ret, op_errno, preop_stbuf,
                           postop_stbuf, xdata);
bail:
        return 0;
}

int32_t
dht2_setattr (call_frame_t *frame, xlator_t *this, loc_t *loc,
              struct iatt *stbuf, int32_t valid, dict_t *xdata)
{
        dht2_conf_t     *conf = NULL;
        dht2_local_t    *local = NULL;
        int32_t          op_errno = 0;
        xlator_t        *wind_subvol = NULL;

        VALIDATE_OR_GOTO (frame, bail);
        VALIDATE_OR_GOTO (this, err);
        VALIDATE_OR_GOTO (loc, err);
        VALIDATE_OR_GOTO (loc->inode, err);

        conf = this->private;
        if (!conf)
                goto err;

        local = dht2_local_init (frame, conf, loc, NULL, GF_FOP_SETATTR);
        if (!local) {
                op_errno = ENOMEM;
                goto err;
        }

        if (gf_uuid_is_null (loc->inode->gfid)) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_MISSING_GFID_IN_INODE,
                        "Missing GFID for inode %p",
                        loc->inode);
        }

        /* determine subvolume to wind to */
        wind_subvol = dht2_find_subvol_for_gfid (conf, loc->inode->gfid,
                                                 DHT2_MDS_LAYOUT);
        if (!wind_subvol) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s",
                        uuid_utoa (loc->inode->gfid));
                goto err;
        }

        /* wind to subvolume */
        STACK_WIND (frame, dht2_setattr_cbk,
                    wind_subvol, wind_subvol->fops->setattr,
                    loc, stbuf, valid, xdata);

        return 0;
err:
        DHT2_STACK_UNWIND (setattr, frame, -1, op_errno, NULL, NULL, NULL);
bail:
        return 0;
}

int32_t
dht2_stat_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
               int32_t op_ret, int32_t op_errno, struct iatt *buf,
               dict_t *xdata)
{
        VALIDATE_OR_GOTO (frame, bail);

        /* TODO: Rebalance EREMOTE or equivalent errors need to be handled */
        DHT2_STACK_UNWIND (stat, frame, op_ret, op_errno, buf, xdata);
bail:
        return 0;
}

int32_t
dht2_stat (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xattr_req)
{
        dht2_conf_t     *conf = NULL;
        dht2_local_t    *local = NULL;
        int32_t          op_errno = 0;
        xlator_t        *wind_subvol = NULL;

        VALIDATE_OR_GOTO (frame, bail);
        VALIDATE_OR_GOTO (this, err);
        VALIDATE_OR_GOTO (loc, err);
        VALIDATE_OR_GOTO (loc->inode, err);

        conf = this->private;
        if (!conf)
                goto err;

        local = dht2_local_init (frame, conf, loc, NULL, GF_FOP_STAT);
        if (!local) {
                op_errno = ENOMEM;
                goto err;
        }

        if (gf_uuid_is_null (loc->inode->gfid)) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_MISSING_GFID_IN_INODE,
                        "Missing GFID for inode %p",
                        loc->inode);
        }

        /* determine subvolume to wind stat to */
        wind_subvol = dht2_find_subvol_for_gfid (conf, loc->inode->gfid,
                                                 DHT2_MDS_LAYOUT);
        if (!wind_subvol) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s",
                        uuid_utoa (loc->inode->gfid));
                goto err;
        }

        /* wind stat to subvolume */
        STACK_WIND (frame, dht2_stat_cbk,
                    wind_subvol, wind_subvol->fops->stat,
                    loc, xattr_req);

        return 0;
err:
        DHT2_STACK_UNWIND (stat, frame, -1, op_errno, NULL, NULL);
bail:
        return 0;
}

int32_t
dht2_flush_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                int32_t op_ret, int32_t op_errno, dict_t *xdata)
{
        VALIDATE_OR_GOTO (frame, bail);

        /* TODO: Rebalance EREMOTE or equivalent errors need to be handled */
        DHT2_STACK_UNWIND (flush, frame, op_ret, op_errno, xdata);
bail:
        return 0;
}

int32_t
dht2_flush (call_frame_t *frame, xlator_t *this, fd_t *fd, dict_t *xdata)
{
        dht2_conf_t     *conf = NULL;
        dht2_local_t    *local = NULL;
        int32_t          op_errno = 0;
        xlator_t        *wind_subvol = NULL;

        VALIDATE_OR_GOTO (frame, bail);
        VALIDATE_OR_GOTO (this, err);
        VALIDATE_OR_GOTO (fd, err);
        VALIDATE_OR_GOTO (fd->inode, err);

        conf = this->private;
        if (!conf)
                goto err;

        local = dht2_local_init (frame, conf, NULL, fd, GF_FOP_FLUSH);
        if (!local) {
                op_errno = ENOMEM;
                goto err;
        }

        if (gf_uuid_is_null (fd->inode->gfid)) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_MISSING_GFID_IN_INODE,
                        "Missing GFID for inode %p",
                        fd->inode);
        }

        /* determine subvolume to wind to */
        wind_subvol = dht2_find_subvol_for_gfid (conf, fd->inode->gfid,
                                                 DHT2_MDS_LAYOUT);
        if (!wind_subvol) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s",
                        uuid_utoa (fd->inode->gfid));
                goto err;
        }

        /* wind to subvolume */
        STACK_WIND (frame, dht2_flush_cbk, wind_subvol,
                    wind_subvol->fops->flush, fd, xdata);

        return 0;
err:
        DHT2_STACK_UNWIND (flush, frame, -1, op_errno, NULL);
bail:
        return 0;
}
