/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-layout.c
 * This file contains all layout helpers, manupulation routines
 * and options.
 * Extrapolating the implementation, to have pluggable layouts, this would be
 * the class abstraction calling into the required provider for the configured
 * layout type.
 */

#include "dht2-helpers.h"
#include "dht2-layout.h"

xlator_t *
dht2_find_subvol_for_gfid (dht2_conf_t *conf, uuid_t gfid,
                           dht2_layout_type_t which)
{
        xlator_t **search_layout = NULL;

        if (which == DHT2_MDS_LAYOUT)
                search_layout = conf->d2cnf_layout->d2lay_mds_subvol_list;
        else
                search_layout = conf->d2cnf_layout->d2lay_ds_subvol_list;

        /* return subvol at bucket index */
        /* TODO: extraction is top 2 bytes hence cannot overflow,
         * maybe check later */
        return search_layout[gfid_to_bucket (gfid)];
}

/* TODO: This function should ideally fetch an bucket assignment for subvolumes
 * or more popularly the layout and store it in the DHT2 conf structure.
 * At present it just statically computes a bucket assignment as cluster
 * expansion is not yet supported */
dht2_layout_t *
dht2_layout_fetch (xlator_t *this, dht2_conf_t *conf)
{
        int subvol_position, i;
        dht2_layout_t *layout = NULL;
        xlator_list_t *subvol_list, *subvol_data_list;

        layout = GF_CALLOC (1, sizeof (dht2_layout_t),
                            gf_dht2_mt_dht2_layout_t);
        if (!layout)
                return NULL;

        /* Iterate over bucket array assigning in order subvolumes */
        /* for MDS first */
        subvol_list = this->children;
        subvol_position = 0;
        for (i = 0; i < DHT2_LAYOUT_MAX_BUCKETS; i++) {
                layout->d2lay_mds_subvol_list[i] =
                        subvol_list->xlator;
                subvol_list = subvol_list->next;
                subvol_position++;
                /* reset to head, to start assigning next set of buckets */
                if (subvol_position >= conf->d2cnf_mds_count) {
                        subvol_list = this->children;
                        subvol_position = 0;
                }
        }

        /* skip MDS subvolumes */
        subvol_list = this->children;
        for (i = 0; i < conf->d2cnf_mds_count; i++) {
                subvol_list = subvol_list->next;
        }
        subvol_data_list = subvol_list;

        /* DATA next
         * subvol_list is already at start of DATA subvolumes
         * as per above loop*/
        subvol_position = 0;
        for (i = 0; i < DHT2_LAYOUT_MAX_BUCKETS; i++) {
                layout->d2lay_ds_subvol_list[i] =
                        subvol_list->xlator;
                subvol_list = subvol_list->next;
                subvol_position++;
                /* reset to head, to start assigning next set of buckets */
                if (subvol_position >= conf->d2cnf_data_count) {
                        subvol_list = subvol_data_list;
                        subvol_position = 0;
                }
        }

        return layout;
}

void
dht2_layout_return (dht2_layout_t *layout)
{
        if (layout) {
                GF_FREE (layout);
        }

        return;
}
