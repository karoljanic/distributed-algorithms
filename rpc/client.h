#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#include "common.h"


typedef struct RFile File;

File *rfs_open(const char *pathname, const char *mode);
ssize_t rfs_read(File *f, void *buf, size_t count);
ssize_t rfs_write(File *f, const void *buf, size_t count);
off_t   rfs_lseek(File *f, off_t offset, int whence);
int     rfs_chmod(const char *pathname, mode_t mode);
int     rfs_unlink(const char *pathname);
int     rfs_rename(const char *oldpath, const char *newpath);
int     rfs_close(File *f);

#endif // RPC_CLIENT_H
