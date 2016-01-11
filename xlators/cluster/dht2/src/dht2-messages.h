/*Copyright (c) 2015 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

#ifndef _DHT2_MESSAGES_H_
#define _DHT2_MESSAGES_H_

#include "glfs-message-id.h"

/*! \file dht2-messages.h
 *  \brief DHT2 log-message IDs and their descriptions
 *
 */

#define GLFS_DHT2_BASE          GLFS_MSGID_COMP_DHT2
#define GLFS_DHT2_NUM_MESSAGES  4
#define GLFS_MSGID_DHT2_END     (GLFS_DHT2_BASE + GLFS_DHT2_NUM_MESSAGES + 1)

/*!
 * @messageid 109001 (TBD: Correct this number)
 * @diagnosis Entry name for object failed to return inode/gfid number
 * @recommendedaction  None
 *
 */
#define DHT2_MSG_INODE_NUMBER_MISSING (GLFS_DHT2_BASE + 1)

#define DHT2_MSG_FIND_SUBVOL_ERROR (GLFS_DHT2_BASE + 2)

#define DHT2_MSG_EREMOTE_REASON_MISSING (GLFS_DHT2_BASE + 3)

#define DHT2_MSG_MISSING_GFID_IN_INODE (GLFS_DHT2_BASE + 4)

#define DHT2_MSG_INVALID_FOP (GLFS_DHT2_BASE + 5)

#endif /* _DHT2_MESSAGES_H_ */
