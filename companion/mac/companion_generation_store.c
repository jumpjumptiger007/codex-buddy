#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include "companion_generation_store.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/random.h>
#endif
static int os_random(void *bytes, size_t size)
{
#if defined(__APPLE__)
    return getentropy(bytes, size);
#elif defined(__linux__)
    return getrandom(bytes, size, 0) == (ssize_t)size ? 0 : -1;
#else
    (void)bytes; (void)size; return -1;
#endif
}
static int durable_sync(int fd)
{
    if (fsync(fd) != 0) return -1;
#if defined(__APPLE__)
    if (fcntl(fd, F_FULLFSYNC) != 0) return -1;
#endif
    return 0;
}
static bool load(void *context, uint32_t *epoch)
{
    companion_generation_store_t *s = context;
    if (!s->open || s->poisoned || !epoch) return false;
    int fd = open(s->path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return false;
    uint8_t bytes[9]; struct stat st;
    ssize_t n = read(fd, bytes, sizeof(bytes));
    bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_uid == geteuid() &&
        !(st.st_mode & 0077) && n == 8 && !memcmp(bytes, "GEN1", 4);
    if (close(fd) != 0) ok = false;
    if (!ok) return false;
    *epoch = ((uint32_t)bytes[4] << 24) | ((uint32_t)bytes[5] << 16) |
        ((uint32_t)bytes[6] << 8) | bytes[7];
    return true;
}
static bool save(void *context, uint32_t epoch)
{
    companion_generation_store_t *s = context;
    if (!s->open || s->poisoned) return false;
    char temporary[COMPANION_GENERATION_PATH_BYTES + 16];
    char directory[COMPANION_GENERATION_PATH_BYTES];
    snprintf(temporary, sizeof(temporary), "%s.XXXXXX", s->path);
    strcpy(directory, s->path);
    char *slash = strrchr(directory, '/');
    if (slash == directory) slash[1] = 0;
    else if (slash) *slash = 0;
    else strcpy(directory, ".");
    int fd = mkstemp(temporary);
    if (fd < 0) { s->poisoned = true; return false; }
    uint8_t bytes[8] = {'G','E','N','1', (uint8_t)(epoch >> 24),
        (uint8_t)(epoch >> 16), (uint8_t)(epoch >> 8), (uint8_t)epoch};
    bool ok = s->io.write_bytes(fd, bytes, sizeof(bytes)) == (ssize_t)sizeof(bytes) &&
        s->io.sync_file(fd) == 0;
    if (close(fd) != 0) ok = false;
    bool renamed = ok && s->io.replace_file(temporary, s->path) == 0;
    if (!renamed) { (void)unlink(temporary); ok = false; }
    if (renamed) {
        int parent = open(directory, O_RDONLY | O_DIRECTORY);
        ok = parent >= 0 && s->io.sync_directory(parent) == 0;
        if (parent >= 0 && close(parent) != 0) ok = false;
    }
    if (!ok) s->poisoned = true; /* Uncertain durability cannot emit/reuse an ID. */
    return ok;
}
static bool random_part(void *context, uint32_t *value)
{
    companion_generation_store_t *s = context;
    return s->open && !s->poisoned && s->io.random_bytes(value, sizeof(*value)) == 0;
}
void companion_generation_store_close(companion_generation_store_t *s)
{
    if (!s || !s->open) return;
    (void)close(s->lock_fd);
    memset(s, 0, sizeof(*s)); s->lock_fd = -1;
}
bool companion_generation_store_open(companion_generation_store_t *s, const char *path,
                                     const companion_generation_io_t *ops)
{
    if (!s || s->open || !path || !*path || strlen(path) >= sizeof(s->path)) return false;
    memset(s, 0, sizeof(*s)); s->lock_fd = -1;
    strcpy(s->path, path);
    s->io = (companion_generation_io_t){write, durable_sync, fsync, rename, os_random};
    if (ops) {
        if (ops->write_bytes) s->io.write_bytes = ops->write_bytes;
        if (ops->sync_file) s->io.sync_file = ops->sync_file;
        if (ops->sync_directory) s->io.sync_directory = ops->sync_directory;
        if (ops->replace_file) s->io.replace_file = ops->replace_file;
        if (ops->random_bytes) s->io.random_bytes = ops->random_bytes;
    }
    char lock_path[COMPANION_GENERATION_PATH_BYTES + 8];
    snprintf(lock_path, sizeof(lock_path), "%s.lock", path);
    bool fresh = true;
    int fd = open(lock_path, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0 && errno == EEXIST) { fresh = false; fd = open(lock_path, O_RDWR | O_NOFOLLOW); }
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        (st.st_mode & 0077) || flock(fd, LOCK_EX | LOCK_NB) != 0) { close(fd); return false; }
    s->lock_fd = fd; s->open = true;
    s->backend = (ambient_generation_backend_t){load, save, random_part, s};
    uint32_t epoch;
    if (load(s, &epoch)) return true;
    /* Existing or corrupt state never silently starts again at zero. */
    if (fresh && access(path, F_OK) != 0 && errno == ENOENT && save(s, 0)) return true;
    companion_generation_store_close(s); return false;
}
