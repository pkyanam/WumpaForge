/* Actual POSIX translator plus real filesystem spelling assertions. */
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int ambiguous_entries;
static struct dirent *fixture_readdir(DIR *directory)
{
    if (!ambiguous_entries) return readdir(directory);
    static struct dirent entry;
    if (ambiguous_entries==1) {strcpy(entry.d_name,"Twin");ambiguous_entries=2;return &entry;}
    if (ambiguous_entries==2) {strcpy(entry.d_name,"twin");ambiguous_entries=3;return &entry;}
    return NULL;
}
#define readdir fixture_readdir
#include "kernel_path.c"
#undef readdir
const char *xbox_LookupSymbolicLink(const char *name) {(void)name;return NULL;}
void xbox_log(int level,const char *subsystem,const char *format,...) {(void)level;(void)subsystem;(void)format;}
static void make_dir(const char *p) {assert(!mkdir(p,0700));}
static void make_file(const char *p) {FILE *f=fopen(p,"wb");assert(f);assert(fwrite("original",1,8,f)==8);assert(!fclose(f));}
static void translated(const char *guest,const char *expected)
{
    char output[MAX_PATH];assert(xbox_translate_path(guest,output,sizeof(output)));
    if(strcmp(output,expected))fprintf(stderr,"Expected %s, got %s\n",expected,output);
    assert(!strcmp(output,expected));assert(!strcmp(xbox_LastHostPath(),expected));
}
int main(void)
{
    make_dir("Game");make_dir("Saves");make_dir("Game/Crashdat");make_dir("Game/Crashdat/levels");
    make_dir("Game/Crashdat/levels/b");make_dir("Game/Crashdat/levels/b/intro2");
    make_file("Game/Crashdat/levels/b/intro2/Control.cut");
    make_dir("Saves/tdata");make_dir("Saves/tdata/Existing");make_file("Saves/tdata/Existing/Save.Dat");
    xbox_path_init("Game","Saves");
    translated("D:\\Crashdat\\levels\\b\\intro2\\control.cut","Game/Crashdat/levels/b/intro2/Control.cut");
    translated("D:\\CRASHDAT\\LEVELS\\B\\INTRO2\\CONTROL.CUT","Game/Crashdat/levels/b/intro2/Control.cut");
    translated("D:\\Crashdat\\levels\\b\\intro2\\Control.cut","Game/Crashdat/levels/b/intro2/Control.cut");
    translated("T:\\existing\\save.dat","Saves/tdata/Existing/Save.Dat");
    translated("T:\\existing\\NewSave.Dat","Saves/tdata/Existing/NewSave.Dat");
    assert(access("Saves/tdata/Existing/NewSave.Dat",F_OK)<0); /* lookup did not create it */
    translated("T:\\Existing\\.\\unused\\..\\save.dat","Saves/tdata/Existing/Save.Dat");
    char output[MAX_PATH];
    assert(!xbox_translate_path("D:\\..\\Saves\\tdata",output,sizeof(output))&&errno==EACCES);
    assert(!xbox_translate_path("D:\\Crashdat\\..\\..\\Saves",output,sizeof(output))&&errno==EACCES);
    assert(!xbox_translate_path("T:\\missing\\save.dat",output,sizeof(output))&&errno==ENOENT);
    assert(!xbox_translate_path("T:\\Existing\\Save.Dat\\child",output,sizeof(output))&&errno==ENOTDIR);
    assert(!xbox_translate_path("D:\\Crashdat",output,4)&&errno==ENAMETOOLONG);
    assert(!symlink("../Saves","Game/escape"));
    assert(!xbox_translate_path("D:\\escape\\tdata",output,sizeof(output))&&errno==ELOOP);
    assert(!symlink("../Saves/tdata/Existing/Save.Dat","Game/link"));
    assert(!xbox_translate_path("D:\\link",output,sizeof(output))&&errno==ELOOP);
    /* APFS cannot store case-colliding siblings: inject only those dirents to
       exercise ambiguity rejection, while every spelling test above is real. */
#ifdef WRATH_ANDROID_TV
    ambiguous_entries=1;
    assert(!xbox_case_path("Game","TwIn",output,sizeof(output),0)&&errno==EEXIST);
    ambiguous_entries=0;
    assert(xbox_case_path("Saves","NewNamespace/Child",output,sizeof(output),1));
    assert(!strcmp(output,"Saves/NewNamespace/Child"));assert(access("Saves/NewNamespace",F_OK)<0);
#endif
    translated("Z:\\FreshSave.dat","Saves/Cache/Partition5/FreshSave.dat");
    struct stat st;assert(!stat("Saves/Cache/Partition5",&st)&&S_ISDIR(st.st_mode));
    FILE *f=fopen("Game/Crashdat/levels/b/intro2/Control.cut","rb");assert(f);char bytes[9]={0};assert(fread(bytes,1,8,f)==8);fclose(f);assert(!strcmp(bytes,"original"));
    puts("PASS: canonical asset/save case, missing create leaf, namespace creation, dot normalization, boundaries, symlinks, ambiguity, unchanged files");return 0;
}
