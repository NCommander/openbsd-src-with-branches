
#include <stand.h>
#include <ufs.h>

struct fs_ops file_system[] = {
	{ ufs_open,  ufs_close,  ufs_read,  ufs_write,  ufs_seek,
	  ufs_stat,  ufs_readdir  },
	{ null_open, null_close, null_read, null_write, null_seek,
	  null_stat, null_readdir }
};
int nfsys = sizeof(file_system)/sizeof(file_system[0]);

