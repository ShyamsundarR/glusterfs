/*
  Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

#ifndef __POSIX2_MESSAGES_H__
#define __POSIX2_MESSAGES_H__

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "glfs-message-id.h"

/*! \file posix2-messages.h
 *  \brief Posix (v2) log-message IDs and their descriptions
 */

#define POSIX2_COMP_BASE         GLFS_MSGID_COMP_POSIX
#define GLFS_NUM_MESSAGES        1
#define GLFS_MSGID_END           (POSIX2_COMP_BASE + GLFS_NUM_MESSAGES + 1)

/* Messaged with message IDs */
#define glfs_msg_start_x POSIX2_COMP_BASE, "Invalid: Start of messages"
/*------------*/

#define POSIX2_MSG_SUBVOL_ERR                      (POSIX2_COMP_BASE + 1)

#define POSIX2_MSG_DANGLING_VOL                    (POSIX2_COMP_BASE + 2)

#define POSIX2_MSG_EXPORT_MISSING                  (POSIX2_COMP_BASE + 3)

#define POSIX2_MSG_EXPORT_NOTDIR                   (POSIX2_COMP_BASE + 4)

#define POSXI2_ZFXATTR_MSG_EXPORT_NO_XASUP         (POSIX2_COMP_BASE + 5)

#define POSIX2_ZFXATTR_MSG_EXPORT_INVAL_VOLID      (POSIX2_COMP_BASE + 6)

#define POSIX2_ZFXATTR_MSG_EXPORT_MISSING_VOLID    (POSIX2_COMP_BASE + 7)

#define POSIX2_ZFXATTR_MSG_EXPORT_VOLID_FAILURE    (POSIX2_COMP_BASE + 8)

#define POSIX2_MSG_INODE_NULL_GFID                 (POSIX2_COMP_BASE + 9)

#define POSIX2_MSG_PFD_NULL                        (POSIX2_COMP_BASE + 10)

#define POSIX2_MSG_FSTAT_FAILED                    (POSIX2_COMP_BASE + 11)

#endif  /* __POSIX2_MESSAGES_H__ */
