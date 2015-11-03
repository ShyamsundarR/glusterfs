#!/usr/bin/python

import sys

inode_ops = {'open', 'setattr', 'stat', 'access', 'truncate', 'readlink',
        'setxattr', 'getxattr', 'removexattr', 'inodelk', 'entrylk',
        'xattrop'}
fd_ops = {'fsetattr', 'fstat', 'ftruncate', 'fsetxattr', 'fgetxattr',
        'fremovexattr', 'lk', 'finodelk', 'fentrylk', 'fxattrop', 'flush'}
