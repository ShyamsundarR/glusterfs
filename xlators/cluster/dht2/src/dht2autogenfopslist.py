#!/usr/bin/python

import sys

inode_ops = {'open', 'setattr', 'stat', 'access', 'truncate', 'readlink',
        'setxattr', 'getxattr', 'removexattr', 'inodelk', 'entrylk',
        'xattrop', 'opendir'}
fd_ops = {'fsetattr', 'fstat', 'ftruncate', 'fsetxattr', 'fgetxattr',
        'fremovexattr', 'lk', 'finodelk', 'fentrylk', 'fxattrop', 'flush',
        'readdir'}
unsup_ops = {'readdirp', 'rchecksum', 'getspec', 'fallocate', 'discard',
        'zerofill', 'ipc'}
