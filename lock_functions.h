#include <sys/file.h>
int file_lock_read(int fd);
int file_unlock(int fd);
int file_lock_write(int fd);
int file_try_lock_read(int fd);
int file_try_lock_write(int fd);
