# POSIX xlator functional spec for DHT2 based on disk format
With DHT2 the on disk format of storing data changes as a result the
POSIX xlator also needs to adapt to the changes.

This document details how the POSIX layer would function given the new on disk
format.

## New on disk format
Borrowed from the DHT2 First prototype design specification,

### MetaDataServer (MDS):
1. Consolidated file system view across all MDS bricks
  /0x00/0x00/0x00000001/ (Gluster root inode)
              - Dir1  (GFID: 0xDD000001)
              - Dir2  (GFID: 0xDD000002)
              - File1 (GFID: 0xFF000001)
  /0xDD/0x00/0xDD000001/ (Dir1)
              - File2 (GFID: 0xFF000002)
              - Dir3  (GFID: 0xDD000003)
  /0xDD/0x00/0xDD000002/ (Dir2)
              - File3 (GFID: 0xFF000003)
  /0xDD/0x00/0xDD000003/ (Dir3)
  
### DataServer (DS):
1. Consolidated view across all DS bricks
  /0xFF/0x00/0xFF000001
  /0xFF/0x00/0xFF000002
  /0xFF/0x00/0xFF000003

## RPC payload
Before getting into the actual FOPs it is useful to understand what information
is exchanged in the RPC packet, as that helps design each FOP at the POSIX
layer appropriately.

Typically in any non-fd based FOP, the RPC contains,
  - (GFID) of the object that the FOP is targeted at
  OR,
  - (ParGFID, basename) of the object being created, renamed, linked, unlinked

For fd based FOPs,
  - (FD) i.e the file descriptor is passed, which can help the brick process
  glean which inode and hence the GFID being operated upon
  - (FD, GFID) where FD is ANON and GFID is the inode that needs to be operated
  upon

As the above shows, most operations depend on the client sending in the GFID
of the object being operated upon. Hence a simple path creation to get to the
said GFID helps in most POSIX calls. I.e,
  - Given GFID, 0xXXYYABCD
    - Convert to /0xXX/0xYY/0xXXYYABCD path

FD based calls are relatively simple, as the 'real' POSIX fd is stashed and
extracted to operate on in the fd context.

ANON FD calls are handled, by opening the fd->inode->gfid and performing the
required operation. Which is equivalent to opening the converted GFID path as
above.

The set of (ParGFID, basename) based FOPs are the complicated ones as these
need to either lookup the entry and return the GFID of the entry or,
create/rename just the basename given the ParGFID. The commonality though is
path creation again, where given a ParGFID of say 0xXXYYABCD and basename of
'OBJECT' the path is, 
  - /0xXX/0xYY/0xXXYYBACD/OBJECT

As a result, at the POSIX layer, just depending on the above values should be
enough to implement a working model for the new on disk representation.

## Server resolve_and_resume
Another consideration is how the inode and dentry table on the server xlator
is maintained in the new on disk scheme. In the current scheme, this is an
clean hierarchy as represented on the virtual or real file system, as directories
are created in all subvolumes, and so hierarchy is maintained.

With the new on disk format, the server side inode/dentry cache represents a
.glusterfs like name space in memory.

How this impacts resolve_and_resume and related routines at the server xlator
needs to be analyzed.

## FOPs functional specification

### lookup
Named lookup: Given pargfid and basename to lookup
  - Actual entry is located in /0xXX/0xYY/0xXXYYABCD/basename
  - So, construct the path as above, given the pargfid and basename
  - stat the entry
  - Success:
    - Read the xattr containing the GFID for the basename
    - Return EREMOTE with xdata containing the GFID
      * The above needs to be vetted [Issue 1](#Issue 1)
  - Failure:
    - ENOENT

Nameless lookup: Given just the gfid and no pargfid, or given gifid and no
basename
  - Generate on disk path /0xXX/0xYY/0xXXYYABCD
  - stat the entry
  - Success:
    - Read any requested xdata
    - Return success
  - Failure:
    - ESTALE

### open
Given a filesystem path (pargfid, basename) or just the gfid
  - Generate the inode handle path
    - (pargfid, basename) : /0xXX/0xYY/0xXXYYABCD/basename
    - gfid : /0xXX/0xYY/0xXXYYABCD
  - invoke open() [path, flags, mode]
    - handle failures and return
    - save fd number in fd_t ctx

At this point, we have two choice at the DHT (2 :)) layer:

  - Invoke open() on the object (w/ gfid) in the DS before returning
    the open call back to the client. This can result in increased
    fop times for certain work loads [open, close, ..., open, close]
  - Defer open() on the DS until the *first* fop destined to the DS
    side. In this case, the very first data fop will be a composite
    operation on the DS: ->open(), ->fop().

### stat
Input given is GFID of the object on which the FOP (stat) needs to be performed,
  - Generate on disk path, /0xXX/0xYY/0xXXYYABCD
  - stat (ondisk_path) and return status

Other FOPs that fall in the same pattern are,
  - truncate, access, readlink, symlink, open, opendir, statfs, setxattr,
  getxattr, removexattr, inodelk, entrylk, xattrop, setattr
  - Some of the above FOPs return pre/post stat information about the GFID
  which hence involves a stat after path conversion for the pre-stat information
  and then the actual FOP, followed by another stat for the post-stat
  information. The stat is done path based.

### fstat
Other FOPs that fall in the same pattern are,
  - ftruncate, readv, writev, flush, fsync, readdir, readdirp (see [Issue 1]),
  fsyncdir, fsetxattr, fgetxattr, fremovexattr, lk, finodelk, fentrylk,
  rchecksum, fxattrop, fsetattr, fallocate, discard, zerofill
  - Some of the above FOPs return pre/post stat information about the GFID
  which hence involves a stat after path conversion for the pre-stat information
  and then the actual FOP, followed by another stat for the post-stat
  information. The stat is done fd based.

### mknod, mkdir, create
There are 2 forms of these FOPs in the new on disk structure,
  - Input given is GFID, args
  - Input given is ParGFID, basename, GFID

As the name exists on one subvolume and the inode on another, DHT2 would
initiate the operation as a transaction and hence POSIX would need to handle the
above 2 forms of entry creation.

Input given is GFID, args
  - Generate path as, /0xXX/0xYY/0xXXYYABCD
  - mkdir/mknod/create the generated path
  - Set, acls (from xdata), xattrs (from xdata)
  - Return

Input given is ParGFID, basename, GFID
  - Generate path as, /0xXX/0xYY/0xXXYYABCD/basename
  - mkdir/mknod/create the generated path
  - Set xattr to store the GFID for the basename
  - Return

The impact of doing this as a transaction by DHT2 and its impact on xlators
below DHT2 needs to be understood better [Issue 2](#Issue 2)

### unlink, rmdir
These FOPs also have 2 forms, one where the GFID representation is removed and
the other where the name is removed. As a result it follows something similar
to [mknod, mkdir, create](#mknod, mkdir, create) in terms of implementation.

It also has the same transaction impact to xlators below DHT2,
as in [Issue 2](#Issue 2)

unlink would further need to conditionally unlink the GFID only if total hard
links to the inode would be 0 at the end of the operation.

### rename
There is some analysis to be done when renaming across directories, but simple
renames, i.e rename of objects within the same directory, should just fall on
one subvolume that contains the oldname, and hence require a simple rename
at the POSIX layer.

### link
<TBD> The thought is to maintain a link count at the GFID as an xattr, which
would hence need to be incremented as a xattrop (or atomically), and then the
new name linked into the parent as needed. This second part of name creation
would be the same as a create against with (ParGFID, basename, GFID), hence
link would only have one form that increments the link count on the GFID.

## Issue 1
The operation of looking up a name is a success if the name is found. We return
the GFID for the name back to the client, or just back up the xlator stack on
the brick process.

As a result either all xlators need to treat EREMOTE as a special non-error, or
we return no error up the stack and an empty iatt information from lookup.

Simpler thing to do is return an empty iatt information back up the stack, so
that there is no special error handling required across all xlators.

The same problem exists for readdirp where all entries are just GFIDs, as a
result, DHT2 can just do a readdir, and not a readdirp, as readdirp would be
an implementation in DHT2 xlator, where a stat on each entry returned by
POSIX has to be performed. This implies no readdirp implementation at POSIX
for the first prototype.

In the second prototype, when the file GFID is co-located with the ParGFID
readdirp could just return GFID information for some file whose GFID is not
located in the same subvolume as the ParGFID.

## Issue 2
Any entry creation or removal is a 2 step process starting from DHT2 downwards.
As a result we need to determine, how we are going to let POSIX know this is
a GFID operation or a name operation is an open problem.

We also need to understand the impact of this across other xlators.
