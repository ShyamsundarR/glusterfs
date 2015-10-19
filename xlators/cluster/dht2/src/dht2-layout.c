/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-layout.c
 * DHT2 layout handling
 */

#include "dht2-layout.h"
#include "dht2-mem-types.h"

/**
 * Fixed size (unexpandable) layout generation.
 */

/**
 * This header is kept in the C source file on purpose as this is a temporary
 * "holding" memory used by layout generators. Various layout generators would
 * have different mechanism to assign layouts to subvolumes, hence each one of
 * of them (layout handlers) should declare thier own "holding" structures.
 *
 * This keeps management (and generation) of layouts close to this C source
 * making layout management strictly via APIs.
 */
struct dht2_fixed_bucket_layout {
        uint32_t lstart;
        uint32_t lrange;
#define DHT2_MAX_FIXED_BUCKET_SIZE  (1 << 16)
};

void *
dht2_fixed_bucket_layout_prepare (void *arg)
{
        uint32_t buckets = 0;
        struct dht2_fixed_bucket_layout *fblayout = NULL;

        buckets = *(uint32_t *)arg;
        if (buckets <= 0)
                goto error_return;

        fblayout = GF_CALLOC (1, sizeof (*fblayout), gf_dht2_mt_layout_t);
        if (!fblayout)
                goto error_return;

        fblayout->lstart = 0;
        fblayout->lrange = (DHT2_MAX_FIXED_BUCKET_SIZE / buckets);

        return fblayout;

 error_return:
        return NULL;
}

void
dht2_fixed_bucket_layout_wreck (void *arg)
{
        struct dht2_fixed_bucket_layout *fblayout = arg;

        GF_FREE (fblayout);
}

void
dht2_fixed_bucket_layout_gen (struct dht2_layout *layout, void *arg)
{
        struct fixedbucket *currlayout = &layout->subvolayout.fixedbucket;
        struct dht2_fixed_bucket_layout *fblayout = arg;

        currlayout->start = fblayout->lstart;
        currlayout->end = fblayout->lstart + fblayout->lrange;

        fblayout->lstart += fblayout->lrange;
}

/* layout is of the form [start, end) */
int
dht2_fixed_bucket_layout_search (struct dht2_layout *layout, void *search)
{
        uint32_t bucket = *(uint32_t *)search;
        struct fixedbucket *currlayout = &layout->subvolayout.fixedbucket;

        if ((bucket >= currlayout->start) && (bucket < currlayout->end))
                return 1;
        return 0;
}
