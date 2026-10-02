#pragma once
#include "ambient_generation.h"
#include <stddef.h>
#include <sys/types.h>
#define COMPANION_GENERATION_PATH_BYTES 1024U
/* Optional finite syscall seam; NULL selects real durable I/O and OS CSPRNG. */
typedef struct {
    ssize_t (*write_bytes)(int, const void *, size_t);
    int (*sync_file)(int);
    int (*sync_directory)(int);
    int (*replace_file)(const char *, const char *);
    int (*random_bytes)(void *, size_t);
} companion_generation_io_t;
typedef struct {
    int lock_fd;
    bool open, poisoned;
    char path[COMPANION_GENERATION_PATH_BYTES];
    companion_generation_io_t io;
    ambient_generation_backend_t backend;
} companion_generation_store_t;
/* Explicit existing parent directory; no default user path/config installation.
 * Single owner held with flock for entire lifetime. Serialize calls on one queue.
 * Keep .lock and epoch file together; deleting/restoring them breaks uniqueness.
 * A retained lock with missing state is rejected, never reset to epoch zero. */
bool companion_generation_store_open(companion_generation_store_t *, const char *,
                                    const companion_generation_io_t *);
void companion_generation_store_close(companion_generation_store_t *);
