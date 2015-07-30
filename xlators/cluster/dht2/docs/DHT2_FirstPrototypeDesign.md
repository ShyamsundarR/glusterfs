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
presented here. Some useful materials towards this would be,
  - <TBD>

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

2. Object types in MDS
  - Gluster Directory inode
    - In the above example, 0x00000001 is a Gluster directory inode
    - 0x00000001 itself is a GFID or an inode number in gluster FS
    - It stores the metadata about the directory inode that it refers to
      - i.e attrs, times, size, xattrs, etc.
    - IOW, it is a regular inode, but of type directory, and hence created as
        a regular directory on the local file system
  - Gluster Name entry
    - In the above disk view, 'Dir1' or 'File1' is a Gluster Name entry
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
<TBD> Further elaboration needed

- Layouts are at the volume level
- Hashing is based on GFID, hence subvol is chosen based on hash of GFID
- Post lookups the cached inode information helps redirect FOPs to the
  right subvolume.

## Virtual file system construction on the client
<TBD> Further elaboration as needed

Root inode has a special reserved GFID, which is 0x00000001 (shortened form).
As a result looking up root is done by the client side DHT xlator based
on rootGFID hash.

Once root is determined, further lookups would build the inode and dentry
table on the client to provide the file system view.

## Details on FOP implementation

### Limitations in initial implementation
A. We do not consider rebalance, hence nothing is out of balance and in
  the right place
B. We need to determine order of operations when doing composite operations,
  like create, unlink
  - Meaning, what is created first the inode or the name entry on the parent
  - What is unlinked first
  * We should be able to find some useful designs when looking at other local
    file system implementations

### FOPs
1. lookup
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

2. stat
  - subvol = layout_search (inode->type? mds : ds, gfid) || inode->subvol
  - return stat (subvol, gfid)

3. mkdir
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

4. rmdir
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

5. readdirp
  - inodes = readdir (fd->subvol, fd, args...)
  - for each inode in inodes
    - subvols[1] = layout_search (mds, inode->gfid)
    - subvols[2] = layout_search (ds, inode->gfid)
    - stat (subvols, gfid)
    - fill up stat info in return to readdirp call

6. create
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

7. setattr
  - subvol = layout_search (inode->type? mds : ds, gfid) || inode->subvol
  - return setattr (subvol, gfid)

8. open(dir)
  - subvol = layout_search (inode->type? mds : ds, gfid) || inode->subvol
  - return open(dir) (subvol, gfid)

9. readv
  - subvol = inode->subvol
  - return readv (subvol, fd, args...)

10. writev
  - subvol = inode->subvol
  - return writev (subvol, fd, args...)

11. unlink
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

12. rename
  - Simple rename: i.e rename a directory or a file within the same parent and
    newname does not exist
  - rename (pgfid->subvol, oldname, newname)

13. link
  - This is more involved, as we need to keep a link count on the gfid inode

14. fsync

15. fsyncdir

16. lk

## Details on cross functional requirements (as identified)


## Open problems, issues and limitations
<TBD>

=========================================
Rough area, to be factored into the above
=========================================
# Open high level problems:
- Which is the meta-data master?
  - How to keep meta-data in sync across DS and MDS

# Basic set of questions to analyze on the *any* design
- How to optimize readdirp?
  - Or even keep it sane, if we have 2 copies of the meta-data?
- How is hard link maintained
- How are fd's handled?
  - Anon and non-Anon fd's
  - Open has to do access checks
- Unlink and fd preservation
- How does lookup work?
- How does discover/nameless GFID based lookup, relate the objects in the 2 name spaces?
- How will quota function
  - IOW, how do we update consumption of a grandparent, or walk upwards the name tree
- How will we maintain parent GFID to name conversion, as this exists in current
  on disk design through soft links in .glusterfs for directories

# Basic list of FOPs to analyze in any approach
lookup, stat, mkdir, unlink, rmdir, rename, link, create, open, readv, writev, readdirp, fsyncdir, lk, setattr

# Directory as a file at the POSIX layer
## High level requirements
  - Cannot hardlink GFID name space to real name, as a file does not exist on the MDS as a real file, it is just an offset into a file that represents the directory
    - This could be a boon, if we could make the hard link, a link based on offset into the directory file, and thus given a GFID we can track its parent and it's name from this offset into the directory
  - Need to have an case-insensitive name index for the same, helps SAMBA if at all possible
  - Can be sharded to provide hot directory immunity (Future)
  - 

# Immidiate tasks
- Enable definition of a vol file using dht2
  - Done: code @ https://forge.gluster.org/~shyam/glusterfs-core/shyams-glusterfs/commits/dht2-prototype
- Create 'root' entry for the volume
- Modify POSIX xlator to adapt to,
  - pgfid/bname: translations to on disk format as presented above (MDS)
  - gfid: translation to on disk format as presented above (DS)
- Start with one set of DHT2 assumptions/model and proceed with the prototype

# Current Models
## Model 1
  - MDS is only name entries no meta data at all in this space
  - readdirp
    - Needs to be optimized by prefetcing and also maintaining caches on the client, that are updated using upcalls when needed
  - lookup is 2 FOPs one to get the GFID from the name entry and the next on the DS space to get the stat information

## Model 2
  - MDS is the master
  - GFID represents just data blocks and carries no meta data

## Model 3
  - MDS and DS are strong copies of the meta data

## Model 4
  - One of MDS or DS is a weak on disk cache of the meta data
    - When this is the DS, it makes no sense to have a weak cache there
    - So this boils down to DS is the master and MDS is a weak cache to prevent 2 network FOPs to get a lookup done
  
  
  
