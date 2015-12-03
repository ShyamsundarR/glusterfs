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

# mkdir test
TEST mkdir $M0/d{0..10}

# setattr test
chmod 777 $M0/f{0..10}
chown 1234 $M0/d{1..10}

# nested directories and files
TEST mkdir -p $M0/D1/D2/D3/D4
TEST touch $M0/D1/F1
TEST touch $M0/D1/D2/F2
TEST touch $M0/D1/D2/D3/F3
TEST touch $M0/D1/D2/D3/D4/F4

# NOTE: no umoun test as there's a segfault due to missing statfs() implementation
#       in posix, v2.
# TEST umount $M0

cleanup;
