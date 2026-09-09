/* Bundled assets are read-only; user data must survive moving/replacing the app. */
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>

static int wrath_make_directory_tree(const char *path)
{
    char copy[4096];
    if (strlen(path) >= sizeof(copy)) { errno = ENAMETOOLONG; return -1; }
    strcpy(copy, path);
    for (char *p = copy + 1; ; ++p) {
        if (*p != '/' && *p != '\0') continue;
        char end = *p; *p = '\0';
        if (mkdir(copy, 0700) && errno != EEXIST) return -1;
        struct stat info;
        if (stat(copy, &info) || !S_ISDIR(info.st_mode)) { errno = ENOTDIR; return -1; }
        *p = end;
        if (!end) return 0;
    }
}

static int wrath_macos_state_paths(char *save, size_t save_size, char *run, size_t run_size)
{
    char root[4096];
    const char *override = getenv("WRATH_STATE_ROOT");
    const char *home = getenv("HOME");
    if (!home || !*home) {
        struct passwd *user = getpwuid(getuid());
        home = user ? user->pw_dir : NULL;
    }
    int length;
    if (override && *override) length = snprintf(root, sizeof(root), "%s", override);
    else if (home && *home) length = snprintf(root, sizeof(root), "%s/Library/Application Support/WumpaForge", home);
    else { errno = ENOENT; return -1; }
    if (length < 0 || (size_t)length >= sizeof(root)) { errno = ENAMETOOLONG; return -1; }
    int a = snprintf(save, save_size, "%s/saves", root);
    int b = snprintf(run, run_size, "%s/run", root);
    if (a < 0 || b < 0 || (size_t)a >= save_size || (size_t)b >= run_size) {
        errno = ENAMETOOLONG; return -1;
    }
    return wrath_make_directory_tree(save) || wrath_make_directory_tree(run) ? -1 : 0;
}
