/*
  Copyright (c) 2008-2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: dht2-inode-fops-tmpl.c
 * This file contains the DHT2 inode based FOPs template. This is run through
 * the code generator, generator.py to generate the required FOPs.
 */

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "glusterfs.h"
#include "xlator.h"
#include "logging.h"
#include "statedump.h"
#include "dht2.h"
#include "dht2-helpers.h"
#include "dht2-layout.h"
#include "dht2-messages.h"
#include "dht2-autogen-fops.h"

#pragma generate
