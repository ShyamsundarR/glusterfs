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

struct dht2_layouthandler dht2_layout_handler = {
                .layoutprepare = dht2_fixed_bucket_layout_prepare,
                .layoutwreck   = dht2_fixed_bucket_layout_wreck,
                .layoutgen     = dht2_fixed_bucket_layout_gen,
                .layoutsearch  = dht2_fixed_bucket_layout_search,
};

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

/**
 * Group a set of subvolume(s) starting at offset @start in @children into a
 * appropriate list. Returns the number of subvolumes grouped iff it matches
 * @count, -1 on failure.
 */
static int32_t
dht2_group_subvol (xlator_t *this,
                     struct dht2_conf *conf,
                     struct dht2_subvol *dht2s,
                     struct list_head *list, int start, int count)
{
        int32_t subvolcnt = 0;
        xlator_list_t *subvol = NULL;
        struct dht2_subvol *curr = NULL;

        subvol = this->children;
        for (; start > 0; subvol = subvol->next, start--, dht2s++)
                ; /* deliberate */

        while (subvol && (subvolcnt < count)) {
                curr = (dht2s + subvolcnt++);

                curr->this = subvol->xlator;
                INIT_LIST_HEAD (&curr->list);

                list_add_tail (&curr->list, list);
                subvol = subvol->next;
        }

        if (subvolcnt != count)
                return -1;
        return subvolcnt;
}

/**
 * Splitting is simple, first N subvolumes are metadata servers and the next
 * M are data (storage) servers. This routine "splits" the xlators subvolume
 * in separate list (->mds and ->ds) based on the metadata and data count
 * provided via xlator configuration.
 *
 * Note that the xlator bails out if the count boundaries underflow, i.e., the
 * number of subvolumes should be equal to the sum of data and metadata count.
 * However, in case of overflow, extra subvolumes are thankfully ignored.
 */
static int32_t
dht2_split_subvols (xlator_t *this, struct dht2_conf *conf)
{
        int parsed = 0;
        struct dht2_subvol *dht2s = NULL;

        GF_OPTION_INIT("dht2-mds-count", conf->mds_count, int32, out);
        GF_OPTION_INIT("dht2-data-count", conf->data_count, int32, out);

        if (!(conf->mds_count > 0) || !(conf->data_count > 0)) {
                gf_msg (this->name, GF_LOG_CRITICAL, EINVAL, 0,
                        "Invalid mds or data count [data - %d, mds - %d]",
                        conf->data_count, conf->mds_count);
                goto out;
        }

        INIT_LIST_HEAD (&conf->mds);
        INIT_LIST_HEAD (&conf->ds);

        dht2s = GF_CALLOC (conf->mds_count + conf->data_count,
                           sizeof (struct dht2_subvol), gf_dht2_mt_subvol_t);
        if (!dht2s)
                goto out;

        /* split MDS and DS */
        parsed = dht2_group_subvol (this, conf,
                                    dht2s, &conf->mds, 0, conf->mds_count);
        if (parsed < 0)
                goto free_subvols;
        parsed = dht2_group_subvol (this, conf, dht2s,
                                    &conf->ds, parsed, conf->data_count);
        if (parsed < 0)
                goto free_subvols;

        gf_msg (this->name, GF_LOG_INFO, 0, 0,
                "Distribute v2 initialized with %d MDS node(s), %d DS node(s)",
                conf->mds_count, conf->data_count);

        conf->dht2s = dht2s;
        return 0;

 free_subvols:
        GF_FREE (dht2s);
 out:
        return -1;
}

/* Initialize only MDS layout (for now..) */
static int32_t
dht2_init_layout (xlator_t *this, struct dht2_conf *conf)
{
        void *layoutdata = NULL;
        struct dht2_subvol *subvol = NULL;
        struct dht2_layouthandler *layouthandler = NULL;

        layouthandler = conf->layouthandler = &dht2_layout_handler;

        /* parepare layout */
        layoutdata = layouthandler->layoutprepare ((void *)&(conf->mds_count));
        if (!layoutdata)
                return -1;

        /* generate layout for subvolumes */
        for_each_mds_entry (subvol, conf) {
                layouthandler->layoutgen (&subvol->layout, layoutdata);
        }

        /* wreck (purge) temporary handler data */
        layouthandler->layoutwreck (layoutdata);
        return 0;
}

int32_t
dht2_init (xlator_t *this)
{
        int               ret  = -1;
        struct dht2_conf *conf = NULL;

        if (!this->parents) {
                gf_log (this->name, GF_LOG_WARNING,
                        "dangling volume. check volfile ");
                goto out;
        }

        if (!this->children) {
                gf_msg (this->name, GF_LOG_CRITICAL, EINVAL, 0,
                        "Distribute (v2) needs more than one subvolume");
                goto out;
        }

        conf = GF_CALLOC (1, sizeof (*conf), gf_dht2_mt_conf_t);
        if (!conf)
                goto out;

        ret = dht2_split_subvols (this, conf);
        if (ret)
                goto free_conf;
        ret = dht2_init_layout (this, conf);
        if (ret)
                goto free_conf;

        LOCK_INIT (&conf->lock);
        this->private = conf;
        return 0;

 free_conf:
        GF_FREE (conf);
 out:
        return -1;
}

void
dht2_fini (xlator_t *this)
{
        struct dht2_conf *conf = NULL;

        GF_VALIDATE_OR_GOTO ("dht2", this, out);

        conf = this->private;
        this->private = NULL;
        if (conf) {
                LOCK_DESTROY (&conf->lock);
                GF_FREE (conf->dht2s);
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
        .lookup = dht2_lookup,
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
