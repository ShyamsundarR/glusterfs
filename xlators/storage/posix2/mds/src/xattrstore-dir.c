/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "xattrstore.h"
#include "posix2-mds.h"

/**
 * Xattr store directory operations
 */

/**
 * Given an export and a basename, calculate the entry handle length.
 * "parent" length is the length of canonical form of UUID.
 */
static int
xas_handle_length (char *basepath)
{
        return strlen (basepath)        /*   ->exportdir   */
                + 1                     /*       /         */
                + 2                     /*   GFID[0.1]     */
                + 1                     /*       /         */
                + 2                     /*   GFID[2.3]     */
                + 1                     /*       /         */
                + 36                    /*   PARGFID       */
                + 1;                    /*       \0        */
}

static int
xas_make_handle (xlator_t *this, char *export,
                 uuid_t gfid, char *handle, size_t handlesize)
{
        return snprintf (handle, handlesize, XAS_ENTRY_HANDLE_FMT,
                         export, gfid[0], gfid[1], uuid_utoa (gfid));
}

/**
 * Given a handle patch (constructed from GFID), fill in @stbuf with
 * inode attributes. @dircheck guarantees stat information only for
 * directories.
 */
static int32_t
xas_stat_handle (xlator_t *this, uuid_t gfid,
                 char *path, struct iatt *stbuf, gf_boolean_t dircheck)
{
        int32_t ret = 0;
        struct stat lstatbuf = {0,};

        ret = lstat (path, &lstatbuf);
        if (ret)
                return -1;
        if (dircheck && !S_ISDIR (lstatbuf.st_mode))
                return -1;
        if (stbuf) {
                iatt_from_stat (stbuf, &lstatbuf);
                gf_uuid_copy (stbuf->ia_gfid, gfid);
                posix2_fill_ino_from_gfid (this, stbuf);
        }
        return 0;
}

static int32_t
xas_handle_entry (xlator_t *this, char *export,
                  char *parpath, const char *basename, struct iatt *stbuf)
{
        int32_t ret = 0;
        int entrylen = 0;
        char *entry = NULL;
        ssize_t size = 0;
        uuid_t tgtuuid = {0,};
        char realpath[PATH_MAX] = {0,};

        (void) snprintf (realpath, PATH_MAX, "%s/%s", parpath, basename);

        size = sys_lgetxattr (realpath,
                              GFID_XATTR_KEY, tgtuuid, sizeof (uuid_t));
        if (size != sizeof (uuid_t))
                goto error_return;

        entrylen = xas_handle_length (export);
        entry = alloca (entrylen);

        entrylen = xas_make_handle (this, export, tgtuuid, entry, entrylen);
        if (entrylen <= 0)
                goto error_return;

        ret = xas_stat_handle (this, tgtuuid, entry, stbuf, _gf_false);
        if (ret < 0) {
                if (errno == ENOENT)
                        errno = EREMOTE;
                goto error_return;
        }

        return 0;

 error_return:
        return -1;
}

static int32_t
xas_named_lookup (call_frame_t *frame,
                  xlator_t *this, struct xattrstore *xas, loc_t *loc)
{
        int32_t ret = 0;
        int parlen = 0;
        char *parpath = NULL;
        struct iatt buf = {0,};
        struct iatt postbuf = {0,};

        parlen = xas_handle_length (xas->exportdir);
        parpath = alloca (parlen);

        errno = EINVAL;

        /* make parent handle */
        parlen = xas_make_handle (this, xas->exportdir,
                                  loc->pargfid, parpath, parlen);
        if (parlen <= 0)
                goto unwind_err;

        /* lookup entry */
        ret = xas_handle_entry (this, xas->exportdir, parpath, loc->name, &buf);
        if (ret)
                goto unwind_err;

        ret = xas_stat_handle (this, loc->pargfid, parpath, &postbuf, _gf_true);
        if (ret)
                goto unwind_err;

        STACK_UNWIND_STRICT (lookup, frame, 0, 0,
                             loc->inode, &buf, NULL, &postbuf);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (lookup, frame, -1, errno, NULL, NULL, NULL, NULL);
        return 0;
}

static int32_t
xas_nameless_lookup (call_frame_t *frame,
                     xlator_t *this, struct xattrstore *xas, loc_t *loc)
{
        int32_t      ret      = 0;
        int          entrylen = 0;
        char        *entry    = NULL;
        uuid_t       tgtuuid  = {0,};
        char        *export   = xas->exportdir;
        struct iatt  buf      = {0,};

        entrylen = xas_handle_length (export);
        entry = alloca (entrylen);

        if (!gf_uuid_is_null (loc->gfid))
                gf_uuid_copy (tgtuuid, loc->gfid);
        else
                gf_uuid_copy (tgtuuid, loc->inode->gfid);

        errno = EINVAL;
        entrylen = xas_make_handle (this, export, tgtuuid, entry, entrylen);
        if (entrylen <= 0)
                goto unwind_err;

        ret = xas_stat_handle (this, tgtuuid, entry, &buf, _gf_false);
        if (ret < 0) {
                if (errno == ENOENT)
                        errno = EREMOTE;
                goto unwind_err;
        }

        STACK_UNWIND_STRICT (lookup, frame, 0, 0, loc->inode, &buf, NULL, NULL);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (lookup, frame, -1,
                             errno, NULL, NULL, NULL, NULL);
        return 0;
}

int32_t
xattrstore_lookup (call_frame_t *frame,
                   xlator_t *this, loc_t *loc, dict_t *xdata)
{
        int32_t ret = 0;
        struct xattrstore *xas = NULL;

        xas = posix2_get_store (this);

        if (posix2_lookup_is_nameless (loc))
                ret = xas_nameless_lookup (frame, this, xas, loc);
        else
                ret = xas_named_lookup (frame, this, xas, loc);

        return ret;
}
