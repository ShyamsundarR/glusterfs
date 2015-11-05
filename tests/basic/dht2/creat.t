#!/bin/bash

. $(dirname $0)/../../include.rc
. $(dirname $0)/../../volume.rc
. $(dirname $0)/../../dht2.rc

cleanup;

TEST glusterd
TEST pidof glusterd

# create distribute2 volume {2,2}
TEST create_dht2_volume $V0 2 2 $H0:$B0/${V0}1-mds $H0:$B0/${V0}2-mds $H0:$B0/${V0}1-ds $H0:$B0/${V0}2-ds
EXPECT "$V0" volinfo_field $V0 'Volume Name';
EXPECT 'Created' volinfo_field $V0 'Status';

# start
TEST $CLI volume start $V0
EXPECT 'Started' volinfo_field $V0 'Status';

# fuse mount
TEST $GFS --volfile-server=$H0 --volfile-id=$V0 $M0;

# empty file creations
TEST touch $M0/f{0..300}

# stat() check
TEST stat $M0/f{0..300}

# NOTE: no umoun test as there's a segfault due to missing statfs() implementation
#       in posix, v2.
# TEST umount $M0

cleanup;
