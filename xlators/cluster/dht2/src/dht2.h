/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2hc
 * Primary header for dht2 code.
 */

#ifndef _DHT2_H_
#define _DHT2_H_

#include "list.h"
#include "locking.h"
#include "dht2-mem-types.h"

struct dht2_subvol {
        xlator_t *this;

        struct list_head list;
};

struct dht2_conf {
        gf_lock_t lock;
        void *dht2s;             /* alloced subvolume list, void type to prevent
                                    unnecessary fiddling, use ->mds, ->ds */

        int mds_count;           /* metadata server count */
        int data_count;          /* data server count */

        struct list_head mds;    /* list of metadata server(s) */
        struct list_head ds;     /* list of data server(s) */
};

#define list_for_each_mds_entry(pos, conf)              \
        list_for_each_entry (pos, &(conf)->mds, list)

#define list_for_each_ds_entry(pos, conf)               \
        list_for_each_entry (pos, &(conf)->ds, list)

#endif /* _DHT2_H_ */
