#!/usr/bin/python

import sys
# TODO: In a build environment this should come from some form of ENV variable
# I would assume, hardcoding this relative path may not be the best idea
sys.path.append('../../../../libglusterfs/src')
from generator import ops, fop_subs, cbk_subs, generate

INODE_OP_CBK_TEMPLATE = """
int32_t
dht2_@NAME@_cbk (
        call_frame_t *frame, void *cookie, xlator_t *this,
	int32_t op_ret, int32_t op_errno,
        @LONG_ARGS@)
{
        VALIDATE_OR_GOTO (frame, bail);

        /* TODO: Rebalance EREMOTE or equivalent errors need to be handled */
        DHT2_STACK_UNWIND (@NAME@, frame, op_ret, op_errno,
                           @SHORT_ARGS@);
bail:
        return 0;
}
"""

INODE_OP_FOP_TEMPLATE = """
int32_t
dht2_@NAME@ (
	call_frame_t *frame, xlator_t *this,
	@LONG_ARGS@)
{
        dht2_conf_t     *conf = NULL;
        dht2_local_t    *local = NULL;
        int32_t          op_errno = 0;
        xlator_t        *wind_subvol = NULL;

        VALIDATE_OR_GOTO (frame, bail);
        VALIDATE_OR_GOTO (this, err);
        /* Code gen assumption that this variable is always named loc */
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

        /* determine subvolume to wind @NAME@ to */
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

        /* wind @NAME@ to subvolume */
        STACK_WIND (frame, dht2_@NAME@_cbk,
                    wind_subvol, wind_subvol->fops->@NAME@,
                    @SHORT_ARGS@);

        return 0;
err:
        DHT2_STACK_UNWIND (@NAME@, frame, -1, op_errno,
                           @CBK_ERROR_ARGS@);
bail:
        return 0;
}
"""

# TODO: Very tempting to merge up FD and INODE templates, as it is just where
# the inode comes from that is different. Not doing it at present so that IF
# there is a change, it is easy to absorb than split the parts then.
FD_OP_CBK_TEMPLATE = """
int32_t
dht2_@NAME@_cbk (
        call_frame_t *frame, void *cookie, xlator_t *this,
        int32_t op_ret, int32_t op_errno,
        @LONG_ARGS@)
{
        VALIDATE_OR_GOTO (frame, bail);

        /* TODO: Rebalance EREMOTE or equivalent errors need to be handled */
        DHT2_STACK_UNWIND (@NAME@, frame, op_ret, op_errno,
                           @SHORT_ARGS@);
bail:
        return 0;
}
"""

FD_OP_FOP_TEMPLATE = """
int32_t
dht2_@NAME@ (
        call_frame_t *frame, xlator_t *this,
        @LONG_ARGS@)
{
        dht2_conf_t     *conf = NULL;
        dht2_local_t    *local = NULL;
        int32_t          op_errno = 0;
        xlator_t        *wind_subvol = NULL;

        VALIDATE_OR_GOTO (frame, bail);
        VALIDATE_OR_GOTO (this, err);
        /* Assumes that code gen will always name fd as 'fd' */
        VALIDATE_OR_GOTO (fd, err);
        VALIDATE_OR_GOTO (fd->inode, err);

        conf = this->private;
        if (!conf)
                goto err;

        /* TODO: GF_FOP_FLUSH should be generated, we are not using it at
         * present, so letting it pass for now as a constant */
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

        /* determine subvolume to wind @NAME@ to */
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

        /* wind @NAME@ to subvolume */
        STACK_WIND (frame, dht2_@NAME@_cbk, wind_subvol,
                    wind_subvol->fops->@NAME@,
                    @SHORT_ARGS@);

        return 0;
err:
        DHT2_STACK_UNWIND (@NAME@, frame, -1, op_errno,
                           @CBK_ERROR_ARGS@);
bail:
        return 0;
}
"""

def gen_defaults ():
	for name in inode_ops:
		print generate(INODE_OP_CBK_TEMPLATE,name,cbk_subs)
                print generate(INODE_OP_FOP_TEMPLATE,name,fop_subs)
        for name in fd_ops:
                print generate(FD_OP_CBK_TEMPLATE,name,cbk_subs)
                print generate(FD_OP_FOP_TEMPLATE,name,fop_subs)

inode_ops = {'open', 'setattr', 'stat'}
fd_ops = {'flush'}

for l in open(sys.argv[1],'r').readlines():
	if l.find('#pragma generate') != -1:
		print "/* BEGIN GENERATED CODE - DO NOT MODIFY */"
		gen_defaults()
		print "/* END GENERATED CODE */"
	else:
		print l[:-1]
