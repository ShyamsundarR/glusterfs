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
dht2_create_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                 int32_t op_ret, int32_t op_errno, fd_t *fd, inode_t *inode,
                 struct iatt *buf, struct iatt *preparent,
                 struct iatt *postparent, dict_t *xdata)
{
        GF_VALIDATE_OR_GOTO ("dht2", frame, bail);

        /* on success, or unhandled failures, unwind */
        if (!op_ret || (op_ret != 0 && op_errno != EREMOTE)) {
                DHT2_STACK_UNWIND (create, frame, op_ret, op_errno, fd, inode,
                                   buf, preparent, postparent, xdata);
                return 0;
        }

        /* TODO: We need to handle the EREMOTE case, so for now the
         * unhandled part is just maintained as an sepcial exception */
        /* (op_errno == EREMOTE)) */
        DHT2_STACK_UNWIND (create, frame, op_ret, op_errno, fd, inode,
                           buf, preparent, postparent, xdata);
bail:
        return 0;
}

/* DHT2 Create:
 * Create has 4 cases that it can end up with based on what the creat flags are,
 * - Success cases
 *  1: Created and opened (O_CREAT | O_EXCL)
 *  2: Exists and opened (O_CREAT & !O_EXCL)
 * - Failed cases
 *  3: EEXIST, where flags had O_EXCL
 *  4: EREMOTE, where flags had !O_EXCL
 *  - EOTHER, any other error, which we do not handle
 *
 * 1/2/3: We can just unwind with the results.
 * 4: This marks an interesting case where races need to be handled. We get the
 * GFID of the file that exists, now this needs to be opened (not created) at
 * the inode location. Before this open completes, an unlink can remove the file
 * in such cases to adhere to POSIX creat (or open with O_CREAT flags) we need
 * to run the create again.
 * This problem of EREMOTE is not handled yet. Further, this transaction, of
 * finding the name to exist and the race between this event and the open at
 * the remote location, will be handled by a server side DHT2-orchestrator
 * xlator.
 */
int32_t
dht2_create (call_frame_t *frame, xlator_t *this, loc_t *loc, int32_t flags,
             mode_t mode, mode_t umask, fd_t *fd, dict_t *xdata)
{
        int              ret = -1;
        dht2_conf_t     *conf = NULL;
        dht2_local_t    *local = NULL;
        int32_t          op_errno = 0;
        xlator_t        *wind_subvol = NULL;
        uuid_t           gfid_req;

        VALIDATE_OR_GOTO (frame, bail);
        VALIDATE_OR_GOTO (this, err);
        VALIDATE_OR_GOTO (loc, err);
        VALIDATE_OR_GOTO (loc->parent, err);

        conf = this->private;
        if (!conf)
                goto err;

        if (gf_uuid_is_null (loc->parent->gfid)) {
                op_errno = EINVAL;
                goto err;
        }

        local = dht2_local_init (frame, conf, loc, NULL, GF_FOP_CREATE);
        if (!local) {
                op_errno = ENOMEM;
                goto err;
        }

        /* determine subvolume to wind to */
        wind_subvol = dht2_find_subvol_for_gfid (conf, loc->parent->gfid,
                                                 DHT2_MDS_LAYOUT);
        if (!wind_subvol) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s",
                        uuid_utoa (loc->parent->gfid));
                goto err;
        }

        if (xdata) {
                local->d2local_xattr_req = dict_ref (xdata);
        } else {
                local->d2local_xattr_req = dict_new ();
        }
        if (local->d2local_xattr_req == NULL) {
                op_errno = ENOMEM;
                goto err;
        }

        /* fill in gfid-req for entry creation, to colocate file inode with the
         * parent */
        dht2_generate_uuid_with_constraint (gfid_req, loc->parent->gfid);
        gf_msg (this->name, GF_LOG_DEBUG, 0, 0,
                "UUID: [%s] winding to subvol: [%s]",
                uuid_utoa (gfid_req), wind_subvol->name);

        /* TODO: gfid-req is something other parts of the code use as well. Need
         * to understand how this would work, when we overwrite the same with
         * our requested GFID */
        /* Assumption: POSIX2 would look at this xattr to use the same for the
         * to be created object GFID. This is set from the client as the client
         * has the knowledge of the layouts */
        ret = dict_set_static_bin (local->d2local_xattr_req, "gfid-req",
                                   gfid_req, 16);
        if (ret) {
                errno = ENOMEM;
                goto err;
        }

        /* wind call to subvolume */
        STACK_WIND (frame, dht2_create_cbk,
                    wind_subvol, wind_subvol->fops->create,
                    loc, flags, mode, umask, fd, local->d2local_xattr_req);

        return 0;
err:
        DHT2_STACK_UNWIND (create, frame, -1, op_errno, NULL, NULL, NULL,
                           NULL, NULL, NULL);
bail:
        return 0;
}

int32_t
dht2_namelink_cbk (call_frame_t *frame, void *cookie,
                   xlator_t *this, int32_t op_ret, int32_t op_errno,
                   struct iatt *prebuf, struct iatt *postbuf, dict_t *xdata)
{
        dht2_local_t *local = NULL;

        if (op_ret < 0)
                goto unwinderr;

        local = frame->local;

        DHT2_STACK_UNWIND (mkdir, frame,
                           op_ret, op_errno, local->d2local_loc.inode,
                           &local->d2local_stbuf, prebuf, postbuf, xdata);
        return 0;

 unwinderr:
        DHT2_STACK_UNWIND (mkdir, frame,
                           op_ret, op_errno, NULL, NULL, NULL, NULL, NULL);
        return 0;
}

/**
 * Tie knot between an inode and its name also known as "namelink". A namelink()
 * fop is sent to the parent inode metadata server, thereby "linking" the
 * inode [gfid] with a name entry.
 */
static int32_t
dht2_do_namelink (call_frame_t *frame,
                  xlator_t *this, dht2_conf_t *conf, dht2_local_t *local)
{
        int32_t ret = 0;
        loc_t *loc = NULL;
        loc_t wind_loc = {0, };
        xlator_t *wind_subvol = NULL;

        loc = &local->d2local_loc;

        wind_subvol = dht2_find_subvol_for_gfid (conf, loc->parent->gfid,
                                                 DHT2_MDS_LAYOUT);
        if (!wind_subvol) {
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, EINVAL,
                        DHT2_MSG_FIND_SUBVOL_ERROR, "Unable to find subvolume "
                        "for GFID %s", loc->parent->gfid);
                goto error_return;
        }

        ret = dht2_generate_name_loc (&wind_loc, loc);
        if (ret)
                goto error_return;

        STACK_WIND (frame, dht2_namelink_cbk,
                    wind_subvol, wind_subvol->fops->namelink,
                    &wind_loc, local->d2local_xattr_req);
        loc_wipe (&wind_loc);
        return 0;

 error_return:
        return -1;
}

int32_t
dht2_directory_inode_cbk (call_frame_t *frame,
                          void *cookie, xlator_t *this,
                          int32_t op_ret, int32_t op_errno,
                          inode_t *inode, struct iatt *buf, dict_t *xdata)
{
        int32_t ret = 0;
        dht2_conf_t *conf = NULL;
        dht2_local_t *local = NULL;

        if (op_ret < 0)
                goto unwinderr;

        conf = this->private;

        local = frame->local;
        local->d2local_stbuf = *buf;

        ret = dht2_do_namelink (frame, this, conf, local);
        if (ret)
                goto unwinderr;
        return 0;

 unwinderr:
        DHT2_STACK_UNWIND (mkdir, frame,
                           op_ret, op_errno, NULL, NULL, NULL, NULL, NULL);
        return 0;
}

static int32_t
dht2_do_mkdir (call_frame_t *frame, xlator_t *this,
               dht2_conf_t *conf, mode_t mode, mode_t umask, dict_t *xdata)
{
        int32_t       ret         = 0;
        int32_t       op_errno    = EINVAL;
        uuid_t        gfid        = {0,};
        void         *uuidreq     = NULL;
        loc_t         wind_loc    = {0,};
        dht2_local_t *local       = NULL;
        xlator_t     *wind_subvol = NULL;

        local = frame->local;

        ret = dict_get_ptr (local->d2local_xattr_req, "gfid-req", &uuidreq);
        if (ret)
                goto error_return;
        gf_uuid_copy (gfid, uuidreq);

        /**
         * Use the GFID as-it-is as a source for randomness for MDS selection
         */
        wind_subvol = dht2_find_subvol_for_gfid (conf, gfid, DHT2_MDS_LAYOUT);
        if (!wind_subvol) {
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s", gfid);
                goto error_return;
        }

        ret = dht2_prepare_inode_loc (&wind_loc, &local->d2local_loc, gfid);
        if (ret)
                goto error_return;

        /* directory inode creation request */
        STACK_WIND (frame, dht2_directory_inode_cbk,
                    wind_subvol, wind_subvol->fops->icreate,
                    &wind_loc, S_IFDIR | mode, local->d2local_xattr_req);
        loc_wipe (&wind_loc);
        return 0;

 error_return:
        return -op_errno;
}

/**
 * Directories do not follow colocation stratergy which is heavily relied
 * by other dentry opetations such as creat/mknod. Doing this would place
 * the entire filesystem tree on one MDS -- fatal.
 *
 * Therefore, directory creations are treated specially: the inode of the
 * directory is randomly placed in one of the MDS with the name "colocated"
 * with it's parent pointing to it's inode remote MDS. lookup() is handled
 * by catching EREMOTE and re-winding to the inode MDS, c.f. dht2_lookup().
 *
 * NOTE: As of now directory inode/name creation is driven by the client.
 *       Later on, this should be taken care by the dht2 server component
 *       [server component on the "name" MDS to be specific] and provide
 *       crash consistency semantics, rollbacks, etc..
 */
int32_t
dht2_mkdir (call_frame_t *frame, xlator_t *this,
            loc_t *loc, mode_t mode, mode_t umask, dict_t *xdata)
{
        int32_t       ret         = 0;
        dht2_conf_t  *conf        = NULL;
        dht2_local_t *local       = NULL;
        int32_t       op_errno    = EINVAL;

        VALIDATE_OR_GOTO (frame, err);
        VALIDATE_OR_GOTO (this, err);
        VALIDATE_OR_GOTO (loc, err);
        VALIDATE_OR_GOTO (loc->parent, err);

        conf = this->private;
        if (!conf)
                goto err;
        if (gf_uuid_is_null (loc->parent->gfid))
                goto err;

        local = dht2_local_init (frame, conf, loc, NULL, GF_FOP_MKDIR);
        if (!local) {
                op_errno = ENOMEM;
                goto err;
        }

        local->d2local_xattr_req = dict_ref (xdata);
        if (!local->d2local_xattr_req) {
                op_errno = ENOMEM; /* should it? it just a ref */
                goto err;
        }

        ret = dht2_do_mkdir (frame, this, conf, mode, umask, xdata);
        if (ret < 0) {
                op_errno = -ret;
                goto err;
        }
        return 0;

 err:
        DHT2_STACK_UNWIND (mkdir, frame,
                           -1, op_errno, NULL, NULL, NULL, NULL, NULL);
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
        if (dht2_generate_nameless_loc (&wind_loc, &local->d2local_loc)) {
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

        local = dht2_local_init (frame, conf, loc, NULL, GF_FOP_LOOKUP);
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
                if (dht2_generate_nameless_loc (&tmp_loc, loc)) {
                        op_errno = errno;
                        goto err;
                }

                wind_loc = &tmp_loc;
        } else if (__is_root_gfid (loc->inode->gfid)) {
                /* root inode is always a GFID based operation */
                gf_uuid_copy (search_gfid, loc->inode->gfid);

                /* generate wind loc based on GFID only */
                if (dht2_generate_nameless_loc (&tmp_loc, loc)) {
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
dht2_ds_open_cbk(
        call_frame_t *frame, void *cookie, xlator_t *this,
	int32_t op_ret, int32_t op_errno,
        fd_t * fd,
	dict_t * xdata)
{

        VALIDATE_OR_GOTO (frame, bail);

        /* TODO: Rebalance EREMOTE or equivalent errors need to be handled */

        DHT2_STACK_UNWIND (open, frame, op_ret, op_errno,
                           fd, xdata);
bail:
        return 0;
}

int32_t
dht2_open_cbk(
        call_frame_t *frame, void *cookie, xlator_t *this,
	int32_t op_ret, int32_t op_errno,
        fd_t * fd,
	dict_t * xdata)
{
        dht2_conf_t      *conf           = NULL;
        dht2_local_t     *local          = NULL;
        xlator_t         *ds_wind_subvol = NULL;
        uuid_t            gfid           = {0,};
        loc_t            *wind_loc       = NULL;

        VALIDATE_OR_GOTO (frame, bail);
        GF_VALIDATE_OR_GOTO ("dht2", this, err);
        GF_VALIDATE_OR_GOTO ("dht2", frame->local, err);
        GF_VALIDATE_OR_GOTO ("dht2", this->private, err);

        if (op_ret == -1)
                goto err;

        /* Open FD on DS side. */
        conf = this->private;
        local = frame->local;
        wind_loc = &local->d2local_loc;

        if (!gf_uuid_is_null (wind_loc->gfid))
                gf_uuid_copy (gfid, wind_loc->gfid);
        else if (wind_loc->inode && !gf_uuid_is_null (wind_loc->inode->gfid))
                gf_uuid_copy (gfid, wind_loc->inode->gfid);
        else
                goto err;

        ds_wind_subvol = dht2_find_subvol_for_gfid (conf, gfid, DHT2_DS_LAYOUT);
        if (!ds_wind_subvol) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s", gfid);
                goto err;
        }

        /* wind open to DS subvolume */
        STACK_WIND (frame, dht2_ds_open_cbk,
                    ds_wind_subvol, ds_wind_subvol->fops->open,
                    wind_loc, fd->flags, fd, xdata);

        return 0;
err:
        op_errno = (op_errno == -1) ? errno : op_errno;
        DHT2_STACK_UNWIND (open, frame, -1, op_errno,
                           fd, xdata);
bail:
        return 0;
}

int32_t
dht2_open (
	call_frame_t *frame, xlator_t *this,
	loc_t * loc,
	int32_t flags,
	fd_t * fd,
	dict_t * xdata)
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

        /* TODO: GF_FOP_STAT should be generated, we are not using it at
         * present, so letting it pass for now as a constant */
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

        /* determine subvolume to wind open to */
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

        /* wind open to subvolume */
        STACK_WIND (frame, dht2_open_cbk,
                    wind_subvol, wind_subvol->fops->open,
                    loc, flags, fd, xdata);

        return 0;
err:
        DHT2_STACK_UNWIND (open, frame, -1, op_errno,
                           NULL, NULL);
bail:
        return 0;
}

int
dht2_writev_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                 int op_ret, int op_errno, struct iatt *prebuf,
                 struct iatt *postbuf, dict_t *xdata)
{

        call_frame_t  *prev = NULL;

        GF_VALIDATE_OR_GOTO ("dht2", frame, bail);
        prev = cookie;

        if (op_ret == -1) {
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_WRITEV_ERROR,
                        "subvolume %s returned -1 (%s)",
                        prev->this->name, strerror (op_errno));
                goto out;
        }

out:
        DHT2_STACK_UNWIND (writev, frame, op_ret, op_errno, prebuf, postbuf,
                           xdata);
bail:
        return 0;
}

int
dht2_writev (call_frame_t *frame, xlator_t *this, fd_t *fd,
            struct iovec *vector, int count, off_t off, uint32_t flags,
            struct iobref *iobref, dict_t *xdata)
{
        dht2_conf_t     *conf        = NULL;
        xlator_t        *wind_subvol = NULL;
        int              op_errno    = -1;
        dht2_local_t    *local       = NULL;

        VALIDATE_OR_GOTO (frame, err);
        VALIDATE_OR_GOTO (this, err);
        VALIDATE_OR_GOTO (this->private, err);
        VALIDATE_OR_GOTO (fd, err);

        conf = this->private;

        /* LOOKUP */
        local = dht2_local_init (frame, conf, NULL, fd, GF_FOP_WRITE);
        if (!local) {
                op_errno = ENOMEM;
                goto err;
        }

        wind_subvol = local->cached_subvol;
        if (!wind_subvol) {
                op_errno = EINVAL;
                gf_msg (DHT2_MSG_DOM, GF_LOG_ERROR, op_errno,
                        DHT2_MSG_FIND_SUBVOL_ERROR,
                        "Unable to find subvolume for GFID %s",
                        fd->inode?uuid_utoa(fd->inode->gfid):"Inode is NULL");
                goto err;
        }

        STACK_WIND (frame, dht2_writev_cbk,
                    wind_subvol, wind_subvol->fops->writev,
                    fd, vector, count, off, flags, iobref, xdata);
        return 0;

err:
        op_errno = (op_errno == -1) ? errno : op_errno;
        DHT2_STACK_UNWIND (writev, frame, -1, op_errno, NULL, NULL, NULL);

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

        GF_FREE (local_conf->d2cnf_eremote_reason_xattr_name);

        dht2_layout_return (local_conf->d2cnf_layout);

        /* TODO: Is it safe to destroy the mem-pools now, no pending FOPs? */
        mem_pool_destroy (local_conf->d2cnf_localpool);

        GF_FREE (*conf);
        *conf = NULL;

        return;
}

int32_t
dht2_init (xlator_t *this)
{
        dht2_conf_t *conf = NULL;

        if (!this->parents) {
                gf_log (this->name, GF_LOG_WARNING,
                        "dangling volume. check volfile ");
                goto err;
        }

        conf = GF_CALLOC (1, sizeof (*conf), gf_dht2_mt_dht2_conf_t);
        if (!conf)
                goto err;

        GF_OPTION_INIT("dht2-data-count", conf->d2cnf_data_count, int32, err);
        GF_OPTION_INIT("dht2-mds-count", conf->d2cnf_mds_count, int32, err);

        if (!conf->d2cnf_data_count || !conf->d2cnf_mds_count) {
                gf_msg (this->name, GF_LOG_CRITICAL, EINVAL, 0,
                        "Invalid mds or data count specified"
                        " (data - %d, mds - %d)",
                        conf->d2cnf_data_count, conf->d2cnf_mds_count);
                goto err;
        }

        gf_msg (this->name, GF_LOG_INFO, 0, 0,
                        "mds and data count specified"
                        " (data - %d, mds - %d)",
                        conf->d2cnf_data_count, conf->d2cnf_mds_count);

        GF_OPTION_INIT ("dht2-xattr-base-name", conf->d2cnf_xattr_base_name,
                        str, err);
        gf_asprintf (&conf->d2cnf_eremote_reason_xattr_name,
                     "%s."DHT_EREMOTE_XATTR_STR, conf->d2cnf_xattr_base_name);

        conf->d2cnf_layout = dht2_layout_fetch (this, conf);
        if (!conf->d2cnf_layout) {
                gf_msg (this->name, GF_LOG_CRITICAL, errno, 0,
                        "Unable to fetch layout information.");
                goto err;
        }

        conf->d2cnf_localpool = mem_pool_new (dht2_local_t, 512);
        if (!conf->d2cnf_localpool)
                goto err;

        this->private = conf;

        return 0;
err:
        dht2_free_conf (&conf);

        return -1;
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
/*      .reconfigure,
        .notify */
};

struct xlator_fops fops = {
        .lookup         = dht2_lookup,
        .stat           = dht2_stat,
        .fstat          = dht2_fstat,
        .truncate       = dht2_truncate,
        .ftruncate      = dht2_ftruncate,
        .access         = dht2_access,
        .readlink       = dht2_readlink,
/*        .mknod, */
        .mkdir          = dht2_mkdir,
/*        .unlink, */
/*        .rmdir, */
/*        .symlink        = dht2_symlink, */
/*        .rename; */
/*        .link; */
        .create         = dht2_create,
        .open           = dht2_open,
/*        .readv, */
        .writev         = dht2_writev,
        .flush          = dht2_flush,
/*        .fsync, */
/*        .opendir, */
/*        .readdir, */
        .readdirp       = dht2_readdirp,
/*        .fsyncdir, */
/*        .statfs, */
        .setxattr       = dht2_setxattr,
        .getxattr       = dht2_getxattr,
        .fsetxattr      = dht2_fsetxattr,
        .fgetxattr      = dht2_fgetxattr,
        .removexattr    = dht2_removexattr,
        .fremovexattr   = dht2_fremovexattr,
        .lk             = dht2_lk,
        .inodelk        = dht2_inodelk,
        .finodelk       = dht2_finodelk,
        .entrylk        = dht2_entrylk,
        .fentrylk       = dht2_fentrylk,
        .rchecksum      = dht2_rchecksum,
        .xattrop        = dht2_xattrop,
        .fxattrop       = dht2_fxattrop,
        .setattr        = dht2_setattr,
        .fsetattr       = dht2_fsetattr,
        .getspec        = dht2_getspec,
        .fallocate      = dht2_fallocate,
        .discard        = dht2_discard,
        .zerofill       = dht2_zerofill,
        .ipc            = dht2_ipc
};

struct xlator_cbks cbks = {
/*        .forget,
        .release,
        .releasedir,
        .invalidate
        .client_destroy,
        .client_disconnect,
        .ictxmerge*/
};

/*
struct xlator_dumpops dumpops = {
        .priv,
        .inode,
        .fd,
        .inodectx,
        .fdctx,
        .priv_to_dict,
        .node_to_dict,
        .fd_to_dict,
        .inodectx_to_dict,
        .fdctx_to_dict,
        .history
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
