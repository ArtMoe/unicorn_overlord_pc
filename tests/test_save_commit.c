/* Save files opened for writing replace the old file only when closed (a crash keeps it). */
#include "../src/runtime_file.c"
#include <assert.h>
static int32_t guest_errno;
int32_t *runtime_errno(void) { return &guest_errno; }
int32_t runtime_guest_errno(int e) { return e; }
uintptr_t runtime_lookup(const RuntimeExport *table,size_t n,const char *name) {
    for (size_t i=0;i<n;++i) if (!strcmp(table[i].name,name)) return (uintptr_t)table[i].function;
    return 0;
}
static void read_file(const char *path,char *out,size_t size) {
    memset(out,0,size);
    FILE *f=fopen(path,"rb"); assert(f);
    assert(fread(out,1,size-1,f)<size);
    fclose(f);
}
static int listed(int dir,const char *suffix) {
    char entries[4096]; int64_t count=do_getdents(dir,entries,sizeof(entries),NULL); assert(count>=0);
    for (int64_t p=0;p<count;) {
        uint16_t length; memcpy(&length,entries+p+4,2);
        const char *name=entries+p+8; size_t n=strlen(name), m=strlen(suffix);
        if (n>=m && !strcmp(name+n-m,suffix)) return 1;
        p+=length;
    }
    return 0;
}
int main(void) {
    char root[]="/tmp/vw-save-commit-XXXXXX";
    assert(mkdtemp(root));
    char game[512],user[512],saves[512],file[600];
    snprintf(game,sizeof(game),"%s/game",root);
    snprintf(user,sizeof(user),"%s/user",root);
    snprintf(saves,sizeof(saves),"%s/save",root);
    snprintf(file,sizeof(file),"%s/userdata",saves);
    assert(!mkdir(game,0755) && !mkdir(saves,0755));
    FILE *f=fopen(file,"wb"); assert(f); assert(fputs("old",f)>=0); assert(!fclose(f));
    runtime_file_configure(game,user);
    assert(!runtime_file_mount("/savedata0",saves));

    /* Written through a copy: the file keeps its old contents until close. */
    int fd=(int)do_open("/savedata0/userdata",0x1|0x400,0); assert(fd>=3);
    assert(do_write(fd,"new data",8)==8);
    char content[32]; read_file(file,content,sizeof(content)); assert(!strcmp(content,"old"));
    GuestStat info; assert(!do_stat("/savedata0/userdata",&info) && info.size==8); /* the copy */
    int dir=(int)do_open("/savedata0",0x20000,0); assert(dir>=3);
    assert(!listed(dir,".vwsave")); assert(!do_close(dir));
    assert(!do_close(fd));
    read_file(file,content,sizeof(content)); assert(!strcmp(content,"new data"));

    /* Opened for writing, never written: nothing changes. */
    fd=(int)do_open("/savedata0/userdata",0x2,0); assert(fd>=3); assert(!do_close(fd));
    read_file(file,content,sizeof(content)); assert(!strcmp(content,"new data"));

    /* A crash mid-save: the copy is left behind, the file is intact; the next mount removes it. */
    fd=(int)do_open("/savedata0/userdata",0x1|0x400,0); assert(fd>=3);
    assert(do_write(fd,"torn",4)==4);
    char stale[700]; snprintf(stale,sizeof(stale),"%s.%u.vwsave",file,temp_serial);
    assert(!access(stale,F_OK));
    files[fd]=(File){0}; /* the process dies: no close */
    read_file(file,content,sizeof(content)); assert(!strcmp(content,"new data"));
    assert(!runtime_file_mount("/savedata0",saves));
    assert(access(stale,F_OK));

    /* New files are created at close too. */
    fd=(int)do_open("/savedata0/param",0x1|0x200,0644); assert(fd>=3);
    assert(do_write(fd,"p",1)==1);
    char param[600]; snprintf(param,sizeof(param),"%s/param",saves);
    assert(access(param,F_OK));
    assert(!do_close(fd)); assert(!access(param,F_OK));
    puts("save commit: ok");
    return 0;
}
