#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "readdir.h"

/**********************************************************************
 * Implement dirent-style opendir/readdir/closedir on Window 95/NT
 *
 * Functions defined are opendir(), readdir() and closedir() with the
 * same prototypes as the normal dirent.h implementation.
 *
 * Does not implement telldir(), seekdir(), rewinddir() or scandir(). 
 * The dirent struct is compatible with Unix, except that d_ino is 
 * always 1 and d_off is made up as we go along.
 *
 * The DIR typedef is not compatible with Unix.
 **********************************************************************/

API_EXPORT(DIR *) opendir(const char *dir)
{
    DIR *dp;
    char *filespec;
    long handle;
    int index;

    filespec = malloc(strlen(dir) + 2 + 1);
    strcpy(filespec, dir);
    index = strlen(filespec) - 1;
    if (index >= 0 && (filespec[index] == '/' || filespec[index] == '\\'))
        filespec[index] = '\0';
    strcat(filespec, "/*");

    dp = (DIR *)malloc(sizeof(DIR));
    dp->offset = 0;
    dp->finished = 0;
    dp->dir = strdup(dir);

    if ((handle = _findfirst(filespec, &(dp->fileinfo))) < 0) {
        if (errno == ENOENT)
            dp->finished = 1;
        else
        return NULL;
    }

    dp->handle = handle;
    free(filespec);

    return dp;
}

API_EXPORT(struct dirent *) readdir(DIR *dp)
{
    if (!dp || dp->finished) return NULL;

    if (dp->offset != 0) {
        if (_findnext(dp->handle, &(dp->fileinfo)) < 0) {
            dp->finished = 1;
            return NULL;
        }
    }
    dp->offset++;

    strncpy(dp->dent.d_name, dp->fileinfo.name, _MAX_FNAME);
    dp->dent.d_ino = 1;
    dp->dent.d_reclen = strlen(dp->dent.d_name);
    dp->dent.d_off = dp->offset;

    return &(dp->dent);
}

API_EXPORT(int) closedir(DIR *dp)
{
    if (!dp) return 0;
    _findclose(dp->handle);
    if (dp->dir) free(dp->dir);
    if (dp) free(dp);

    return 0;
}
