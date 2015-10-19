/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-layout.h
 * Header for dht2-layout related defines, macros and routines
 * and options.
 */

#ifndef _DHT2_LAYOUT_H_
#define _DHT2_LAYOUT_H_

#include "dht2.h"
#include "uuid.h"

#define DHT2_LAYOUT_MAX_BUCKETS 65536 /* 2 bytes */

typedef enum dht2_layout_type {
        DHT2_MDS_LAYOUT = 1,
        DHT2_DS_LAYOUT = 2
} dht2_layout_type_t;

struct dht2_layout {
        xlator_t *d2lay_mds_subvol_list[DHT2_LAYOUT_MAX_BUCKETS];
        xlator_t *d2lay_ds_subvol_list[DHT2_LAYOUT_MAX_BUCKETS];
};
typedef struct dht2_layout dht2_layout_t;

xlator_t *dht2_find_subvol_for_gfid (dht2_conf_t *, uuid_t, dht2_layout_type_t);

int dht2_layout_fetch (xlator_t *, dht2_conf_t *);

void dht2_layout_return (dht2_layout_t *);

#endif /* _DHT2_LAYOUT_H_ */
