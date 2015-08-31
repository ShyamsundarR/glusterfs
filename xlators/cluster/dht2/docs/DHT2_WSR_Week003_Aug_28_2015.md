# DHT2 Weekly Report
Week:   002
Ending: Aug-28-1015

## Major milestones achieved:
  1. None

## Tasks done:
  1. POSIX xlator refactoring to suit DHT2 Phase1 prototype in progress
    here, https://review.gerrithub.io/#/c/243885/
  2. Firmed up POSIX Phase1 functional specification,
    here, https://review.gerrithub.io/#/c/243880/
  3. Started maturing DHT2 Phase2 design,
    here, https://review.gerrithub.io/#/c/244752/

## Learning's or new open problems:
  1. open fd and unlink behavior
  As we progress into DHT2 Phase2, one of the issues that needed consideration
  was, where would an fd be opened and how would it prevent an unlink from
  removing the name and data blocks. The initial thoughts around this are,
    - Open the fd at the MDS
    - On the first fd based operation on the DS (which would be read/write
    /truncate), open and keep the fd open at the ds as well
    - On unlink, the name entry in the parent GFID is unlinked, and the inode
    on the MDS for the object, is marked for deletion, and unlinked when the
    last fd is closed against the same. This is also the point at which the data
    blocks for this object is unlinked/freed at the DS.
    - As the above required some transaction support, considering various
    failures that could occur in the process, and possibly would require some
    form of FS/brick journal to retain consistency of the file system and also
    to prevent inode leaks (i.e name entry being unlinked, and actual inode not
    freed)
  2. Small file creation and its scale in DHT2
    In DHT2 Phase2 design, a file inode additionally needs a extent GFID
    allocated in the DS space. This would be an additional RPC request, in case
    this is done on the first DS operation. This step needs optimization so that
    we do not suffer the penalty of the additional RPC.
      - One way to mitigate it is to allocate the first (or only at this point)
      extent, at the time for file inode creation at the MDS, and perform a
      delayed creation of the same on the DS. This and other thoughts need to be
      matured further.
  3. Reverse path walking/construction for directories
    In current Gluster, POSIX xlator maintains a soft link for a directory,
    to its PGFID:name entry. This allows constructing a full path on the brick
    given a GFID for a directory.
    In DHT2, this is not the case (yet) and it can be maintained as an extended
    attribute if required. As directories do not have hard links to them, there
    would be a unique name to GFID:ParGFID mapping, but maintaining it and
    ensuring renames work as well would need some thought.