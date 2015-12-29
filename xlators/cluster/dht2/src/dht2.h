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

#include "iatt.h"
#include "common-utils.h"
#include "xlator.h"
#include "dht2-mem-types.h"
#include "dht2-autogen-fops.h"

#define DHT2_MSG_DOM "dht2"
#define DHT_EREMOTE_XATTR_STR "eremote"

typedef enum dht2_eremote_reasons {
        DHT2_INODE_REMOTE = 1,
        DHT2_UNKNOWN = 2
} dht2_eremote_reasons_t;

struct dht2_conf {
        /* memory pools */
        struct mem_pool *d2cnf_localpool;   /* mem pool for struct dht2_local
                                           allocation */

        /* subvolume configuration */
        int     d2cnf_data_count;       /* count of data subvolumes */
        int     d2cnf_mds_count;        /* count of meta-data subvolumes */

        /* layout specification */
        struct  dht2_layout *d2cnf_layout;

        /* xattr specification */
        char    *d2cnf_xattr_base_name; /* xattr base name for DHT2 xattrs */
        char    *d2cnf_eremote_reason_xattr_name; /* xattr name for DHT2
                                                   * EREMOTE error */
};
typedef struct dht2_conf dht2_conf_t;

struct dht2_local {
        glusterfs_fop_t      d2local_fop;
        loc_t                d2local_loc;
        dict_t              *d2local_xattr_req;
        gf_boolean_t         d2local_postparent_stbuf_filled;
        struct iatt          d2local_postparent_stbuf;

        struct iatt          d2local_stbuf;         /* stat buf of the entry */
        xlator_t            *cached_subvol;
        fd_t                *fd;
};
typedef struct dht2_local dht2_local_t;

/* xlator FOP entry/exit points */
int32_t dht2_lookup (call_frame_t *, xlator_t *, loc_t *, dict_t *);

int32_t dht2_create (call_frame_t *, xlator_t *, loc_t *, int32_t,
                     mode_t, mode_t, fd_t *, dict_t *);

#endif /* _DHT2_H */
