// Copyright 2008,2009,2010 Massachusetts Institute of Technology.
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#ifndef _CRC32C_H_
#define _CRC32C_H_

uint32_t crc32cSlicingBy8 (uint32_t, const void *, size_t);

#endif /* _CRC32C_H_ */
