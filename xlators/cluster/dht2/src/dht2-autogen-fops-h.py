#!/usr/bin/python

import sys
sys.path.append('../../../../libglusterfs/src')
from generator import ops, fop_subs, cbk_subs, generate

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
	for name in my_ops:
		print generate(OP_CBK_TEMPLATE,name,cbk_subs)
                print generate(OP_FOP_TEMPLATE,name,fop_subs)

my_ops = {'open', 'setattr', 'stat', 'flush'}


for l in open(sys.argv[1],'r').readlines():
	if l.find('#pragma generate') != -1:
		print "/* BEGIN GENERATED CODE - DO NOT MODIFY */"
		gen_defaults()
		print "/* END GENERATED CODE */"
	else:
		print l[:-1]
