#!/usr/bin/python

import sys
from generator import ops, fop_subs, cbk_subs, generate
from dht2autogenfopslist import inode_ops, fd_ops_mds,fd_ops_ds, unsup_ops

OP_CBK_TEMPLATE = """
int32_t
dht2_@NAME@_cbk (
        call_frame_t *frame, void *cookie, xlator_t *this,
	int32_t op_ret, int32_t op_errno,
        @LONG_ARGS@);
"""

OP_FOP_TEMPLATE = """
int32_t
dht2_@NAME@ (
	call_frame_t *frame, xlator_t *this,
	@LONG_ARGS@);
"""

def gen_defaults ():
	for name in inode_ops:
		print generate(OP_CBK_TEMPLATE,name,cbk_subs)
                print generate(OP_FOP_TEMPLATE,name,fop_subs)
        for name in fd_ops_mds:
                print generate(OP_CBK_TEMPLATE,name,cbk_subs)
                print generate(OP_FOP_TEMPLATE,name,fop_subs)
        for name in fd_ops_ds:
                print generate(OP_CBK_TEMPLATE,name,cbk_subs,layout="DS")
                print generate(OP_FOP_TEMPLATE,name,fop_subs,layout="DS")
        for name in unsup_ops:
                print generate(OP_FOP_TEMPLATE,name,fop_subs)


for l in open(sys.argv[1],'r').readlines():
	if l.find('#pragma generate') != -1:
		print "/* BEGIN GENERATED CODE - DO NOT MODIFY */"
		gen_defaults()
		print "/* END GENERATED CODE */"
	else:
		print l[:-1]
