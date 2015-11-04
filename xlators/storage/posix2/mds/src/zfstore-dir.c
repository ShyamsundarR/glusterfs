/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

/**
 * ZeroFile store directory operations
 */

#include "zfstore-handle.h"

static int32_t
zfstore_named_lookup (call_frame_t *frame,
                      xlator_t *this, struct zfstore *zf, loc_t *loc)
{
        int32_t      ret     = 0;
        int          parlen  = 0;
        char        *parpath = NULL;
        struct iatt  buf     = {0,};
        struct iatt  postbuf = {0,};

        parlen = zfstore_handle_length (zf->exportdir);
        parpath = alloca (parlen);

        errno = EINVAL;

        /* make parent handle */
        parlen = zfstore_make_handle (this, zf->exportdir,
                                      loc->pargfid, parpath, parlen);
        if (parlen <= 0)
                goto unwind_err;

        /* lookup entry */
        ret = zfstore_handle_entry (this, zf, parpath, loc->name, &buf);
        if (ret)
                goto unwind_err;

        ret = zfstore_resolve_inodeptr
                    (this, zf, loc->pargfid, parpath, &postbuf, _gf_true);
        if (ret)
                goto unwind_err;

        STACK_UNWIND_STRICT (lookup, frame, 0, 0,
                             loc->inode, &buf, NULL, &postbuf);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (lookup, frame, -1, errno, NULL, NULL, NULL, NULL);
        return 0;
}

/**
 * Create the root inode. There's no need to link name as it's the beasty
 * root ("/").
 */
static int32_t
zfstore_create_inode0x1 (xlator_t *this,
                         struct zfstore *zf,
                         char *entry, uuid_t rootgfid, struct iatt *stbuf)
{
        int32_t ret = 0;
        mode_t mode = (0700 | S_IFDIR);

        ret = zfstore_create_inode (this, zf, entry, 0, mode);
        if (ret)
                goto error_return;

        /* we just created it, but still.. */
        ret = zfstore_resolve_inodeptr
                      (this, zf, rootgfid, entry, stbuf, _gf_true);
        /**
         * If lookup() fails now, we return without doing doing any cleanups as
         * we haven't left anything half-baked.
         */
        if (ret)
                goto error_return;
        return 0;

 error_return:
        return -1;
}

static int32_t
zfstore_nameless_lookup (call_frame_t *frame,
                         xlator_t *this, struct zfstore *zf, loc_t *loc)
{
        int32_t      ret      = 0;
        int          entrylen = 0;
        char        *entry    = NULL;
        uuid_t       tgtuuid  = {0,};
        char        *export   = zf->exportdir;
        struct iatt  buf      = {0,};

        entrylen = zfstore_handle_length (export);
        entry = alloca (entrylen);

        if (!gf_uuid_is_null (loc->gfid))
                gf_uuid_copy (tgtuuid, loc->gfid);
        else
                gf_uuid_copy (tgtuuid, loc->inode->gfid);

        errno = EINVAL;
        entrylen = zfstore_make_handle (this, export, tgtuuid, entry, entrylen);
        if (entrylen <= 0)
                goto unwind_err;

        ret = zfstore_resolve_inodeptr
                        (this, zf, tgtuuid, entry, &buf, _gf_false);
        if (ret < 0) {
                if (errno != ENOENT)
                        goto unwind_err;
                if ((errno == ENOENT) && __is_root_gfid (tgtuuid))
                        ret = zfstore_create_inode0x1 (this, zf, entry, tgtuuid, &buf);
                if (ret) {
                        if (errno == ENOENT)
                                errno = ESTALE;
                        goto unwind_err;
                }
        }

        STACK_UNWIND_STRICT (lookup, frame, 0, 0, loc->inode, &buf, NULL, NULL);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (lookup, frame, -1,
                             errno, NULL, NULL, NULL, NULL);
        return 0;
}

int32_t
zfstore_lookup (call_frame_t *frame,
                xlator_t *this, loc_t *loc, dict_t *xdata)
{
        int32_t ret = 0;
        struct zfstore *zf = NULL;

        zf = posix2_get_store (this);

        if (posix2_lookup_is_nameless (loc))
                ret = zfstore_nameless_lookup (frame, this, zf, loc);
        else
                ret = zfstore_named_lookup (frame, this, zf, loc);

        return ret;
}

static int32_t
zfstore_open_and_save (xlator_t *this, fd_t *fd, char *entry, int32_t flags)
{
        int openfd = -1;
        int32_t ret = 0;

        openfd = open (entry, flags);
        if (openfd < 0)
                goto error_return;
        ret = posix2_save_openfd (this, fd, openfd, flags);
        if (ret)
                goto closefd;
        return 0;

 closefd:
        close (openfd);
 error_return:
        return -1;
}

static int32_t
zfstore_open_inode (xlator_t *this,
                    char *export, uuid_t gfid, fd_t *fd, int32_t flags)
{
        int32_t  ret      = 0;
        int      entrylen = 0;
        char    *entry    = NULL;

        entrylen = zfstore_handle_length (export);
        entry = alloca (entrylen);

        errno = EINVAL;
        entrylen = zfstore_make_handle (this, export, gfid, entry, entrylen);
        if (entrylen <= 0)
                goto error_return;

        ret = zfstore_open_and_save (this, fd, entry, flags & ~O_CREAT);
        if (ret)
                goto error_return;
        return 0;

 error_return:
        return -1;
}

static int32_t
zfstore_create_namei (xlator_t *this, char *parpath,
                      loc_t *loc, fd_t *fd, int32_t flags,
                      mode_t mode, dict_t *xdata, struct iatt *stbuf)
{
        int32_t         ret      = 0;
        int             entrylen = 0;
        char           *entry    = NULL;
        char           *export   = NULL;
        void           *uuidreq  = NULL;
        uuid_t          gfid     = {0,};
        struct zfstore *zf       = NULL;

        zf = posix2_get_store (this);
        export = zf->exportdir;

        errno = EINVAL;
        ret = dict_get_ptr (xdata, "gfid-req", &uuidreq);
        if (ret)
                goto error_return;
        gf_uuid_copy (gfid, uuidreq);

        entrylen = zfstore_handle_length (export);
        entry = alloca (entrylen);

        /* create inode */
        entrylen = zfstore_make_handle (this, export, gfid, entry, entrylen);
        if (entrylen <= 0)
                goto error_return;
        ret = zfstore_create_inode (this, zf, entry, flags, mode);
        if (ret)
                goto error_return;

        /* link name to inode */
        ret = zfstore_link_inode (this, zf, parpath, loc->name, gfid);
        if (ret)
                goto purge_inode;

        ret = zfstore_resolve_inodeptr
                        (this, zf, gfid, entry, stbuf, _gf_false);
        if (ret)
                goto purge_entry;

        ret = zfstore_open_and_save (this, fd, entry, flags);
        if (ret)
                goto purge_entry;

        return 0;

 purge_entry:
 purge_inode:
 error_return:
        return -1;
}

/**
 * Create an inode for a given object and acquire an active reference
 * on it. This routine serializes multiple creat() calls for a given
 * parent inode. Parallel creates for the name entry converge at the
 * correct inode and acquire an fd reference.
 */
static int32_t
zfstore_do_namei (xlator_t *this,
                  char *parpath, loc_t *loc, fd_t *fd,
                  int32_t flags, mode_t mode, dict_t *xdata, struct iatt *stbuf)
{
        int32_t         ret    = 0;
        char           *export = NULL;
        inode_t        *parent = NULL;
        struct zfstore *zf     = NULL;

        parent = loc->parent;
        zf = posix2_get_store (this);
        export = zf->exportdir;

        LOCK (&parent->lock);
        {
                ret = zfstore_handle_entry
                               (this, zf, parpath, loc->name, stbuf);
                if (ret < 0) {
                        if (errno != ENOENT)
                                goto unblock;
                        /**
                         * EREMOTE is handled by dht2 as a special case by
                         * looking up the remote object in another MDS. So,
                         * we're left with only two cases:
                         *  - missing entry and inode (ENOENT)
                         *  - valid entry and inode
                         */
                }

                if (ret)
                        ret = zfstore_create_namei (this, parpath, loc, fd,
                                                    flags, mode, xdata, stbuf);
                else
                        ret = zfstore_open_inode
                                (this, export, stbuf->ia_gfid, fd, flags);
        }
 unblock:
        UNLOCK (&parent->lock);

        return ret;
}

int32_t
zfstore_create (call_frame_t *frame,
                xlator_t *this, loc_t *loc, int32_t flags,
                mode_t mode, mode_t umask, fd_t *fd, dict_t *xdata)
{
        int32_t ret = 0;
        int parlen = 0;
        char *parpath = NULL;
        struct iatt buf = {0,};
        struct iatt prebuf = {0,};
        struct iatt postbuf = {0,};
        struct zfstore *zf = NULL;

        zf = posix2_get_store (this);

        parlen = zfstore_handle_length (zf->exportdir);
        parpath = alloca (parlen);

        errno = EINVAL;
        /* parent handle */
        parlen = zfstore_make_handle (this, zf->exportdir,
                                  loc->pargfid, parpath, parlen);
        if (parlen <= 0)
                goto unwind_err;

        /* parent prebuf */
        ret = zfstore_resolve_inodeptr
                       (this, zf, loc->pargfid, parpath, &prebuf, _gf_true);
        if (ret)
                goto unwind_err;

        ret = zfstore_do_namei
                       (this, parpath, loc, fd, flags, mode, xdata, &buf);
        if (ret)
                goto unwind_err;

        /* parent postbuf */
        ret = zfstore_resolve_inodeptr
                       (this, zf, loc->pargfid, parpath, &postbuf, _gf_true);
        if (ret)
                goto unwind_err;

        STACK_UNWIND_STRICT (create, frame, 0, 0,
                             fd, loc->inode, &buf, &prebuf, &postbuf, xdata);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (create, frame,
                             -1, errno, fd, loc->inode, NULL, NULL, NULL, NULL);
        return 0;
}
