/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-main.c
 * This file contains the xlator, loading functions, FOP entry points
 * and options.
 * The entire functionality including comments is TODO.
 */

#include "glusterfs.h"
#include "xlator.h"
#include "logging.h"
#include "statedump.h"
#include "dht2.h"

int
dht2_print_layouts (xlator_t *this, dht2_conf_t *conf)
{
        int i;

        gf_msg (this->name, GF_LOG_INFO, 0, 0, "MDS layout:");
        for (i = 0; i < conf->d2cnf_mds_count; i ++){
                gf_msg (this->name, GF_LOG_INFO, 0, 0, "\t%s - Start - %x :"
                        " Stop - %x",
                        conf->d2cnf_mds_layout[i].d2slay_subvol->name,
                        conf->d2cnf_mds_layout[i].d2slay_start,
                        conf->d2cnf_mds_layout[i].d2slay_stop);
        }

        gf_msg (this->name, GF_LOG_INFO, 0, 0, "Data layout:");
        for (i = 0; i < conf->d2cnf_data_count; i ++){
                gf_msg (this->name, GF_LOG_INFO, 0, 0, "\t%s - Start - %x :"
                        " Stop - %x",
                        conf->d2cnf_data_layout[i].d2slay_subvol->name,
                        conf->d2cnf_data_layout[i].d2slay_start,
                        conf->d2cnf_data_layout[i].d2slay_stop);
        }

        return 0;
}

int
dht2_create_static_layout (xlator_t *this, dht2_conf_t *conf)
{
        int              ret = -1;
        int              i;
        xlator_list_t   *subvol_list;

        /* Wat we do here
         * - We allocate 2 arrays to store the data and mds layout
         * - layout chunk per member is a simple division of the entire range
         *   by the subvols for the layout
         * - each element in the array is sorted by its layout start
         * - each element contains the subvol that represents its range
         * We do this, so as to get the array index a hash belongs to, by
         * simple division of the hash with the layout chunk */

        conf->d2cnf_mds_layout = GF_CALLOC (conf->d2cnf_mds_count,
                                            sizeof(dht2_static_layout_t),
                                            gf_dht2_mt_dht2_static_layout_t);
        if (!conf->d2cnf_mds_layout)
                goto out;

        conf->d2cnf_data_layout = GF_CALLOC (conf->d2cnf_data_count,
                                             sizeof(dht2_static_layout_t),
                                             gf_dht2_mt_dht2_static_layout_t);
        if (!conf->d2cnf_data_layout)
                goto out;

        conf->d2cnf_mds_chunk =
                        DHT2_LAYOUT_MAX_VALUE / conf->d2cnf_mds_count;
        conf->d2cnf_data_chunk =
                        DHT2_LAYOUT_MAX_VALUE / conf->d2cnf_data_count;

        subvol_list = this->children;
        for (i = 0; i < conf->d2cnf_mds_count; i++) {
                conf->d2cnf_mds_layout[i].d2slay_start =
                                        i * conf->d2cnf_mds_chunk;
                conf->d2cnf_mds_layout[i].d2slay_stop =
                                        conf->d2cnf_mds_layout[i].d2slay_start
                                        + conf->d2cnf_mds_chunk - 1;
                conf->d2cnf_mds_layout[i].d2slay_subvol = subvol_list->xlator;

                subvol_list = subvol_list->next;
        }
        conf->d2cnf_mds_layout[conf->d2cnf_mds_count - 1].d2slay_stop =
                                                DHT2_LAYOUT_MAX_VALUE;

        for (i = 0; i < conf->d2cnf_data_count; i++) {
                conf->d2cnf_data_layout[i].d2slay_start =
                                        i * conf->d2cnf_data_chunk;
                conf->d2cnf_data_layout[i].d2slay_stop =
                                        conf->d2cnf_data_layout[i].d2slay_start
                                        + conf->d2cnf_data_chunk - 1;
                conf->d2cnf_data_layout[i].d2slay_subvol = subvol_list->xlator;

                subvol_list = subvol_list->next;
        }
        conf->d2cnf_mds_layout[conf->d2cnf_data_count - 1].d2slay_stop =
                                                DHT2_LAYOUT_MAX_VALUE;

        dht2_print_layouts (this, conf);
        ret = 0;
out:
        return ret;
}
int32_t
mem_acct_init (xlator_t *this)
{
        int     ret = -1;

        GF_VALIDATE_OR_GOTO ("dht2", this, out);

        ret = xlator_mem_acct_init (this, gf_dht2_mt_end + 1);

        if (ret != 0) {
                gf_msg (this->name, GF_LOG_ERROR, errno, 0,
                        "Memory accounting init failed");
                return ret;
        }
out:
        return ret;
}

int32_t
dht2_init (xlator_t *this)
{
        int          ret = -1;
        dht2_conf_t *conf = NULL;

        if (!this->parents) {
                gf_log (this->name, GF_LOG_WARNING,
                        "dangling volume. check volfile ");
                goto out;
        }

        conf = GF_CALLOC (1, sizeof (*conf), gf_dht2_mt_dht2_conf_t);
        if (!conf)
                goto out;


        GF_OPTION_INIT("dht2-data-count", conf->d2cnf_data_count, int32, out);
        GF_OPTION_INIT("dht2-mds-count", conf->d2cnf_mds_count, int32, out);

        if (!conf->d2cnf_data_count || !conf->d2cnf_mds_count) {
                gf_msg (this->name, GF_LOG_CRITICAL, EINVAL, 0,
                        "Invalid mds or data count specified"
                        " (data - %d, mds - %d)",
                        conf->d2cnf_data_count, conf->d2cnf_mds_count);
                goto out;
        }

        gf_msg (this->name, GF_LOG_INFO, 0, 0,
                        "mds and data count specified"
                        " (data - %d, mds - %d)",
                        conf->d2cnf_data_count, conf->d2cnf_mds_count);

        ret = dht2_create_static_layout (this, conf);
        if (ret) {
                gf_msg (this->name, GF_LOG_CRITICAL, errno, 0,
                        "Failed to initialize layouts for distribution");
                goto out;
        }

        this->private = conf;

        ret = 0;

out:
        return ret;
}

void
dht2_fini (xlator_t *this)
{
        dht2_conf_t *conf = NULL;

        GF_VALIDATE_OR_GOTO ("dht2", this, out);

        conf = this->private;
        this->private = NULL;
        if (conf) {
                GF_FREE (conf);
        }
out:
        return;
}

class_methods_t class_methods = {
        .init           = dht2_init,
        .fini           = dht2_fini,
};

struct xlator_fops fops = {
};

struct xlator_cbks cbks = {
};

/*
struct xlator_dumpops dumpops = {
};
*/

struct volume_options options[] = {
        { .key   = {"dht2-data-count"},
          .type  = GF_OPTION_TYPE_INT,
          .default_value = "0",
          .description = "Specifies the number of DHT2 subvols to use as data"
                         "volumes."
        },
        { .key   = {"dht2-mds-count"},
          .type  = GF_OPTION_TYPE_INT,
          .default_value = "0",
          .description = "Specifies the number of DHT2 subvols to use as"
                         " meta-data volumes."
        },
        { .key   = {NULL} },
};
