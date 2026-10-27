/* Common dialogs (message, save data, IME, error, sign-in, PSN dialogs): no system UI is
 * drawn; each dialog does what the player would have done (the IME takes keyboard text). */
#define _GNU_SOURCE
#include "runtime_offline.h"
#include "gpu/vwgpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DIALOG_NOT_INITIALIZED ((int32_t)0x80B80003)

/* ---- Common dialogs: nothing is displayed; an opened dialog finishes. ---- */
typedef struct { const char *name; int initialized, status; uint32_t button; } Dialog;
static Dialog dialogs[]={{"CommonDialog",0,0,0},{"MsgDialog",0,0,0},{"SaveDataDialog",0,0,0},
                         {"NpProfileDialog",0,0,0},{"NpCommerceDialog",0,0,0},{"ImeDialog",0,0,0}};
#define DIALOG_RUNNING 2
#define DIALOG_FINISHED 3
#define DIALOG_NOT_FINISHED ((int32_t)0x80B80005)
#define DIALOG_PARAM_INVALID ((int32_t)0x80B8000A)
#define DIALOG_NOT_RUNNING ((int32_t)0x80B8000B)
#define DIALOG_ARG_NULL ((int32_t)0x80B8000D)
#define DIALOG_NOT_SUPPORTED ((int32_t)0x80B8000F)
static int common_initialized;
static ABI int32_t common_init(void) { common_initialized=1; return 0; }
static int32_t dialog_init(int i) {
    if (dialogs[i].initialized) return (int32_t)0x80B80004;
    dialogs[i].initialized=1; dialogs[i].status=1; return 0;
}
static int32_t dialog_open(int i) {
    if (!dialogs[i].initialized) return DIALOG_NOT_INITIALIZED;
    printf("Runtime: %s opened; completed immediately (no dialog UI yet)\n",dialogs[i].name);
    dialogs[i].status=DIALOG_FINISHED; dialogs[i].button=1; return 0; /* OK / YES */
}
/* A progress bar shows the game's own work: it runs until the game closes it. */
static int32_t dialog_open_progress(int i) {
    if (!dialogs[i].initialized) return DIALOG_NOT_INITIALIZED;
    printf("Runtime: %s progress bar opened (runs until closed)\n",dialogs[i].name);
    dialogs[i].status=DIALOG_RUNNING; dialogs[i].button=0; return 0;
}
static int32_t dialog_close(int i) {
    if (dialogs[i].status!=DIALOG_RUNNING) return DIALOG_NOT_RUNNING;
    dialogs[i].status=DIALOG_FINISHED; return 0;
}
static int32_t dialog_progress(int i, uint32_t target) {
    if (dialogs[i].status!=DIALOG_RUNNING) return DIALOG_NOT_RUNNING;
    return target ? DIALOG_PARAM_INVALID : 0;
}
/* Result: result (OK), button, reserved; SaveDataDialog's begins with the mode. */
static int32_t dialog_result(int i, unsigned char *out, size_t offset) {
    if (dialogs[i].status!=DIALOG_FINISHED) return DIALOG_NOT_FINISHED;
    if (!out) return DIALOG_ARG_NULL;
    uint32_t values[2]={0,dialogs[i].button};
    memcpy(out+offset,values,8);
    return 0;
}
static int32_t dialog_status(int i) { return dialogs[i].status; }
static int32_t dialog_term(int i) {
    if (!dialogs[i].initialized) return DIALOG_NOT_INITIALIZED;
    dialogs[i].initialized=0; dialogs[i].status=0; return 0;
}
#define DIALOG(tag,i) \
    static ABI int32_t tag##_init(void) { return dialog_init(i); } \
    static ABI int32_t tag##_open(const void *p) { (void)p; return dialog_open(i); } \
    static ABI int32_t tag##_status(void) { return dialog_status(i); } \
    static ABI int32_t tag##_term(void) { return dialog_term(i); }
DIALOG(profile,3) DIALOG(commerce,4)
/* OrbisMsgDialogParam: BaseParam (48 bytes), size, mode (2 = progress bar).
 * OrbisSaveDataDialogParam: BaseParam, int32 size, mode (5 = progress bar). */
static int32_t dialog_open_mode(int i, const unsigned char *param, size_t mode_offset, uint32_t progress_mode) {
    if (!param) return DIALOG_ARG_NULL;
    uint32_t mode; memcpy(&mode,param+mode_offset,4);
    return mode==progress_mode ? dialog_open_progress(i) : dialog_open(i);
}
static ABI int32_t msg_init(void) { return dialog_init(1); }
static ABI int32_t msg_open(const unsigned char *p) { return dialog_open_mode(1,p,56,2); }
static ABI int32_t msg_status(void) { return dialog_status(1); }
static ABI int32_t msg_term(void) { return dialog_term(1); }
static ABI int32_t msg_close(void) { return dialog_close(1); }
static ABI int32_t msg_progress(uint32_t target, uint32_t value) { (void)value; return dialog_progress(1,target); }
static ABI int32_t msg_result(unsigned char *out) { return dialog_result(1,out,0); }
/* SaveDataDialog. OrbisSaveDataDialogParam: BaseParam (48), size, mode @52, dispType @56,
 * animParam @64, items @72, ..., userData @112. SaveDialogItems: userId @0, titleId @8,
 * dirName @16 (32-byte names), dirNameNum @24, newItem @32 (title @0), focusPos @40.
 * A list (mode 1) is shown for the player to choose; every other mode answers for the
 * first listed save. The result names the chosen save ("" = the new item). */
#define SAVE_MODE_LIST 1
#define SAVE_RESULT_CANCELED 1
static struct {
    uint32_t mode, result;
    void *user_data;
    char dirs[32][32];
    int dir_count, has_new, choosing;
} save_dialog;
static char save_labels[33][200], save_icons[33][720];
static void save_label(char *out, size_t size, const char *dir) {
    unsigned char param[RUNTIME_SAVE_PARAM_SIZE];
    if (runtime_savedata_param(USER_ID,dir,param)) { snprintf(out,size,"%s",dir); return; }
    int64_t mtime; memcpy(&mtime,param+1288,8);
    time_t t=(time_t)mtime; struct tm tm;
#ifdef _WIN32
    localtime_s(&tm,&t);
#else
    localtime_r(&t,&tm);
#endif
    char when[32]; strftime(when,sizeof(when),"%Y-%m-%d %H:%M",&tm);
    const char *title=(const char *)param, *subtitle=(const char *)param+128;
    /* Title, subtitle and date on their own lines, beside the save's picture. */
    snprintf(out,size,"%.60s\n%.60s%s%s",title[0] ? title : dir,subtitle,subtitle[0] ? "\n" : "",when);
}
static ABI int32_t save_init(void) { return dialog_init(2); }
static ABI int32_t save_open(const unsigned char *p) {
    if (!p) return DIALOG_ARG_NULL;
    if (!dialogs[2].initialized) return DIALOG_NOT_INITIALIZED;
    memset(&save_dialog,0,sizeof(save_dialog));
    memcpy(&save_dialog.mode,p+52,4);
    memcpy(&save_dialog.user_data,p+112,8);
    const unsigned char *items; memcpy(&items,p+72,8);
    if (items) {
        const char *dirs; uint32_t count; const void *new_item;
        memcpy(&dirs,items+16,8); memcpy(&count,items+24,4); memcpy(&new_item,items+32,8);
        for (uint32_t i=0;dirs && i<count && save_dialog.dir_count<32;++i)
            if (dirs[i*32]) memcpy(save_dialog.dirs[save_dialog.dir_count++],dirs+i*32,31);
        save_dialog.has_new=new_item!=NULL;
    }
    if (save_dialog.mode!=SAVE_MODE_LIST) return dialog_open_mode(2,p,52,5);
    const char *labels[33], *icons[33]; int n=0;
    if (save_dialog.has_new) { snprintf(save_labels[n],sizeof(save_labels[n]),"New save"); labels[n]=save_labels[n]; icons[n]=NULL; ++n; }
    for (int i=0;i<save_dialog.dir_count;++i) {
        save_label(save_labels[n],sizeof(save_labels[n]),save_dialog.dirs[i]); labels[n]=save_labels[n];
        icons[n]=runtime_savedata_icon(USER_ID,save_dialog.dirs[i],save_icons[n],sizeof(save_icons[n])) ? NULL : save_icons[n];
        ++n;
    }
    uint32_t type; memcpy(&type,p+56,4);
    const char *title=type==1 ? "Save game" : type==2 ? "Load game" : "Delete save";
    if (!n || !vwgpu_choice_begin(title,labels,icons,n,0)) {
        printf("Runtime: SaveDataDialog list (%s): %s\n",title,n ? "no window, cancelled" : "no saves, cancelled");
        save_dialog.result=SAVE_RESULT_CANCELED;
        dialogs[2].status=DIALOG_FINISHED; dialogs[2].button=0;
        return 0;
    }
    printf("Runtime: SaveDataDialog list (%s): %d entries shown\n",title,n);
    save_dialog.choosing=1;
    dialogs[2].status=DIALOG_RUNNING;
    return 0;
}
static ABI int32_t save_status(void) {
    if (save_dialog.choosing) {
        int choice=vwgpu_choice_poll();
        if (choice!=-1) {
            save_dialog.choosing=0;
            dialogs[2].status=DIALOG_FINISHED;
            if (choice<0) { save_dialog.result=SAVE_RESULT_CANCELED; dialogs[2].button=0; }
            else {
                int dir=choice-save_dialog.has_new;
                /* The chosen save goes first; the new item has no name. */
                if (dir>=0) memcpy(save_dialog.dirs[0],save_dialog.dirs[dir],32);
                else save_dialog.dirs[0][0]=0;
                save_dialog.dir_count=dir>=0 || save_dialog.dir_count ? 1 : 0;
                dialogs[2].button=1;
            }
            printf("Runtime: SaveDataDialog list: %s%s\n",choice<0 ? "cancelled" : "chose ",
                   choice<0 ? "" : save_dialog.dirs[0][0] ? save_dialog.dirs[0] : "new save");
        }
    }
    return dialog_status(2);
}
static ABI int32_t save_term(void) { return dialog_term(2); }
static ABI int32_t save_close(void) { return dialog_close(2); }
static ABI int32_t save_progress(uint32_t target, uint32_t rate) { (void)rate; return dialog_progress(2,target); }
/* OrbisSaveDataDialogResult: mode, result, buttonId, dirName* @16, param* @24, userData @32. */
static ABI int32_t save_result(unsigned char *out) {
    int32_t r=dialog_result(2,out,4);
    if (r) return r;
    memcpy(out,&save_dialog.mode,4);
    memcpy(out+4,&save_dialog.result,4);
    char *dir; unsigned char *param;
    memcpy(&dir,out+16,8); memcpy(&param,out+24,8);
    const char *name=save_dialog.dir_count && !save_dialog.result ? save_dialog.dirs[0] : "";
    if (dir) { memset(dir,0,32); memcpy(dir,name,strnlen(name,31)); }
    if (param && name[0]) runtime_savedata_param(USER_ID,name,param);
    memcpy(out+32,&save_dialog.user_data,8);
    return 0;
}
static ABI int32_t save_ready(void) { return 1; }
static ABI int32_t profile_result(void *result) { if (result) memset(result,0,4); return 0; }
/* ImeDialog: text typed on the keyboard into the game window (title bar shows it).
 * OrbisImeDialogParam: user, type, languages(8), enter label, method, filter,
 * option, max length, char16 buffer, position, alignment, placeholder, title. */
typedef struct {
    int32_t user; uint32_t type; uint64_t languages; uint32_t enter_label, input_method;
    void *filter; uint32_t option, max_length; uint16_t *buffer;
    float x, y; uint32_t halign, valign; const uint16_t *placeholder, *title; int8_t reserved[16];
} ImeParam;
static struct { int running, finished, end_status; uint16_t *buffer; uint32_t max_length; } ime;
/* The system keyboard panel's size (as shadPS4 measured it): 793x528, numbers 370x522,
 * 2x with the over-2K coordinates option; the extended parameters do not change it. */
static ABI int32_t ime_panel_size(const ImeParam *param, const void *extended, uint32_t *width, uint32_t *height) {
    (void)extended;
    if (!param || !width || !height) return (int32_t)0x80BC0031; /* INVALID_ADDRESS */
    if (param->type>3) return (int32_t)0x80BC0011; /* INVALID_TYPE */
    if (param->type==3) { *width=0x172; *height=0x20a; }
    else { *width=0x319; *height=(param->option & 1) ? 0x274 : 0x210; } /* option 1: multiline */
    if (param->option & 0x4000) { *width<<=1; *height<<=1; }
    return 0;
}
static size_t utf16_to_utf8(const uint16_t *in, size_t limit, char *out, size_t size) {
    size_t n=0;
    for (size_t i=0; in && i<limit && in[i] && n+4<size; ++i) {
        uint32_t c=in[i];
        if (c>=0xD800 && c<0xDC00 && i+1<limit && in[i+1]>=0xDC00 && in[i+1]<0xE000) { c=0x10000+((c-0xD800)<<10)+(in[i+1]-0xDC00); ++i; }
        if (c<0x80) out[n++]=(char)c;
        else if (c<0x800) { out[n++]=(char)(0xC0|c>>6); out[n++]=(char)(0x80|(c&63)); }
        else if (c<0x10000) { out[n++]=(char)(0xE0|c>>12); out[n++]=(char)(0x80|((c>>6)&63)); out[n++]=(char)(0x80|(c&63)); }
        else { out[n++]=(char)(0xF0|c>>18); out[n++]=(char)(0x80|((c>>12)&63)); out[n++]=(char)(0x80|((c>>6)&63)); out[n++]=(char)(0x80|(c&63)); }
    }
    out[n]=0; return n;
}
static void utf8_to_utf16(const char *in, uint16_t *out, uint32_t max) {
    uint32_t n=0;
    for (const unsigned char *p=(const unsigned char *)in; *p && n<max;) {
        uint32_t c; int extra;
        if (*p<0x80) { c=*p; extra=0; } else if ((*p&0xE0)==0xC0) { c=*p&31; extra=1; }
        else if ((*p&0xF0)==0xE0) { c=*p&15; extra=2; } else { c=*p&7; extra=3; }
        ++p;
        for (int k=0;k<extra && (*p&0xC0)==0x80;++k) c=(c<<6)|(*p++&63);
        if (c>=0x10000) { if (n+2>max) break; c-=0x10000; out[n++]=(uint16_t)(0xD800+(c>>10)); out[n++]=(uint16_t)(0xDC00+(c&1023)); }
        else out[n++]=(uint16_t)c;
    }
    out[n<max ? n : max]=0;
}
static void ime_complete(int end_status, const char *text) {
    if (!end_status && ime.buffer && ime.max_length) utf8_to_utf16(text,ime.buffer,ime.max_length);
    ime.end_status=end_status; ime.finished=1; ime.running=0;
    printf("Runtime: ImeDialog %s%s%s\n",end_status ? "cancelled" : "text: ",end_status ? "" : text,"");
}
static ABI int32_t ime_init(const ImeParam *param, const void *extended) {
    (void)extended;
    if (!param || !param->buffer || !param->max_length) return (int32_t)0x80BC0004; /* IME INVALID_ADDRESS */
    if (ime.running) return (int32_t)0x80BC0003;                                  /* BUSY */
    memset(&ime,0,sizeof(ime));
    ime.buffer=param->buffer; ime.max_length=param->max_length; ime.running=1;
    char initial[512], prompt[256];
    utf16_to_utf8(param->buffer,param->max_length,initial,sizeof(initial));
    utf16_to_utf8(param->title,128,prompt,sizeof(prompt));
    const char *preset=getenv("VW_IME_TEXT");
    if (preset) { ime_complete(0,preset); return 0; }
    /* An empty field starts with the player name (launcher) or "Hunter": a controller alone
     * can accept it (the port has no on-screen keyboard). */
    const char *name=getenv("VW_USER_NAME");
    const char *start=initial[0] ? initial : name && *name ? name : "Hunter";
    if (!vwgpu_text_input_begin(start,prompt[0] ? prompt : "Text")) ime_complete(0,start);
    else printf("Runtime: ImeDialog opened: shown on screen; keyboard Enter or the confirm button accepts, Esc or the other one cancels\n");
    return 0;
}
static ABI int32_t ime_status(void) {
    if (ime.running) {
        char text[512];
        int state=vwgpu_text_input_poll(text,sizeof(text));
        if (state) ime_complete(state==1 ? 0 : 1,text);
    }
    return ime.running ? 1 : ime.finished ? 2 : 0; /* Running / Finished / None */
}
static ABI int32_t ime_result(uint32_t *result) {
    if (!ime.finished) return (int32_t)0x80BC0101; /* DIALOG NOT_FINISHED */
    if (result) *result=(uint32_t)ime.end_status;
    return 0;
}
static ABI int32_t ime_term(void) { memset(&ime,0,sizeof(ime)); return 0; }

static ABI int32_t common_dialog_is_used(void) { return 0; }
/* Network/PSN errors only: the dialog closes at once as if dismissed. */
typedef struct { uint64_t size; int32_t error_code, user; } ErrorDialogParam;
static int32_t error_dialog_status;
static ABI int32_t error_dialog_init(void) { error_dialog_status=1; return 0; }
static ABI int32_t error_dialog_open(const ErrorDialogParam *param) {
    if (param) printf("Runtime: error dialog for %#x skipped\n",(unsigned)param->error_code);
    error_dialog_status=3; return 0;
}
static ABI int32_t error_dialog_update(void) { return error_dialog_status; }
/* Sign-in dialog: offline, it finishes at once (the player stays signed out). */
static int32_t signin_status=0;
static ABI int32_t signin_init(void) { signin_status=1; return 0; }
static ABI int32_t signin_open(const void *param) { (void)param; signin_status=3; return 0; }
static ABI int32_t signin_update(void) { return signin_status; }
static ABI int32_t signin_term(void) { signin_status=0; return 0; }
/* OrbisSigninDialogResult: result (1 = USER_CANCELED, the account stays signed out), reserved[3]. */
static ABI int32_t signin_result(int32_t *result) {
    if (!signin_status) return (int32_t)0x81350001; /* NOT_INITIALIZED */
    if (!result) return (int32_t)0x81350003; /* PARAM_INVALID */
    memset(result,0,16); result[0]=1; return 0;
}
static ABI int32_t error_dialog_term(void) { error_dialog_status=0; return 0; }

static const RuntimeExport dialogs_exports[]={
    {"sceCommonDialogInitialize",common_init}, {"sceMsgDialogInitialize",msg_init},
    {"sceMsgDialogOpen",msg_open}, {"sceMsgDialogUpdateStatus",msg_status},
    {"sceMsgDialogTerminate",msg_term}, {"sceMsgDialogClose",msg_close},
    {"sceMsgDialogProgressBarSetValue",msg_progress}, {"sceMsgDialogGetResult",msg_result},
    {"sceMsgDialogGetStatus",msg_status}, {"sceSaveDataDialogInitialize",save_init},
    {"sceSaveDataDialogOpen",save_open}, {"sceSaveDataDialogUpdateStatus",save_status},
    {"sceSaveDataDialogTerminate",save_term}, {"sceSaveDataDialogGetStatus",save_status},
    {"sceSaveDataDialogClose",save_close}, {"sceSaveDataDialogProgressBarSetValue",save_progress},
    {"sceSaveDataDialogGetResult",save_result}, {"sceSaveDataDialogIsReadyToDisplay",save_ready},
    {"sceNpProfileDialogInitialize",profile_init}, {"sceNpProfileDialogOpen",profile_open},
    {"sceNpProfileDialogUpdateStatus",profile_status}, {"sceNpProfileDialogTerminate",profile_term},
    {"sceNpProfileDialogGetResult",profile_result}, {"sceNpCommerceDialogInitialize",commerce_init},
    {"sceNpCommerceDialogOpen",commerce_open}, {"sceNpCommerceDialogUpdateStatus",commerce_status},
    {"sceNpCommerceDialogTerminate",commerce_term}, {"sceImeDialogInit",ime_init},
    {"sceImeDialogGetStatus",ime_status}, {"sceImeDialogGetResult",ime_result}, {"sceImeDialogTerm",ime_term},
    {"sceImeDialogAbort",ime_term}, {"sceImeDialogGetPanelSizeExtended",ime_panel_size},
    {"sceCommonDialogIsUsed",common_dialog_is_used}, {"sceErrorDialogInitialize",error_dialog_init},
    {"sceErrorDialogOpen",error_dialog_open}, {"sceErrorDialogUpdateStatus",error_dialog_update},
    {"sceErrorDialogTerminate",error_dialog_term}, {"sceErrorDialogGetStatus",error_dialog_update},
    {"sceSigninDialogInitialize",signin_init}, {"sceSigninDialogOpen",signin_open},
    {"sceSigninDialogUpdateStatus",signin_update}, {"sceSigninDialogGetResult",signin_result},
    {"sceSigninDialogTerminate",signin_term}, {"sceInvitationDialogInitialize",ok_void},
    {"sceInvitationDialogTerminate",ok_void}, {"sceInvitationDialogGetResultA",np_signed_out},
    {"sceInvitationDialogOpenA",np_signed_out}, {"sceInvitationDialogUpdateStatus",np_signed_out},
    {"sceNpCommerceDialogGetResult",np_signed_out}, {"sceNpProfileDialogOpenA",np_signed_out},
};
uintptr_t runtime_dialogs_resolve(const char *name) { return RUNTIME_LOOKUP(dialogs_exports,name); }
