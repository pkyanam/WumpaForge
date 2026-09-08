#include "kernel/kernel.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
/* External logging/link registries are immaterial to raw-device routing. */
void xbox_log(int level, const char *category, const char *fmt, ...)
{ (void)level; (void)category; (void)fmt; }
const char *xbox_LookupSymbolicLink(const char *link) { (void)link; return NULL; }
int main(void)
{
    char temp[] = "/tmp/wrath-partitions-XXXXXX", path[260], guest[128], game[260], save[260];
    assert(mkdtemp(temp));
    snprintf(game, sizeof(game), "%s/game", temp);
    snprintf(save, sizeof(save), "%s/saves", temp);
    assert(!mkdir(game, 0700));
    xbox_path_init(game, save);
    assert(xbox_translate_path("D:\\default.xbe", path, sizeof(path)));
    assert(!strncmp(path, game, strlen(game)));
    assert(xbox_translate_path("\\Device\\Harddisk0\\Partition1\\UDATA\\test", path, sizeof(path)));
    assert(!strncmp(path, save, strlen(save)) && strstr(path, "/UDATA/test"));
    assert(xbox_translate_path("Y:\\test", path, sizeof(path)));
    assert(!strncmp(path, save, strlen(save)) && strstr(path, "/Cache/Partition4/test"));
    assert(xbox_translate_path_for_open("\\Device\\Harddisk0\\Partition1", path, sizeof(path), TRUE));
    assert(!strcmp(path, save));
    struct stat root_st;
    assert(!stat(path, &root_st) && S_ISDIR(root_st.st_mode));
    assert(xbox_translate_path("\\Device\\Harddisk0\\partition0", path, sizeof(path)));
    struct stat st;
    assert(!stat(path, &st) && st.st_size == 0x80000 && st.st_blocks * 512 < 1048576);
    int fd = open(path, O_RDWR);
    assert(fd >= 0);
    uint8_t sector[512];
    assert(pread(fd, sector, sizeof(sector), 0x800) == sizeof(sector));
    for (unsigned i = 0; i < sizeof(sector); i++) assert(!sector[i]);
    uint32_t magic = 0x97315286;
    assert(pwrite(fd, &magic, sizeof(magic), 0x800) == sizeof(magic));
    close(fd);
    /* A subsequent translation/open must keep the game's sector intact. */
    assert(xbox_translate_path("\\Device\\Harddisk0\\Partition0\\", path, sizeof(path)));
    fd = open(path, O_RDONLY);
    uint32_t readback = 0;
    assert(pread(fd, &readback, sizeof(readback), 0x800) == sizeof(readback));
    assert(readback == magic);
    close(fd);
    for (unsigned part = 1; part <= 5; part++) {
        snprintf(guest, sizeof(guest), "\\Device\\Harddisk0\\Partition%u", part);
        assert(xbox_translate_path(guest, path, sizeof(path)));
        assert(!stat(path, &st) && st.st_size >= 500u * 1024u * 1024u);
        assert(st.st_blocks * 512 < 1048576);
        unlink(path);
    }
    snprintf(path, sizeof(path), "%s/Partition0.img", save);
    unlink(path);
    snprintf(path, sizeof(path), "%s/Cache/Partition4", save); rmdir(path);
    snprintf(path, sizeof(path), "%s/Cache", save); rmdir(path);
    rmdir(save); rmdir(game); rmdir(temp);
    puts("PASS: raw-device case matching, sparse geometry and persistent guest-owned cache metadata");
}
