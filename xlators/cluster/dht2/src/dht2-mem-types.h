/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2.h
 * Primary header for dht2 code.
 */

#ifndef _DHT2_MEM_TYPES_H_
#define _DHT2_MEM_TYPES_H_

#include "mem-types.h"

enum gf_dht2_mem_types_ {
        gf_dht2_mt_dht2_conf_t = gf_common_mt_end + 1,
        gf_dht2_mt_end
};

#endif /* _DHT2_MEM_TYPES_H_ */
