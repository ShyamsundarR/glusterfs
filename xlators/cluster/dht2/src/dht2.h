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

struct dht2_conf {
        int     d2cnf_data_count;
        int     d2cnf_mds_count;
};

typedef struct dht2_conf dht2_conf_t;

#endif /* _DHT2_H */
