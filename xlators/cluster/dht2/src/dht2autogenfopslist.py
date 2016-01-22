#!/usr/bin/python

import sys

inode_ops = {'setattr', 'stat', 'access', 'truncate', 'readlink',
        'setxattr', 'getxattr', 'removexattr', 'inodelk', 'entrylk',
        'xattrop'}
fd_ops_mds = {'fsetattr', 'fstat', 'fsetxattr', 'fgetxattr',
        'fremovexattr', 'lk', 'finodelk', 'fentrylk', 'fxattrop', 'flush'}
fd_ops_ds = {'readv', 'writev', 'ftruncate'}
unsup_ops = {'readdirp', 'rchecksum', 'getspec', 'fallocate', 'discard',
        'zerofill', 'ipc'}
