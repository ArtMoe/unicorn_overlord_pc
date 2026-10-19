/* Network and PSN for a console with the cable unplugged and no PSN account: NetCtl
 * disconnected, sockets unreachable, HTTP requests fail, NP calls return signed out (requests
 * and contexts still get ids, so set-up succeeds), voice chat carries no audio, trophies are
 * recorded in the log only. */
#define _GNU_SOURCE
#include "runtime_offline.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#define NET_CTL_NOT_CONNECTED ((int32_t)0x80412108)
#define NET_CTL_INVALID_ADDR ((int32_t)0x80412107)
#define NP_INVALID_ARGUMENT ((int32_t)0x80550003)
#define NET_ENETUNREACH ((int32_t)0x80410133)
#define NET_EINVAL ((int32_t)0x80410116)
#define HTTP_NETWORK ((int32_t)0x80431063)
#define TROPHY_INVALID ((int32_t)0x80551604)

/* ---- NetCtl / Net / Http / Ssl: no network ---- */
static int32_t net_errno;
static ABI int32_t net_init(void) { return 0; }
static ABI int32_t net_term(void) { return 0; }
static ABI int32_t *net_errno_loc(void) { return &net_errno; }
static ABI int32_t net_pool_create(const char *name,int size,int flags) { (void)name; (void)size; (void)flags; return new_id(); }
static ABI int32_t net_pool_destroy(int id) { (void)id; return 0; }
static ABI int32_t net_unreachable(void) { net_errno=51; return NET_ENETUNREACH; }
static ABI int32_t net_epoll_create(const char *name,int flags) { (void)name; (void)flags; return new_id(); }
static ABI int32_t net_epoll_destroy(int id) { (void)id; return 0; }
static ABI int32_t net_resolver_create(const char *name,int pool,int flags) { (void)name; (void)pool; (void)flags; return new_id(); }
static ABI int32_t net_resolver_destroy(int id) { (void)id; return 0; }
static ABI uint16_t net_htons(uint16_t v) { return htons(v); }
static ABI uint16_t net_ntohs(uint16_t v) { return ntohs(v); }
static ABI uint32_t net_htonl(uint32_t v) { return htonl(v); }
static ABI uint32_t net_ntohl(uint32_t v) { return ntohl(v); }
static ABI int32_t net_pton(int af,const char *src,void *dst) {
    if (af!=2) { net_errno=47; return NET_EINVAL; }
    return inet_pton(AF_INET,src,dst);
}
static ABI const char *net_ntop(int af,const void *src,char *dst,uint32_t size) {
    if (af!=2) { net_errno=47; return NULL; }
    return inet_ntop(AF_INET,src,dst,size);
}
static ABI int32_t netctl_state(int32_t *state) { if (!state) return NET_CTL_INVALID_ADDR; *state=0; return 0; }
static ABI int32_t netctl_info(int code,void *info) { (void)code; (void)info; return NET_CTL_NOT_CONNECTED; }
static ABI int32_t netctl_register(void *cb,void *arg,int32_t *cid) { (void)cb; (void)arg; if (!cid) return NET_CTL_INVALID_ADDR; *cid=new_id(); return 0; }
static ABI int32_t netctl_check(void) { return 0; }
static ABI int32_t netctl_unregister(int cid) { (void)cid; return 0; }
static ABI int32_t netctl_nat(uint32_t *info) {
    if (!info) return NET_CTL_INVALID_ADDR;
    info[1]=0; info[2]=3; info[3]=0; /* stun failed, NAT type 3, no mapped address */
    return 0;
}
static ABI int32_t http_fail(void) { return HTTP_NETWORK; }
/* Objects are created so setup code proceeds; any transfer fails as unplugged. */
static ABI int32_t http_object(void) { return new_id(); }
static ABI int32_t http_epoll(int32_t ctx,void **handle) {
    (void)ctx;
    if (!handle) return (int32_t)0x80431077; /* HTTP INVALID_VALUE */
    *handle=(void *)(uintptr_t)(0x100+new_id()); return 0;
}
static ABI int32_t http_wait(void *handle,void *events,int32_t max,int64_t timeout) {
    (void)handle; (void)events; (void)max;
#ifdef _WIN32
    if (timeout>0) compat_sleep_ns((uint64_t)timeout*1000);
#else
    if (timeout>0) { struct timespec t={timeout/1000000,(timeout%1000000)*1000}; nanosleep(&t,NULL); }
#endif
    return 0; /* no events: nothing is in flight */
}

/* ---- NP (PSN): signed out ---- */
static ABI int32_t np_state(int32_t user,int32_t *state) {
    if (!state) return NP_INVALID_ARGUMENT;
    (void)user; *state=1; /* SIGNED_OUT */
    return 0;
}
static ABI int32_t np_register(void *cb,void *arg) { (void)cb; (void)arg; return new_id(); }
static ABI void np_register_void(void *cb,void *arg) { (void)cb; (void)arg; }
static ABI int32_t np_request(const void *param) { (void)param; return new_id(); }
static ABI int32_t np_request_ctx(int32_t ctx,const void *param) { (void)ctx; (void)param; return new_id(); }
static ABI int32_t np_poll(int32_t request,int32_t *result) { (void)request; if (result) *result=NP_SIGNED_OUT; return 0; }
static ABI int32_t np_compare(const void *a,const void *b) {
    if (!a || !b) return NP_INVALID_ARGUMENT;
    return memcmp(a,b,16) ? (int32_t)0x80550609 : 0; /* NP_UTIL NOT_MATCH */
}

/* ---- Voice chat: ports exist, carry no audio ---- */
static ABI int32_t voice_port(void *param,uint32_t *port) { (void)param; if (!port) return (int32_t)0x8029000b; *port=(uint32_t)new_id(); return 0; }
static ABI int32_t voice_read(uint32_t port,void *data,uint32_t *size) { (void)port; (void)data; if (size) *size=0; return 0; }
static ABI int32_t voice_write(uint32_t port,const void *data,uint32_t *size) { (void)port; (void)data; (void)size; return 0; }
static ABI int32_t voice_info(uint32_t port,uint32_t *info) {
    (void)port;
    if (!info) return (int32_t)0x8029000b;
    memset(info,0,40); /* type, state=unconnected, no bytes available */
    return 0;
}


/* ---- Trophies: accepted locally, recorded in the log ---- */
static ABI int32_t trophy_context(int32_t *ctx,int32_t user,uint32_t label,uint64_t options) {
    (void)user; (void)label; (void)options;
    if (!ctx) return TROPHY_INVALID;
    *ctx=new_id(); return 0;
}
static ABI int32_t trophy_handle(int32_t *handle) { if (!handle) return TROPHY_INVALID; *handle=new_id(); return 0; }
static ABI int32_t trophy_register(int32_t ctx,int32_t handle,uint64_t options) { (void)ctx; (void)handle; (void)options; return 0; }
static ABI int32_t trophy_unlock(int32_t ctx,int32_t handle,int32_t id,int32_t *platinum) {
    (void)ctx; (void)handle;
    printf("Runtime: trophy %d unlocked\n",id);
    if (platinum) *platinum=-1;
    return 0;
}
static ABI int32_t trophy_game_info(int32_t ctx,int32_t handle,void *details,void *data) {
    (void)ctx; (void)handle;
    if (details) { uint64_t size; memcpy(&size,details,8); memset((char *)details+8,0,size>8 && size<4096 ? size-8 : 0); }
    if (data) { uint64_t size; memcpy(&size,data,8); memset((char *)data+8,0,size>8 && size<4096 ? size-8 : 0); }
    return 0;
}
static ABI int32_t trophy_info(int32_t ctx,int32_t handle,int32_t id,void *details,void *data) {
    (void)id; return trophy_game_info(ctx,handle,details,data);
}
/* Nothing is unlocked on this console: an empty 128-trophy flag set. */
static ABI int32_t trophy_unlock_state(int32_t ctx,int32_t handle,uint32_t *flags,uint32_t *count) {
    if (!flags || !count) return TROPHY_INVALID;
    if (ctx<=0 || handle<=0) return TROPHY_INVALID;
    memset(flags,0,16); *count=0;
    return 0;
}
static ABI int32_t trophy_release(int32_t id) { return id>0 ? 0 : TROPHY_INVALID; }


/* ---- Http URI escaping: RFC 3986 unreserved characters stay, the rest becomes %XX ---- */
static ABI int32_t http_uri_escape(char *out,uint64_t *required,uint64_t prepared,const char *in) {
    if (!in) return (int32_t)0x804311FE; /* INVALID_VALUE */
    static const char hex[]="0123456789ABCDEF";
    uint64_t need=1;
    for (const unsigned char *p=(const unsigned char *)in;*p;++p)
        need+=(isalnum(*p) || *p=='-' || *p=='_' || *p=='.' || *p=='~') ? 1 : 3;
    if (required) *required=need;
    if (!out) return 0;
    if (prepared<need) return (int32_t)0x80431022; /* OUT_OF_MEMORY */
    for (const unsigned char *p=(const unsigned char *)in;*p;++p) {
        if (isalnum(*p) || *p=='-' || *p=='_' || *p=='.' || *p=='~') *out++=(char)*p;
        else { *out++='%'; *out++=hex[*p>>4]; *out++=hex[*p&15]; }
    }
    *out=0; return 0;
}

/* ---- TUS online storage: signed out ---- */
static ABI int32_t tus_get_data(void) { return NP_SIGNED_OUT; }

static const RuntimeExport online_exports[]={
    {"sceNetInit",net_init}, {"sceNetTerm",net_term}, {"sceNetErrnoLoc",net_errno_loc},
    {"sceNetPoolCreate",net_pool_create}, {"sceNetPoolDestroy",net_pool_destroy},
    {"sceNetEpollCreate",net_epoll_create}, {"sceNetEpollDestroy",net_epoll_destroy},
    {"sceNetResolverCreate",net_resolver_create}, {"sceNetResolverDestroy",net_resolver_destroy},
    {"sceNetHtons",net_htons}, {"sceNetNtohs",net_ntohs}, {"sceNetHtonl",net_htonl},
    {"sceNetNtohl",net_ntohl}, {"sceNetInetPton",net_pton}, {"sceNetInetNtop",net_ntop},
    {"sceNetSocket",net_unreachable}, {"sceNetConnect",net_unreachable}, {"sceNetBind",net_unreachable},
    {"sceNetListen",net_unreachable}, {"sceNetAccept",net_unreachable}, {"sceNetSend",net_unreachable},
    {"sceNetSendto",net_unreachable}, {"sceNetRecv",net_unreachable}, {"sceNetRecvfrom",net_unreachable},
    {"sceNetSetsockopt",net_unreachable}, {"sceNetGetsockopt",net_unreachable},
    {"sceNetGetsockname",net_unreachable}, {"sceNetShutdown",net_unreachable},
    {"sceNetSocketClose",net_unreachable}, {"sceNetSocketAbort",net_unreachable},
    {"sceNetEpollControl",net_unreachable}, {"sceNetEpollWait",net_unreachable},
    {"sceNetEpollAbort",net_unreachable}, {"sceNetResolverStartNtoa",net_unreachable},
    {"sceNetResolverStartAton",net_unreachable}, {"sceNetCtlGetState",netctl_state},
    {"sceNetCtlGetInfo",netctl_info}, {"sceNetCtlRegisterCallback",netctl_register},
    {"sceNetCtlCheckCallback",netctl_check}, {"sceNetCtlUnregisterCallback",netctl_unregister},
    {"sceNetCtlGetNatInfo",netctl_nat}, {"sceSslInit",lib_init_id}, {"sceSslTerm",ok_void},
    {"sceHttpInit",lib_init_id}, {"sceHttpTerm",ok_void}, {"sceHttpCreateTemplate",http_object},
    {"sceHttpDeleteTemplate",ok_void}, {"sceHttpCreateConnectionWithURL",http_object},
    {"sceHttpCreateRequestWithURL",http_object}, {"sceHttpSendRequest",http_fail},
    {"sceHttpCreateEpoll",http_epoll}, {"sceHttpSetNonblock",ok_void}, {"sceHttpSetConnectTimeOut",ok_void},
    {"sceHttpsEnableOption",ok_void}, {"sceHttpsDisableOption",ok_void}, {"sceHttpAddRequestHeader",ok_void},
    {"sceHttpSetRequestContentLength",ok_void}, {"sceHttpDeleteConnection",ok_void},
    {"sceHttpDeleteRequest",ok_void}, {"sceHttpAbortWaitRequest",ok_void}, {"sceHttpDestroyEpoll",ok_void},
    {"sceHttpSetEpoll",ok_void}, {"sceHttpUnsetEpoll",ok_void}, {"sceHttpWaitRequest",http_wait},
    {"sceHttpGetStatusCode",http_fail}, {"sceHttpGetResponseContentLength",http_fail},
    {"sceHttpReadData",http_fail}, {"sceNpGetState",np_state}, {"sceNpGetOnlineId",np_signed_out},
    {"sceNpGetNpId",np_signed_out}, {"sceNpRegisterStateCallback",np_register},
    {"sceNpUnregisterStateCallback",ok_void}, {"sceNpRegisterGamePresenceCallback",np_register_void},
    {"sceNpRegisterPlusEventCallback",np_register}, {"sceNpUnregisterPlusEventCallback",ok_void},
    {"sceNpCheckCallback",ok_void}, {"sceNpSetNpTitleId",ok_void}, {"sceNpNotifyPlusFeature",ok_void},
    {"sceNpSetContentRestriction",ok_void}, {"sceNpCreateAsyncRequest",np_request},
    {"sceNpDeleteRequest",ok_void}, {"sceNpAbortRequest",ok_void}, {"sceNpPollAsync",np_poll},
    {"sceNpCheckNpAvailability",np_signed_out}, {"sceNpGetParentalControlInfo",np_signed_out},
    {"sceNpCheckPlus",np_signed_out}, {"sceNpGetGamePresenceStatus",np_signed_out},
    {"sceNpCmpNpId",np_compare}, {"sceNpCmpOnlineId",np_compare}, {"sceNpAuthCreateAsyncRequest",np_request},
    {"sceNpAuthDeleteRequest",ok_void}, {"sceNpAuthPollAsync",np_poll},
    {"sceNpAuthGetAuthorizationCode",np_signed_out}, {"sceNpLookupCreateTitleCtx",np_request},
    {"sceNpLookupDeleteTitleCtx",ok_void}, {"sceNpLookupCreateAsyncRequest",np_request_ctx},
    {"sceNpLookupDeleteRequest",ok_void}, {"sceNpLookupAbortRequest",ok_void},
    {"sceNpLookupPollAsync",np_poll}, {"sceNpLookupNpId",np_signed_out},
    {"sceNpScoreCreateNpTitleCtx",np_request_ctx}, {"sceNpScoreDeleteNpTitleCtx",ok_void},
    {"sceNpScoreCreateRequest",np_request}, {"sceNpScoreDeleteRequest",ok_void},
    {"sceNpScoreAbortRequest",ok_void}, {"sceNpWebApiInitialize",lib_init_id},
    {"sceNpWebApiTerminate",ok_void}, {"sceNpWebApiCreateContext",np_signed_out},
    {"sceNpWebApiCreateRequest",np_signed_out}, {"sceNpWebApiSendRequest",np_signed_out},
    {"sceNpWebApiDeleteRequest",ok_void}, {"sceNpWebApiAbortRequest",ok_void},
    {"sceNpWebApiDeleteContext",ok_void}, {"sceNpWebApiReadData",np_signed_out},
    {"sceNpWebApiGetHttpStatusCode",np_signed_out}, {"sceNpWebApiGetHttpResponseHeaderValue",np_signed_out},
    {"sceNpWebApiGetHttpResponseHeaderValueLength",np_signed_out},
    {"sceNpWebApiCreatePushEventFilter",np_signed_out}, {"sceNpWebApiDeletePushEventFilter",ok_void},
    {"sceNpWebApiRegisterPushEventCallback",np_signed_out},
    {"sceNpWebApiUnregisterPushEventCallback",ok_void}, {"sceNpWebApiUtilityParseNpId",np_signed_out},
    {"sceNpMatching2ContextStart",np_signed_out}, {"sceNpMatching2ContextStop",ok_void},
    {"sceNpMatching2DestroyContext",ok_void}, {"sceNpMatching2RegisterContextCallback",ok_void},
    {"sceNpMatching2RegisterLobbyEventCallback",ok_void}, {"sceNpMatching2RegisterRoomEventCallback",ok_void},
    {"sceNpMatching2RegisterSignalingCallback",ok_void}, {"sceNpMatching2SetDefaultRequestOptParam",ok_void},
    {"sceNpMatching2CreateJoinRoom",np_signed_out}, {"sceNpMatching2JoinRoom",np_signed_out},
    {"sceNpMatching2LeaveRoom",np_signed_out}, {"sceNpMatching2SearchRoom",np_signed_out},
    {"sceNpMatching2GetServerId",np_signed_out}, {"sceNpMatching2GetWorldInfoList",np_signed_out},
    {"sceNpMatching2GetLobbyInfoList",np_signed_out}, {"sceNpMatching2JoinLobby",np_signed_out},
    {"sceNpMatching2LeaveLobby",np_signed_out}, {"sceNpMatching2GrantRoomOwner",np_signed_out},
    {"sceNpMatching2KickoutRoomMember",np_signed_out}, {"sceNpMatching2SetRoomDataExternal",np_signed_out},
    {"sceNpMatching2SetRoomDataInternal",np_signed_out},
    {"sceNpMatching2SetRoomMemberDataInternal",np_signed_out},
    {"sceNpMatching2SignalingGetConnectionStatus",np_signed_out},
    {"sceNpMatching2SignalingGetPingInfo",np_signed_out}, {"sceNpSignalingDeleteContext",ok_void},
    {"sceNpSignalingActivateConnection",np_signed_out}, {"sceNpSignalingDeactivateConnection",ok_void},
    {"sceNpSignalingGetConnectionStatus",np_signed_out}, {"sceNpScoreCensorComment",np_signed_out},
    {"sceNpScoreSanitizeComment",np_signed_out}, {"sceNpScoreGetBoardInfo",np_signed_out},
    {"sceNpScoreGetGameData",np_signed_out}, {"sceNpScoreGetRankingByNpIdPcId",np_signed_out},
    {"sceNpScoreGetRankingByRange",np_signed_out}, {"sceNpScoreRecordGameData",np_signed_out},
    {"sceNpScoreRecordScore",np_signed_out}, {"sceNpScoreSetPlayerCharacterId",ok_void},
    {"sceVoiceCreatePort",voice_port}, {"sceVoiceDeletePort",ok_void},
    {"sceVoiceConnectIPortToOPort",ok_void}, {"sceVoiceDisconnectIPortFromOPort",ok_void},
    {"sceVoiceStart",ok_void}, {"sceVoiceStop",ok_void}, {"sceVoiceGetPortInfo",voice_info},
    {"sceVoiceReadFromOPort",voice_read}, {"sceVoiceWriteToIPort",voice_write},
    {"sceNpMatching2Initialize",ok_void}, {"sceNpMatching2Terminate",ok_void},
    {"sceNpMatching2CreateContext",np_signed_out}, {"sceNpSignalingInitialize",ok_void},
    {"sceNpSignalingTerminate",ok_void}, {"sceNpSignalingCreateContext",np_signed_out},
    {"sceNpTrophyCreateContext",trophy_context}, {"sceNpTrophyCreateHandle",trophy_handle},
    {"sceNpTrophyRegisterContext",trophy_register}, {"sceNpTrophyUnlockTrophy",trophy_unlock},
    {"sceNpTrophyGetGameInfo",trophy_game_info}, {"sceNpTrophyGetTrophyInfo",trophy_info},
    {"sceNpTrophyGetTrophyUnlockState",trophy_unlock_state}, {"sceNpTrophyAbortHandle",trophy_release},
    {"sceNpTrophyDestroyHandle",trophy_release}, {"sceNpTrophyDestroyContext",trophy_release},
    {"sceHttpUriEscape",http_uri_escape}, {"sceVoiceInit",ok_void}, {"sceVoiceEnd",ok_void},
    {"sceNpTusCreateNpTitleCtx",np_request_ctx}, {"sceNpTusDeleteNpTitleCtx",ok_void},
    {"sceNpTusCreateRequest",np_request}, {"sceNpTusDeleteRequest",ok_void}, {"sceNpTusGetData",tus_get_data},
    {"sceNpTusSetData",tus_get_data}, {"sceShareUtilityAdvanceInitialize",ok_void},
    {"sceShareUtilityAdvanceTerminate",ok_void}, {"sceShareUtilityAdvanceSetUploadContentData",ok_void},
    {"sceNpCheckNpAvailabilityA",np_signed_out}, {"sceNpGetAccountIdA",np_signed_out},
    {"sceNpGetParentalControlInfoA",np_signed_out}, {"sceNpRegisterStateCallbackA",np_register},
    {"sceNpUnregisterStateCallbackA",ok_void}, {"sceNpRegisterNpReachabilityStateCallback",np_register},
    {"sceNpWebApi2Initialize",lib_init_id}, {"sceNpWebApi2CreateUserContext",np_signed_out},
    {"sceNpWebApi2DeleteUserContext",ok_void}, {"sceNpWebApi2PushEventCreateHandle",lib_init_id},
    {"sceNpWebApi2PushEventDeleteHandle",ok_void}, {"sceNpWebApi2PushEventCreateFilter",np_signed_out},
    {"sceNpWebApi2PushEventDeleteFilter",ok_void}, {"sceNpWebApi2PushEventRegisterCallback",np_signed_out},
    {"sceNpWebApi2PushEventUnregisterCallback",ok_void}, {"sceHttp2Init",lib_init_id},
    {"sceHttpSetResolveTimeOut",ok_void}, {"sceHttpSetResolveRetry",ok_void},
    {"sceHttpSetSendTimeOut",ok_void}, {"sceHttpSetRecvTimeOut",ok_void}, {"sceHttpSetCookieEnabled",ok_void},
    {"sceHttpSetAuthInfoCallback",ok_void}, {"sceHttpSetAuthEnabled",ok_void},
    {"sceHttpsSetSslCallback",ok_void}, {"sceHttpGetAllResponseHeaders",http_fail},
    {"sceHttpGetMemoryPoolStats",ok_void}, {"sceNetCtlCheckCallbackForNpToolkit",ok_void},
    {"sceNetCtlUnregisterCallbackForNpToolkit",ok_void}, {"sceNetGetMemoryPoolStats",ok_void},
    {"sceNpAuthAbortRequest",ok_void}, {"sceNpBandwidthTestAbort",ok_void},
    {"sceNpBandwidthTestShutdown",ok_void}, {"sceNpCheckCallbackForLib",ok_void},
    {"sceNpCommerceHidePsStoreIcon",ok_void}, {"sceNpCommerceShowPsStoreIcon",ok_void},
    {"sceNpInGameMessageDeleteHandle",ok_void}, {"sceNpInGameMessageInitialize",ok_void},
    {"sceNpInGameMessageTerminate",ok_void}, {"sceNpMatching2SetSignalingOptParam",ok_void},
    {"sceNpTrophyGetGameIcon",np_signed_out}, {"sceNpTrophyGetGroupIcon",ok_void},
    {"sceNpTrophyGetTrophyIcon",np_signed_out}, {"sceNpTusAbortRequest",ok_void},
    {"sceNpTusDeleteMultiSlotDataA",np_signed_out}, {"sceNpTusDeleteMultiSlotDataVUser",ok_void},
    {"sceNpUnregisterStateCallbackForToolkit",ok_void}, {"sceNpWebApi2AbortRequest",ok_void},
    {"sceNpWebApi2DeleteRequest",ok_void}, {"sceNpWebApiDeleteExtdPushEventFilter",ok_void},
    {"sceNpWebApiDeleteHandle",ok_void}, {"sceNpWebApiGetMemoryPoolStats",ok_void},
    {"sceNpWebApiUnregisterExtdPushEventCallback",ok_void}, {"sceNpWordFilterAbortRequest",ok_void},
    {"sceNpWordFilterDeleteRequest",ok_void}, {"sceNpWordFilterDeleteTitleCtx",ok_void},
    {"sceSslGetMemoryPoolStats",ok_void}, {"sceNetCtlRegisterCallbackForNpToolkit",lib_init_id},
    {"sceNpAuthCreateRequest",lib_init_id}, {"sceNpCreateRequest",lib_init_id},
    {"sceNpInGameMessageCreateHandle",lib_init_id}, {"sceNpMatching2RegisterRoomMessageCallback",lib_init_id},
    {"sceNpRegisterStateCallbackForToolkit",lib_init_id}, {"sceNpScoreCreateNpTitleCtxA",lib_init_id},
    {"sceNpTssCreateNpTitleCtxA",lib_init_id}, {"sceNpTusCreateNpTitleCtxA",lib_init_id},
    {"sceNpWebApi2CreateRequest",lib_init_id}, {"sceNpWebApiCreateExtdPushEventFilter",lib_init_id},
    {"sceNpWebApiCreateHandle",lib_init_id}, {"sceNpWordFilterCreateRequest",lib_init_id},
    {"sceNpWordFilterCreateTitleCtxA",lib_init_id}, {"sceNpAuthGetAuthorizationCodeA",np_signed_out},
    {"sceNpAuthGetIdTokenA",np_signed_out}, {"sceNpBandwidthTestGetStatus",np_signed_out},
    {"sceNpBandwidthTestInitStart",np_signed_out}, {"sceNpGetUserIdByAccountId",np_signed_out},
    {"sceNpInGameMessageGetMemoryPoolStatistics",np_signed_out}, {"sceNpInGameMessagePrepareA",np_signed_out},
    {"sceNpInGameMessageSendDataA",np_signed_out}, {"sceNpMatching2CreateContextA",np_signed_out},
    {"sceNpMatching2CreateJoinRoomA",np_signed_out}, {"sceNpMatching2GetMemoryInfo",np_signed_out},
    {"sceNpMatching2GetRoomDataExternalList",np_signed_out},
    {"sceNpMatching2GetRoomDataInternal",np_signed_out},
    {"sceNpMatching2GetRoomMemberDataInternal",np_signed_out},
    {"sceNpMatching2GetRoomMemberIdListLocal",np_signed_out},
    {"sceNpMatching2GetRoomPasswordLocal",np_signed_out},
    {"sceNpMatching2GetSignalingOptParamLocal",np_signed_out},
    {"sceNpMatching2GetSslMemoryInfo",np_signed_out}, {"sceNpMatching2GetUserInfoListA",np_signed_out},
    {"sceNpMatching2JoinRoomA",np_signed_out}, {"sceNpMatching2SendRoomChatMessage",np_signed_out},
    {"sceNpMatching2SendRoomMessage",np_signed_out}, {"sceNpMatching2SetUserInfo",np_signed_out},
    {"sceNpMatching2SignalingGetConnectionInfoA",np_signed_out},
    {"sceNpScoreGetFriendsRankingA",np_signed_out}, {"sceNpScoreGetFriendsRankingForCrossSave",np_signed_out},
    {"sceNpScoreGetGameDataByAccountId",np_signed_out}, {"sceNpScoreGetRankingByAccountId",np_signed_out},
    {"sceNpScoreGetRankingByAccountIdForCrossSave",np_signed_out},
    {"sceNpScoreGetRankingByAccountIdPcId",np_signed_out},
    {"sceNpScoreGetRankingByAccountIdPcIdForCrossSave",np_signed_out},
    {"sceNpScoreGetRankingByRangeA",np_signed_out}, {"sceNpScoreGetRankingByRangeForCrossSave",np_signed_out},
    {"sceNpTrophyCaptureScreenshot",np_signed_out}, {"sceNpTrophyGetGroupInfo",np_signed_out},
    {"sceNpTrophyShowTrophyList",np_signed_out}, {"sceNpTssGetData",np_signed_out},
    {"sceNpTusAddAndGetVariableA",np_signed_out}, {"sceNpTusAddAndGetVariableAVUser",np_signed_out},
    {"sceNpTusAddAndGetVariableForCrossSave",np_signed_out},
    {"sceNpTusAddAndGetVariableForCrossSaveVUser",np_signed_out}, {"sceNpTusGetDataA",np_signed_out},
    {"sceNpTusGetDataAVUser",np_signed_out}, {"sceNpTusGetDataForCrossSave",np_signed_out},
    {"sceNpTusGetDataForCrossSaveVUser",np_signed_out}, {"sceNpTusGetFriendsDataStatusA",np_signed_out},
    {"sceNpTusGetFriendsDataStatusForCrossSave",np_signed_out}, {"sceNpTusGetFriendsVariableA",np_signed_out},
    {"sceNpTusGetFriendsVariableForCrossSave",np_signed_out}, {"sceNpTusGetMultiSlotVariableA",np_signed_out},
    {"sceNpTusGetMultiSlotVariableAVUser",np_signed_out},
    {"sceNpTusGetMultiSlotVariableForCrossSave",np_signed_out},
    {"sceNpTusGetMultiSlotVariableForCrossSaveVUser",np_signed_out},
    {"sceNpTusGetMultiUserDataStatusA",np_signed_out}, {"sceNpTusGetMultiUserDataStatusAVUser",np_signed_out},
    {"sceNpTusGetMultiUserDataStatusForCrossSave",np_signed_out},
    {"sceNpTusGetMultiUserDataStatusForCrossSaveVUser",np_signed_out},
    {"sceNpTusGetMultiUserVariableA",np_signed_out}, {"sceNpTusGetMultiUserVariableAVUser",np_signed_out},
    {"sceNpTusGetMultiUserVariableForCrossSave",np_signed_out},
    {"sceNpTusGetMultiUserVariableForCrossSaveVUser",np_signed_out}, {"sceNpTusSetDataA",np_signed_out},
    {"sceNpTusSetDataAVUser",np_signed_out}, {"sceNpTusSetMultiSlotVariableA",np_signed_out},
    {"sceNpTusSetMultiSlotVariableVUser",np_signed_out}, {"sceNpTusTryAndSetVariableA",np_signed_out},
    {"sceNpTusTryAndSetVariableAVUser",np_signed_out},
    {"sceNpTusTryAndSetVariableForCrossSave",np_signed_out},
    {"sceNpTusTryAndSetVariableForCrossSaveVUser",np_signed_out},
    {"sceNpWebApi2AddHttpRequestHeader",np_signed_out},
    {"sceNpWebApi2GetHttpResponseHeaderValue",np_signed_out},
    {"sceNpWebApi2GetHttpResponseHeaderValueLength",np_signed_out}, {"sceNpWebApi2ReadData",np_signed_out},
    {"sceNpWebApi2SendRequest",np_signed_out}, {"sceNpWebApiAddHttpRequestHeader",np_signed_out},
    {"sceNpWebApiCreateContextA",np_signed_out}, {"sceNpWebApiRegisterExtdPushEventCallbackA",np_signed_out},
    {"sceNpWebApiSendRequest2",np_signed_out}, {"sceNpWordFilterSanitizeComment",np_signed_out},
    {"sceGameLiveStreamingInitialize",ok_void}, {"sceGameLiveStreamingTerminate",ok_void},
};
uintptr_t runtime_online_resolve(const char *name) { return RUNTIME_LOOKUP(online_exports,name); }
