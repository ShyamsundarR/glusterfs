/*
   Copyright (c) 2006-2015 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

/**
 * ZeroFile store inode operations
 */

#include "zfstore-handle.h"

int32_t
zfstore_flush (call_frame_t *frame, xlator_t *this, fd_t *fd, dict_t *dict)
{
        STACK_UNWIND_STRICT (flush, frame, 0, 0, NULL);
        return 0;
}

static int
zfstore_do_chown (xlator_t *this,
                  const char *entry, struct iatt *stbuf, int32_t valid)
{
        uid_t uid = -1;
        gid_t gid = -1;

        if (valid & GF_SET_ATTR_UID)
                uid = stbuf->ia_uid;

        if (valid & GF_SET_ATTR_GID)
                gid = stbuf->ia_gid;

        return lchown (entry, uid, gid);
}

static int
zfstore_do_chmod (xlator_t *this, const char *entry, struct iatt *stbuf)
{
        mode_t  mode = 0;

        mode = st_mode_from_ia (stbuf->ia_prot, stbuf->ia_type);
        return lchmod (entry, mode);
}

static int
zfstore_do_utimes (xlator_t *this, const char *entry, struct iatt *stbuf)
{
        struct timeval tv[2] = {{0,},{0,}};

        tv[0].tv_sec  = stbuf->ia_atime;
        tv[0].tv_usec = stbuf->ia_atime_nsec / 1000;
        tv[1].tv_sec  = stbuf->ia_mtime;
        tv[1].tv_usec = stbuf->ia_mtime_nsec / 1000;

        return lutimes (entry, tv);
}

static int32_t
zfstore_do_setattr (xlator_t *this, char *entry, int valid, struct iatt *stbuf)
{
        int32_t ret = 0;

        if (valid & (GF_SET_ATTR_UID | GF_SET_ATTR_GID)) {
                ret = zfstore_do_chown (this, entry, stbuf, valid);
                if (ret)
                        goto error_return;
        }

        if (valid & GF_SET_ATTR_MODE) {
                ret = zfstore_do_chmod (this, entry, stbuf);
                if (ret)
                        goto error_return;
        }

        if (valid & (GF_SET_ATTR_ATIME | GF_SET_ATTR_MTIME)) {
                ret = zfstore_do_utimes (this, entry, stbuf);
        }

 error_return:
        return ret;
}

int32_t
zfstore_setattr (call_frame_t *frame, xlator_t *this,
                 loc_t *loc, struct iatt *stbuf, int32_t valid, dict_t *xdata)
{
        int32_t ret = 0;
        int entrylen = 0;
        char *entry = NULL;
        struct zfstore *zf = NULL;
        struct iatt prebuf = {0,};
        struct iatt postbuf = {0,};

        zf = posix2_get_store (this);

        entrylen = zfstore_handle_length (zf->exportdir);
        entry = alloca (entrylen);

        errno = EINVAL;
        entrylen = zfstore_make_handle (this, zf->exportdir,
                                        loc->gfid, entry, entrylen);
        if (entrylen <= 0)
                goto unwind_err;

        ret = zfstore_resolve_inodeptr
                         (this, loc->gfid, entry, &prebuf, _gf_false);
        if (ret)
                goto unwind_err;

        ret = zfstore_do_setattr (this, entry, valid, stbuf);
        if (ret)
                goto unwind_err;

        ret = zfstore_resolve_inodeptr
                         (this, loc->gfid, entry, &postbuf, _gf_false);
        if (ret)
                goto unwind_err;

        STACK_UNWIND_STRICT (setattr, frame, 0, 0, &prebuf, &postbuf, NULL);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (setattr, frame, -1, errno, NULL, NULL, NULL);
        return 0;
}

int32_t
zfstore_stat (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xdata)
{
        int32_t ret = 0;
        int entrylen = 0;
        char *entry = NULL;
        struct iatt buf = {0,};
        struct zfstore *zf = NULL;

        zf = posix2_get_store (this);

        entrylen = zfstore_handle_length (zf->exportdir);
        entry = alloca (entrylen);

        errno = EINVAL;
        entrylen = zfstore_make_handle (this, zf->exportdir,
                                        loc->gfid, entry, entrylen);
        if (entrylen <= 0)
                goto unwind_err;

        ret = zfstore_resolve_inodeptr
                     (this, loc->gfid, entry, &buf, _gf_false);
        if (ret)
                goto unwind_err;

        STACK_UNWIND_STRICT (stat, frame, 0, 0, &buf, NULL);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (stat, frame, -1, errno, NULL, NULL);
        return 0;
}
