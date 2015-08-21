# DHT2 Weekly Report
Week:   002
Ending: Aug-20-1015

## Major milestones achieved:
  1. None
  - But, as this is the first WSR here is some information
    - First prototype design specification is ready and under implementation
      - DHT2 first prototype design:
          https://review.gerrithub.io/#/c/241786/
      - POSIX layer functional specification:
          https://review.gerrithub.io/#/c/243880/

## Tasks done:
  1. POSIX initial functional specification uploaded
  2. Initial set of WIP changed to POSIX layer uploaded

## Learning's or new open problems:
  1. There was an open concern on file handle abuse by untrusted clients and if
  DHT2 design worsens the situation
    - The short answer is that this is not a DHT2 problem, file handle abuse
    is something that NFSv3 has had to deal with and is resolved or solved
    (partially?) using RPCSec-GSS and also volatile file handles in NFSv4
    - Paper regarding the same,
      http://www.fsl.cs.stonybrook.edu/docs/nfscrack-tr/index.html
  2. Current quota implementation is weak when GFID based access is attempted
    - Also, when quota is combined with DHT2, to do accounting, it may be so
    that path walking could necessitate crossing MDS subvolumes making this a
    costly approach
    - Requested vmallika to investigate possibilities on XFS projectid based
    quota implementation for Gluster
    - Also, need to possibly improve quota performance based on a token system
    of consumption, so that accounting can be relaxed till limits are neared