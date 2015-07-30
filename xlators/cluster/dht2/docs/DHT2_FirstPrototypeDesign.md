# DHT2 First prototype design

This is the design elaboration for the first prototype of DHT2. For initial
reading on design and motivation for some of the basic on disk constructs
(i.e directory on a single brick, MDS and DS segregation, flatter backend
structure (no ancestors)) read,
  - https://docs.google.com/document/d/1nJuG1KHtzU99HU9BK9Qxoo1ib9VXf2vwVuHzVQc_lKg
  - https://docs.google.com/document/d/1rcqAPxMAjSJfbUQmqKD1Eo_lswQ71KwWDI2w2Srzx0Y

There are parallels to local file systems and their ondisk and in memory
representation of the file system structure. For example, the entry name and
GFID separation that is achieved in the design below is a classic filesystem
directory inode (i.e file names and inode numbers) and file inode separation.
Thus a brief understanding on these would improve understanding of the content
presented here.

## On disk view:
NOTE: The absolute root (i.e /  in the examples below) could be any part of
the underlying filesystem, as presented to Gluster for its store.

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

  NOTE: Values in round brackets above, is either informational or stored as
  extended attributes on the file or directory object as its meta data

2. Object types in MDS
  - Gluster Directory inode
    - In the above example, 0x00000001 is a Gluster directory inode
    - 0x00000001 itself is a GFID (or an inode number) in gluster FS
    - It stores the metadata about the directory inode that it refers to
      - i.e attrs, times, size, xattrs, etc.
    - IOW, it is a regular inode, but of type directory, and hence created as
      a regular directory on the underlying local file system
  - Gluster Name entry
    - In the above disk view, 'Dir1' or 'File1' are Gluster Name entries
    - It stores the name of the entry, and which inode it points to (i.e its
      GFID)
      - The inode that it points to is stored as a GFID, which is stored as an
      xattr on the file that is created on the local file system
    - The name to parent relationship is defined by the virtue of this name
      being present under the parent GFID Gluster directory inode
    - IOW, it is a regular file, with no attr information stored, and created
      as a regular file on the local file system under the parent
      GFID heirarchy

### DataServer (DS):
1. Consolidated view across all DS bricks
  /0xFF/0x00/0xFF000001
  /0xFF/0x00/0xFF000002
  /0xFF/0x00/0xFF000003

2. Object types in DS
  - Gluster File inode
    - A regular file on the local file system, that is created with the
      GFID as its name
    - Like a regular filesystem inode, stores the metadata regarding this
      file inode
    - File data is written/read to/from this object and left to the local
      file system to maintain the data block mappings
      - IOW, it is a regular file that Gluster brick processes would do
        data and metadata operations on

## Details of inode distribution
These are presented as highlights than in detail as the intention is to keep
layout generation and association to subvolumes a plugable, evolving entity
that helps reduce data motion in the even of cluster exapnsion or contraction.

- Layouts are at the volume level, and stored centrally
- Hashing is based on GFID, hence subvol is chosen based on hash of GFID
- Post lookups the cached inode information helps redirect FOPs to the
  right subvolume.
- To begin with the layout range is divided into equal number of parts, as
  there are subvolumes on the MDS or DS side of the DHT2 volume graph

<TBD> ## Virtual file system construction by DHT2 on the client
Root inode has a special reserved GFID, which is 0x00000001 (shortened form).
As a result looking up root is done by the client side DHT xlator based
on rootGFID hash.

Once root is determined, further lookups would build the inode and dentry
table on the client to provide the file system view.

## Details on FOP implementation

### Limitations in initial implementation
A. We do not consider rebalance, hence nothing is out of balance and in
  the right place always
B. We need to determine order of operations when doing composite operations,
  like create, unlink
  - Meaning, what is created first the inode or the name entry on the parent
  - What is unlinked first
  * We should be able to find some useful patterns when looking at other local
    file system implementations for the same

### lookup
  - Named lookup (parent gfid (pgfid)/parent inode (pinode) + bname)
    - subvol = layout_search (mds, pgfid) || subvol = pinode->subvol
    - lookup (subvol, loc)
    - Success
      - subvol = layout_search (ds, xattr->gfid)
      - lookup (subvol, gfid)
      - Success
        - update inode
        - return results
      - Failure
        - [A] prevents this from occuring
        - [B] cases may cause a failure to find the GFID and needs to be handled
        - IOW, not expected except in race conditions
        - Call this an dorphan (dentry orphan) for now
    - Failure
      - [A] pevents any other possibility other than ENOENT
      - return ENOENT
  - Nameless lookup (gfid) (can be a file or a directory,
                                need to search in both spaces)
    - subvols[1] = layout_search (mds, gfid)
    - subvols [2] = layout_search (ds, gfid)
    - lookup (subvols, gfid)
    - Success
      - update inode
      - return results
    - Failure
      - if found on both, then critical error
      - if not found anywhere, return ESTALE
        - Again due to not considering [A] we do not need to
                lookup everywhere on a failure

### stat
  - subvol = layout_search (inode->type? mds : ds, gfid) || inode->subvol
  - return stat (subvol, gfid)

### mkdir
  - subvol = layout_search (mds, gfid)
  - mkdir (subvol, gfid)
  - Success
    - mkdir (pinode->subvol, name, gfid)
    - Success
      - return 0
    - Failure
      - Race to create name entry won by another client
      - async cleanup oinode (orphan inode), i.e the created gfid on subvol
      - return EEXIST
  - Failure
    - return ESYS

### rmdir
  - subvol = layout_search (mds, gfid) || inode->subvol
  - rmdir (subvol, gfid)
  - Success
    - Directory empty, also no further creates of dentries possible as parent
      inode (i.e GFID) does not exist, dentry for this directory at this point
      is an dorphan
    - rmdir (pinode->subvol, name, gfid)
    - Success
      - return 0
    - Failure
      - This should not fail, other than for connectivity/brick unavailability
        reasons, at this point we leave a dorphan behind
  - Failure
    - Either not empty, or unable to remove due to other systemic reasons
    - return error

### readdirp
  - inodes = readdir (fd->subvol, fd, args...)
  - for each inode in inodes
    - subvols[1] = layout_search (mds, inode->gfid)
    - subvols[2] = layout_search (ds, inode->gfid)
    - stat (subvols, gfid)
    - fill up stat info in return to readdirp call

### create
  - subvol = layout_search (ds, gfid)
  - create (subvol, gfid)
  - Success
    - create (pinode->subvol, name, gfid)
    - Success
      - return 0
    - Failure
      - Race to create name entry won by another client
      - async cleanup oinode (orphan inode), i.e the created gfid on subvol
      - return EEXIST
  - Failure
    - return ESYS

### setattr
  - subvol = layout_search (inode->type? mds : ds, gfid) || inode->subvol
  - return setattr (subvol, gfid)

### open(dir)
  - subvol = layout_search (inode->type? mds : ds, gfid) || inode->subvol
  - return open(dir) (subvol, gfid)

### readv
  - subvol = inode->subvol
  - return readv (subvol, fd, args...)

### writev
  - subvol = inode->subvol
  - return writev (subvol, fd, args...)

### unlink
  - subvol = layout_search (ds, gfid) || inode->subvol
  - unlink (subvol, gfid)
  - Success
    - open fd is preserved by underlying POSIX fs, so no harm done there
    - rmdir (pinode->subvol, name, gfid)
    - Success
      - return 0
    - Failure
      - This should not fail, other than for connectivity/brick unavailability
        reasons, at this point we leave a dorphan behind
  - Failure
    - return error

### rename
  - Simple rename: i.e rename a directory or a file within the same parent and
    newname does not exist
  - rename (pgfid->subvol, oldname, newname)

### link
  - This is more involved, as we need to keep a link count on the gfid inode

### fsync

### fsyncdir

### lk

## Open problems, issues and limitations
1. readdirp would be very slow in the approach above, need ways to alleviate
  the same
2. Quota needs a rethink as ancestory in a brick is not present for accounting
  and other such needs. We need to possibly rework quota in any approach to
  be able to adapt to this change on disk. One such promising approach seems to
  be using XFS project quota like ideas in Gluster, see
    - http://oss.sgi.com/projects/xfs/papers/xfs_filesystem_structure.pdf
      - See, "Internal inodes" -> "Quota inodes"
3. lookup also translates into a 2 network RPC FOP, and needs improvement

## Appendix A: Directory as a file at the POSIX layer
## High level requirements
  - Cannot hardlink GFID name space to real name, as a file does not exist
    on the MDS as a real file, it is just an offset into a file that
    represents the directory
    - Also even if such a hard link is possible this is a link to the name
      entry for the file, not to its inode, as that exists as a separate
      object in the local file system anyway
    - Bottomline is that either with directory as a file or not, the current
      on disk format prevents a .glusterfs name space to exist in parallel on
      the disk and is actually not needed anyway
  - Need to have an case-insensitive name index for the same, helps SAMBA
    if at all possible
  - Can be sharded to provide hot directory immunity (Future)
  - Should retain d_off across replicas, i.e a d_off should be continuable on
    any available replica, this is one of the primary needs for this feature
    as well
