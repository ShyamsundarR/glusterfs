#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "glfs.h"
#include "glfs-handles.h"
#include <string.h>
#include <time.h>

#ifdef DEBUG
static void
peek_stat (struct stat *sb)
{
        printf ("Dumping stat information:\n");
        printf ("File type:                ");

        switch (sb->st_mode & S_IFMT) {
                case S_IFBLK:  printf ("block device\n");            break;
                case S_IFCHR:  printf ("character device\n");        break;
                case S_IFDIR:  printf ("directory\n");               break;
                case S_IFIFO:  printf ("FIFO/pipe\n");               break;
                case S_IFLNK:  printf ("symlink\n");                 break;
                case S_IFREG:  printf ("regular file\n");            break;
                case S_IFSOCK: printf ("socket\n");                  break;
                default:       printf ("unknown?\n");                break;
        }

        printf ("I-node number:            %ld\n", (long) sb->st_ino);

        printf ("Mode:                     %lo (octal)\n",
                (unsigned long) sb->st_mode);

        printf ("Link count:               %ld\n", (long) sb->st_nlink);
        printf ("Ownership:                UID=%ld   GID=%ld\n",
                (long) sb->st_uid, (long) sb->st_gid);

        printf ("Preferred I/O block size: %ld bytes\n",
                (long) sb->st_blksize);
        printf ("File size:                %lld bytes\n",
                (long long) sb->st_size);
        printf ("Blocks allocated:         %lld\n",
                (long long) sb->st_blocks);

        printf ("Last status change:       %s", ctime(&sb->st_ctime));
        printf ("Last file access:         %s", ctime(&sb->st_atime));
        printf ("Last file modification:   %s", ctime(&sb->st_mtime));

        return;
}

static void
peek_handle (unsigned char *glid)
{
        int i;

        for (i = 0; i < GFAPI_HANDLE_LENGTH; i++)
        {
                printf (":%02x:", glid[i]);
        }
        printf ("\n");
}
#else /* DEBUG */
static void
peek_stat (struct stat *sb)
{
        return;
}

static void
peek_handle (unsigned char *id)
{
        return;
}
#endif /* DEBUG */

#define STAT_PERMS (S_IRWXU | S_IRWXG | S_IRWXO)
#define ERRLOG(fname, strarg1)   do  {                                  \
        fprintf (stderr, "%s:%d: %s(%s) %s\n", __FUNCTION__, __LINE__,  \
                         fname, strarg1, strerror (errno));             \
} while (0)

int
test_basics (char *argv[], glfs_t *fs1, glfs_t *fs2)
{
        int                 ret = 0;
        struct stat         sb = {0, };
        mode_t              mode = 00744;
        glfs_fd_t          *fd;

        /* Check root */
        ret = glfs_stat (fs1, "/", &sb);
        if (ret != 0) {
                ERRLOG ("stat", "/");
                goto out;
        }

        /* File creation */
        fd = glfs_creat (fs1, "/File1", O_CREAT, mode);
        if (fd < 0) {
                ERRLOG ("creat", "/File1");
                goto out;
        }
        glfs_close (fd);

        /* Changing file ownership */
        ret = glfs_chown (fs1, "/File1", 1234, -1);
        if (ret != 0) {
                ERRLOG ("chown", "/File1");
                goto out;
        }

        ret = glfs_stat (fs1, "/File1", &sb);
        if (ret != 0) {
                ERRLOG ("stat", "/File1");
                goto out;
        }
        if (sb.st_uid  != 1234) {
                ERRLOG ("stat", "/File1");
                goto out;
        }

        /* Synthetic mkdir and stat/lookup that directory */
        /* Create file within subdirectory */
        /* Synthetic non-colocated file creation and lookup of that file */

        ret = 0;
out:
        return ret;
}

int
main (int argc, char *argv[])
{
        glfs_t          *fs2 = NULL, *fs = NULL;
        int              ret = 0;

        if (argc < 3) {
                fprintf (stderr, "Expects following args\n\t%s <volname>"
                         " <hostname> [log_location]\n", argv[0]);
                return -1;
        }

        fs = glfs_new (argv[1]);
        if (!fs) {
                fprintf (stderr, "glfs_new: returned NULL\n");
                return -1;
        }

/*      TODO: We may want to just create a volfile that is DHT2+POSIX2
        to test just this functionality sans other things in the stack
        ret = glfs_set_volfile (fs, "/tmp/posix.vol"); */

        ret = glfs_set_volfile_server (fs, "tcp", argv[2], 24007);
        if (ret != 0) {
                fprintf (stderr, "glfs_set_volfile_server: retuned %d\n", ret);
                goto out;
        }

/*      ret = glfs_set_volfile_server (fs, "unix", "/tmp/gluster.sock", 0); */

        if (argc == 4)
                ret = glfs_set_logging (fs, argv[3], 7);
        else
                ret = glfs_set_logging (fs, "/dev/null", 7);
        if (ret != 0) {
                fprintf (stderr, "glfs_set_logging: returned %d\n", ret);
                goto out;
        }

        ret = glfs_init (fs);
        if (ret) {
                fprintf (stderr, "glfs_init: returned %d\n", ret);
                goto out;
        }

        sleep (2);

        fs2 = glfs_new (argv[1]);
        if (!fs2) {
                fprintf (stderr, "glfs_new: returned NULL\n");
                goto out;
        }

/*      ret = glfs_set_volfile (fs2, "/tmp/posix.vol"); */

        ret = glfs_set_volfile_server (fs2, "tcp", argv[2], 24007);
        if (ret != 0) {
                fprintf (stderr, "glfs_set_volfile_server: retuned %d\n", ret);
                goto out;
        }

        if (argc == 4)
                ret = glfs_set_logging (fs2, argv[3], 7);
        else
                ret = glfs_set_logging (fs2, "/dev/stderr", 7);
        if (ret != 0) {
                fprintf (stderr, "glfs_set_logging: returned %d\n", ret);
                goto out;
        }

        ret = glfs_init (fs2);
        if (ret) {
                fprintf (stderr, "glfs_init: returned %d\n", ret);
                goto out;
        }

        /* Basics: Consider this a brief health check */
        ret = test_basics (argv, fs, fs2);

out:
        if (fs)
                glfs_fini (fs);

        if (fs2)
                glfs_fini (fs2);

        return ret;
}
