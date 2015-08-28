# Maintaining dentry and inode locality in a distributed file system

In order to scale the directory operations and to mainatin/improve consistency
across different file operations from different clients, distribution of
directories in gluster is being changed, from being present on all subvolumes,
to a single subvolume.

This brings about some interesting properties that are sought for such a
distribution, and what follows are a series of design steps that enables
these properties.

It presents a method for colocating the file inode with its directory entry,
and separating the file data as a result, to provide an optimization for all
entry operations.

## Properties:
  - Need to distribute directories across a subset of subvolumes, call these
    the MetaData Servers (MDS)
    - Reason: We cannot have all directories in one MDS, as that would not
      be a scalable architechture, making this one MDS a bottleneck for scale
      (this is a well known limitation in various distributed systems and pNFS
      that limit their active MDS counts to one)
  - As directories are distributed across MDS, hirearchy needs to be
    maintained across these distributed directories
  - File inode and its data need to be distributed separetly from its name to
    directory mapping (directory entry)
    - Reason: If we maintain the file inode data and directory entry on the
    same MDS server, then we will not achieve distribution of data, and hence
    not scale out the storage, but rather be limited to the set of MDS nodes
  - File inode should be immutable in location, on operations like rename and
    to permit operations like hard links, for maintaining file system
    consistency

## Design highlights
1. We define a range of tokens, where each token in that range can be assigned
  to a single MDS. Further each MDS is allocated the same # of tokens as the
  other, with the remainder of tokes distributed at random and atmost one
  among the MDS.
  - For the sake of simplicity we define this range as {1..T}
  - Each token is further referred to as t
  - T >> M, where M is the maximum # of MDS nodes
    - Assume for implementation we will choose large theoritical T values, such
    that T is >> M and can accomodate expansion of the cluster as it grows. For
    example T can be 65536 (or 16 bits)
  - Each MDS instance m hence has eitherof[T/M | (T/M) + 1] tokens assigned to it

2. When a directory is created, its inode location is determined by choosing
a random token value. Once this is chosen, the directory is created on
the MDS that token 't' belongs to.
  - It is also entirely possible for fair distribution to start at a random
    t value, and later do a incremental, cyclic allocation of token values,
    for further directories that are created
  - It is further possible to have weights assigned over time to each token
    so that when choosing a token further entry data is more fairly distributed
    and randomness does not overwhelm a single MDS
  - Ideally, the scheme to decide which token a directory would belong to can be
    a pluggable policy

3. The directories inode # or, GFID in Gluster terms, is encoded with the token
number in high order bits of the GFID. This provides the location for an GFID
encoded within itself when gluster is presented with the same by clients
  - Continuing the example, T = 65536, would mean 16 high order bits if the GFID
    is encoded with the token number
  - This still leaves 2^112 unique inodes per token (as a GFID is 128 bits worth
    of data in all)

4. File inode # or GFID, are generated with the same token number as the
directory that they belong to. This enables a files inode information to
co-reside in the same MDS as the directory it belongs to. Thus enabling faster
lookups, readdirp, create operations and also enabling higher consistency of
operations as a single MDS is involved in creating and unlinking these entries.
  - There are cases where this would untrue, notably,
    - hard links to files, where the file GFID already exists
    - renames of a file from a parent with a different token than the new target
    - These cases are handled as EREMOTE errors by the client to perform
      the inode operation as required on the remote location

5. File directory entry is created using the file name and its GFID under the
parent directory. Which, as the token values are the same, would belong to the
same MDS node.

6. Finally, to get better distribution of data, the file inode has another
GFID which is the data GFID. This GFID points to the files data inode, and can
have the same or a different encoding scheme as needed and resides on the data
nodes which could be separate from the MDS nodes.
  - We basically delink the file data block from the file itself, and hence
    can now place it anywhere, part of the MDS, part of its own cluster of nodes
    (i.e the DS), part of a totally external store

7. When the cluster grows, tokens are reassigned to the newer MDS nodes, which
ensures that all inodes belonging to that token migrates to its new location.
This enables preservation of file inodes and their directory entries within
the same node.

The optimization that is gained is for entry based FOPs on files, we need
a single network request to fulfill the request on the MDS of choice, when
file inode and its directory entry are colocated (except when the file is
non-trivialy renamed or has hard links). When the file inode and directory
entry are allowed to diverge by design, then we need at least 2 network
requests to fulfill an entry FOP always.

The fall out of the above approach is that, we need mechanisims to keep size
information in sync across the data block for a file and its file inode.

## Example optimization
NOTE: We only compare file inode colocation to directory entry, property
in the examples below, and not the older gluster DHT scheme to the one
presented here.

Case 1: directory entry and file inode are not colocated
Case 2: directory entry and file inode are colocated as per the scheme above

1) readdirp
Case 1:
  - inodes = readdir (fd->subvol, fd, args...)
    // Fetch the list of name, gfid entries from the directory
  - for each inode in inodes
    - subvols[1] = layout_search (mds, inode->gfid)
    - subvols[2] = layout_search (ds, inode->gfid)
    - stat (subvols, gfid)
      // make a request to both the file inode servers and directory inode
         servers to retrieve the stat information for the entry

  * The number of network requests in this scheme is 3, with the the last
    2 network operations being performed as a fan out.

Case 2:
  - inodes + stat = readdirp (fd->subvol, fd, args...)
    // Fetch stat and name info from the same MDS as file inode is colocated
  - for each inode in inodes
    - if (inode.stat == NULL && inode.fork == EREMOTE)
      // If it was a non-trivial rename or a hard link, follow the remote
         GFID to its actual subvol
      - subvol = layout_search (mds, inode.gfid)
      - stat (subvol, inode.gfid)
    
2) lookup
Case 1:
  - lookup on parent GFID, for provided basename
  - lookup GFID returned for actual stat information and inode type

Case 2:
  - lookup on parent GFID, for provided basename
  - On recieving EREMOTE, which could be due to the inode being a directory
    or a rename in the past, or a hard link
    - lookup GFID returned for actual stat information and inode type
