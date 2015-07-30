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

#ifndef _DHT2_H
#define _DHT2_H

#include "dht2-mem-types.h"

#define DHT2_LAYOUT_MAX_VALUE 0xFFFFFFFF

struct dht2_static_layout {
        uint32_t  d2slay_start;
        uint32_t  d2slay_stop;
        xlator_t *d2slay_subvol;
};
typedef struct dht2_static_layout dht2_static_layout_t;

struct dht2_conf {
        int                      d2cnf_mds_count;
        uint32_t                 d2cnf_mds_chunk;
        dht2_static_layout_t    *d2cnf_mds_layout;

        int                      d2cnf_data_count;
        uint32_t                 d2cnf_data_chunk;
        dht2_static_layout_t    *d2cnf_data_layout;
};
typedef struct dht2_conf dht2_conf_t;

#endif /* _DHT2_H */
