/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-helpers.h
 * Header for DHT2 helper routines and macros.
 */

#ifndef _DHT2_HELPERS_H_
#define _DHT2_HELPERS_H_

#include "dht2.h"
#include "xlator.h"

#define set_if_greater(a, b) do {               \
                if ((a) < (b))                  \
                        (a) = (b);              \
        } while (0)


#define set_if_greater_time(a, an, b, bn) do {                          \
                if (((a) < (b)) || (((a) == (b)) && ((an) < (bn)))) {   \
                        (a) = (b);                                      \
                        (an) = (bn);                                    \
                }                                                       \
        } while (0)                                                     \

dht2_local_t *dht2_local_init (call_frame_t *, loc_t *, fd_t *,
                               glusterfs_fop_t);

void dht2_local_wipe (xlator_t *, dht2_local_t *);

#define DHT2_STACK_UNWIND(fop, frame, params ...) do {          \
                dht2_local_t *__local = NULL;                    \
                xlator_t    *__xl    = NULL;                    \
                if (frame) {                                    \
                        __xl         = frame->this;             \
                        __local      = frame->local;            \
                        frame->local = NULL;                    \
                }                                               \
                STACK_UNWIND_STRICT (fop, frame, params);       \
                dht2_local_wipe (__xl, __local);                \
        } while (0)

int dht2_iatt_merge (struct iatt *, struct iatt *);

uint32_t gfid_to_bucket (uuid_t);

int dht2_generate_nameless_loc (loc_t *, loc_t *);

#endif /* _DHT2_HELPERS_H_ */
