/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-layout.h
 * DHT2 layout header file
 */

#ifndef _DHT2_LAYOUT_H_
#define _DHT2_LAYOUT_H_

#include "xlator.h"

struct dht2_layout {
        union {
                struct fixedbucket {
                        uint32_t start;
                        uint32_t end;
                }fixedbucket;
        }subvolayout;
};

struct dht2_layouthandler {
        /**
         * Layout prepare and wreck - @layoutprepare needs to be called as a
         * precursor before calling @layoutgen.
         */
        void * (*layoutprepare) (void *);
        void (*layoutwreck) (void *);

        /**
         * @layoutgen can make use of the value returned bt @layoutpreapre which
         * is passed as the second argument. OTOH, @layoutsearch does not make
         * use this value at all.
         */
        void (*layoutgen) (struct dht2_layout *, void *);
        int (*layoutsearch) (struct dht2_layout *, void *);

};

/* fixed bucket layout handler */
void *dht2_fixed_bucket_layout_prepare (void *);
void  dht2_fixed_bucket_layout_wreck (void *);
void  dht2_fixed_bucket_layout_gen (struct dht2_layout *, void *);
int   dht2_fixed_bucket_layout_search (struct dht2_layout *, void *);

#endif /* _DHT2_LAYOUT_H_ */
