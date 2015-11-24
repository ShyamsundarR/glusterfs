#!/bin/bash

. $(dirname $0)/../../include.rc
. $(dirname $0)/../../volume.rc

cleanup;

TEST glusterd

TEST $CLI volume create $V0 mds 3 data 1 $H0:$B0/${V0}{0..3} ;
EXPECT 'Created' volinfo_field $V0 'Status';

TEST $CLI volume start $V0;
EXPECT 'Started' volinfo_field $V0 'Status';

logdir=`gluster --print-logdir`

TEST build_tester $(dirname $0)/dht2-basictests-gfapi.c -lgfapi -I api/src -Wall -O0 -o $(dirname $0)/dht2-basictests-gfapi
TEST ./$(dirname $0)/dht2-basictests-gfapi $V0  localhost $logdir/dht2-basictests-gfapi.log

cleanup_tester $(dirname $0)/dht2-basictests-gfapi

TEST $CLI volume stop $V0
TEST $CLI volume delete $V0

cleanup;
