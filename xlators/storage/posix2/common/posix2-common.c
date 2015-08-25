/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "posix2.h"

/**
 * Set of common (helper) routines for metadata & data implementation.
 * Make sure whatever ends up here is "really really" common. No hacks
 * please.
 */

int32_t posix2_lookup_is_nameless (loc_t *loc)
{
        return (gf_uuid_is_null (loc->pargfid) && !loc->name);
}
