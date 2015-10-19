/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-main.c
 * This file contains the xlator loading functions and options.
 */

#include "glusterfs.h"
#include "xlator.h"
#include "logging.h"
#include "statedump.h"
#include "dht2.h"
#include "dht2-helpers.h"
#include "dht2-layout.h"
#include "dht2-messages.h"


/* xlator FOP entry and cbk functions */
/* TODO: FOPs would possibly go into their own .c files based on grouping */

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

        local = dht2_local_init (frame, loc, NULL, GF_FOP_STAT);
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
        DHT2_STACK_UNWIND (lookup, frame, -1, op_errno, NULL, NULL, NULL, NULL);
bail:
        return 0;
}

int32_t dht2_lookup_cbk (call_frame_t *, void *, xlator_t *, int32_t , int32_t,
                inode_t *, struct iatt *, dict_t *, struct iatt *);

int
dht2_lookup_remote_inode (call_frame_t *frame, xlator_t *this, inode_t *inode,
                          struct iatt *buf, dict_t *xdata,
                          struct iatt *postparent)
{
        int32_t          op_errno = 0;
        xlator_t        *wind_subvol = NULL;
        dht2_conf_t     *conf = NULL;
        dht2_local_t    *local = NULL;
        loc_t            wind_loc = {0};

        GF_VALIDATE_OR_GOTO ("dht2", buf, err);

        conf = this->private;
        local = frame->local;

        /* ASSUMPTION: The gfid on a succesful find of the name is returned
         * as a part of the iatt buffer */
        if (gf_uuid_is_null (buf->ia_gfid)) {
                /* no GFID returned to search in remote subvolumes, error out */
                op_errno = EIO;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_INODE_NUMBER_MISSING,
                        "Missing GFID for name entry");
                goto err;
        }

        /* determine subvolume to wind lookup to */
        wind_subvol = dht2_find_subvol_for_gfid (conf, buf->ia_gfid,
                                                 DHT2_MDS_LAYOUT);
        if (!wind_subvol) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s",
                        uuid_utoa (buf->ia_gfid));
                goto err;
        }

        /* stash return values from name lookup */
        dht2_iatt_merge (&local->d2local_postparent_stbuf, postparent);
        local->d2local_postparent_stbuf_filled = _gf_true;
        /* ASSUMPTION: xdata from name entry is not stashed, can be, but
         * expectation is that all meta-data for a file is stored with the
         * inode/GFID and not the name. With the exception of the GFID itself */

        /* modify loc to contain the gfid of the file */
        gf_uuid_copy (local->d2local_loc.gfid, buf->ia_gfid);

        /* generate a GFID based nameless loc for further layers */
        if (!dht2_generate_nameless_loc (&wind_loc, &local->d2local_loc)) {
                goto err;
        }

        /* wind lookup to subvolume */
        STACK_WIND (frame, dht2_lookup_cbk,
                    wind_subvol, wind_subvol->fops->lookup,
                    &wind_loc, local->d2local_xattr_req);
        loc_wipe (&wind_loc);

        return 0;
err:
        DHT2_STACK_UNWIND (lookup, frame, -1, op_errno ? op_errno : errno,
                           inode, buf, xdata, postparent);
        loc_wipe (&wind_loc);
        return 0;

}

int32_t
dht2_lookup_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                 int32_t op_ret, int32_t op_errno, inode_t *inode,
                 struct iatt *buf, dict_t *xdata,
                 struct iatt *postparent)
{
        dht2_local_t    *local = NULL;

        GF_VALIDATE_OR_GOTO ("dht2", frame, bail);
        GF_VALIDATE_OR_GOTO ("dht2", this, err);
        GF_VALIDATE_OR_GOTO ("dht2", frame->local, err);
        GF_VALIDATE_OR_GOTO ("dht2", cookie, err);

        local = frame->local;

        /* success, unwind */
        if (!op_ret) {
                DHT2_STACK_UNWIND (lookup, frame, op_ret, op_errno, inode,
                                   buf, xdata,
                                   (local->d2local_postparent_stbuf_filled ?
                                    &local->d2local_postparent_stbuf :
                                    postparent));
                return 0;
        }

        /* error analysis and actions */
        if (op_ret) {
                /* EREMOTE denotes either name and inode are in different
                 * subvolumes, or rebalance is in progress */
                /* TODO: rebalance EREMOTE cases not handled yet */
                /* TODO: How to differentiate between rebalance and remote inode
                 * also needs to be worked out */
                if (op_errno == EREMOTE) {
                        dht2_lookup_remote_inode (frame, this, inode, buf,
                                                  xdata, postparent);
                        return 0;
                } else {
                        goto err;
                }
        }
err:
        DHT2_STACK_UNWIND (lookup, frame, op_ret, op_errno, inode, buf,
                           xdata, postparent);
bail:
        return 0;
}

int32_t
dht2_lookup (call_frame_t *frame, xlator_t *this,
             loc_t *loc, dict_t *xattr_req)
{
        dht2_conf_t     *conf = NULL;
        dht2_local_t    *local = NULL;
        int32_t          op_errno = 0;
        uuid_t           search_gfid = {0};
        xlator_t        *wind_subvol = NULL;
        loc_t           *wind_loc = NULL, tmp_loc = {0};

        VALIDATE_OR_GOTO (frame, bail);
        VALIDATE_OR_GOTO (this, err);
        VALIDATE_OR_GOTO (loc, err);
        VALIDATE_OR_GOTO (loc->inode, err);

        conf = this->private;
        if (!conf)
                goto err;

        local = dht2_local_init (frame, loc, NULL, GF_FOP_LOOKUP);
        if (!local) {
                op_errno = ENOMEM;
                goto err;
        }

        /* stash a reference to the request xattrs */
        if (xattr_req) {
                local->d2local_xattr_req = dict_ref (xattr_req);
        }

        /* A nameless lookup is when parent is unknown, when parent is known
         * we should revalidate the name against the inode, as rename or other
         * changes to the name could have occured. IOW, a lookup with a parent
         * needs a validation of name->inode and needs stat information back */
        if (gf_uuid_is_null (loc->pargfid) && !gf_uuid_is_null (loc->gfid)) {
                gf_uuid_copy (search_gfid, loc->gfid);

                /* generate wind loc based on GFID only */
                if (!dht2_generate_nameless_loc (&tmp_loc, loc)) {
                        op_errno = errno;
                        goto err;
                }

                wind_loc = &tmp_loc;
        } else if (__is_root_gfid (loc->inode->gfid)) {
                /* root inode is always a GFID based operation */
                gf_uuid_copy (search_gfid, loc->inode->gfid);

                /* generate wind loc based on GFID only */
                if (!dht2_generate_nameless_loc (&tmp_loc, loc)) {
                        op_errno = errno;
                        goto err;
                }

                wind_loc = &tmp_loc;
        } else {
                /* named lookup: parGFID+basename */
                /* TODO: trust the GFID on the parent inode first or the loc? */
                if (!gf_uuid_is_null (loc->pargfid))
                        gf_uuid_copy (search_gfid, loc->pargfid);
                else if (loc->parent)
                        gf_uuid_copy (search_gfid, loc->parent->gfid);
                else {
                        op_errno = EINVAL;
                        gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                                DHT2_MSG_INODE_NUMBER_MISSING,
                                "Missing GFID for name entry");
                        goto err;
                }

                wind_loc = loc;
        }

        /* determine subvolume to wind lookup to */
        wind_subvol = dht2_find_subvol_for_gfid (conf, search_gfid,
                                                 DHT2_MDS_LAYOUT);
        if (!wind_subvol) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s",
                        uuid_utoa (search_gfid));
                goto err;
        }

        /* wind lookup to subvolume */
        STACK_WIND (frame, dht2_lookup_cbk,
                    wind_subvol, wind_subvol->fops->lookup,
                    wind_loc, xattr_req);

        loc_wipe (&tmp_loc);
        return 0;
err:
        DHT2_STACK_UNWIND (lookup, frame, -1, op_errno, NULL, NULL, NULL, NULL);
        loc_wipe (&tmp_loc);
bail:
        return 0;
}

int32_t
mem_acct_init (xlator_t *this)
{
        int     ret = -1;

        GF_VALIDATE_OR_GOTO ("dht2", this, out);

        ret = xlator_mem_acct_init (this, gf_dht2_mt_end + 1);

        if (ret != 0) {
                gf_msg (this->name, GF_LOG_ERROR, errno, 0,
                        "Memory accounting init failed");
                return ret;
        }
out:
        return ret;
}

void
dht2_free_conf (dht2_conf_t **conf)
{
        dht2_conf_t *local_conf;

        if (!conf || !*conf)
                return;

        local_conf = *conf;

        GF_FREE (local_conf->d2cnf_xattr_base_name);
        GF_FREE (local_conf->d2cnf_eremote_reason_xattr_name);

        dht2_layout_return (local_conf->d2cnf_layout);

        GF_FREE (*conf);
        *conf = NULL;

        return;
}

int32_t
dht2_init (xlator_t *this)
{
        int          ret = -1;
        dht2_conf_t *conf = NULL;

        if (!this->parents) {
                gf_log (this->name, GF_LOG_WARNING,
                        "dangling volume. check volfile ");
                goto out;
        }

        conf = GF_CALLOC (1, sizeof (*conf), gf_dht2_mt_dht2_conf_t);
        if (!conf)
                goto out;


        GF_OPTION_INIT("dht2-data-count", conf->d2cnf_data_count, int32, out);
        GF_OPTION_INIT("dht2-mds-count", conf->d2cnf_mds_count, int32, out);

        if (!conf->d2cnf_data_count || !conf->d2cnf_mds_count) {
                gf_msg (this->name, GF_LOG_CRITICAL, EINVAL, 0,
                        "Invalid mds or data count specified"
                        " (data - %d, mds - %d)",
                        conf->d2cnf_data_count, conf->d2cnf_mds_count);
                goto out;
        }

        gf_msg (this->name, GF_LOG_INFO, 0, 0,
                        "mds and data count specified"
                        " (data - %d, mds - %d)",
                        conf->d2cnf_data_count, conf->d2cnf_mds_count);

        GF_OPTION_INIT ("dht2-xattr-base-name", conf->d2cnf_xattr_base_name,
                        str, out);
        gf_asprintf (&conf->d2cnf_eremote_reason_xattr_name,
                     "%s."DHT_EREMOTE_XATTR_STR, conf->d2cnf_xattr_base_name);

        ret = dht2_layout_fetch (this, conf);
        if (ret) {
                gf_msg (this->name, GF_LOG_CRITICAL, errno, 0,
                        "Unable to fetch layout information.");
                goto out;
        }

        this->private = conf;

        ret = 0;

out:
        if (ret && conf) {
                dht2_free_conf (&conf);
        }

        return ret;
}

void
dht2_fini (xlator_t *this)
{
        dht2_conf_t *conf = NULL;

        GF_VALIDATE_OR_GOTO ("dht2", this, out);

        conf = this->private;
        this->private = NULL;
        if (conf) {
                dht2_free_conf (&conf);
        }
out:
        return;
}

class_methods_t class_methods = {
        .init           = dht2_init,
        .fini           = dht2_fini,
};

struct xlator_fops fops = {
};

struct xlator_cbks cbks = {
};

/*
struct xlator_dumpops dumpops = {
};
*/

struct volume_options options[] = {
        { .key   = {"dht2-data-count"},
          .type  = GF_OPTION_TYPE_INT,
          .default_value = "0",
          .description = "Specifies the number of DHT2 subvols to use as data"
                         "volumes."
        },
        { .key   = {"dht2-mds-count"},
          .type  = GF_OPTION_TYPE_INT,
          .default_value = "0",
          .description = "Specifies the number of DHT2 subvols to use as"
                         " meta-data volumes."
        },
        { .key = {"dht2-xattr-base-name"},
          .type = GF_OPTION_TYPE_STR,
          .default_value = "trusted.glusterfs.dht2",
          .description = "Base for extended attributes used by DHT2 "
          "translator instance, to avoid conflicts with others above or "
          "below it."
        },
        { .key   = {NULL} },
};
