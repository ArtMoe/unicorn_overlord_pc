/* libSceVideodec: H.264 access units decoded by FFmpeg into the game's NV12 frame buffers.
 * Structures, sizes and the resource estimate follow shadPS4's videodec (GPL-2.0-or-later);
 * pictures larger than the game's frame buffer are an error code instead of an overflow. */
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#define ERR_API_FAIL ((int32_t)0x80C10000)
#define ERR_STRUCT_SIZE ((int32_t)0x80C10002)
#define ERR_HANDLE ((int32_t)0x80C10003)
#define ERR_AU_SIZE ((int32_t)0x80C10009)
#define ERR_AU_POINTER ((int32_t)0x80C1000A)
#define ERR_FRAME_BUFFER_SIZE ((int32_t)0x80C1000B)
#define ERR_FRAME_BUFFER_POINTER ((int32_t)0x80C1000C)
#define ERR_ARGUMENT_POINTER ((int32_t)0x80C1000F)

typedef struct {
    uint64_t this_size;
    uint32_t codec_type, profile, max_level;
    int32_t max_frame_width, max_frame_height, max_dpb_frame_count;
    uint64_t videodec_flags;
} ConfigInfo;
typedef struct {
    uint64_t this_size, cpu_memory_size;
    void *cpu_memory;
    uint64_t cpu_gpu_memory_size;
    void *cpu_gpu_memory;
    uint64_t max_frame_buffer_size;
    uint32_t frame_buffer_alignment;
} ResourceInfo;
typedef struct { uint64_t this_size; void *handle; uint64_t version; } Ctrl;
typedef struct { uint64_t this_size; void *frame_buffer; uint64_t frame_buffer_size; } FrameBuffer;
typedef struct {
    uint32_t num_units_in_tick, time_scale;
    uint8_t fixed_frame_rate_flag, aspect_ratio_idc;
    uint16_t sar_width, sar_height;
    uint8_t colour_primaries, transfer_characteristics, matrix_coefficients, video_full_range_flag;
    uint32_t crop_left, crop_right, crop_top, crop_bottom;
} AvcInfo;
typedef struct {
    uint64_t this_size;
    uint32_t is_valid, codec_type, frame_width, frame_pitch, frame_height, is_error_pic;
    uint64_t pts_data, attached_data;
    union { uint8_t reserved[64]; AvcInfo avc; } codec;
} PictureInfo;
typedef struct { uint64_t this_size; void *au_data; uint64_t au_size, pts_data, dts_data, attached_data; } InputData;
_Static_assert(sizeof(ConfigInfo)==40 && sizeof(ResourceInfo)==56 && sizeof(Ctrl)==24 && sizeof(FrameBuffer)==24 &&
               sizeof(PictureInfo)==112 && sizeof(InputData)==48,"Videodec guest layouts");

typedef struct { AVCodecContext *codec; AVPacket *packet; AVFrame *frame, *nv12; struct SwsContext *sws; } Decoder;
static size_t pictures_decoded;

static uint32_t align(uint32_t value, uint32_t to) { return (value+to-1)&~(to-1); }

static ABI int32_t videodec_query(const ConfigInfo *config, ResourceInfo *out) {
    if (!config || !out) return ERR_ARGUMENT_POINTER;
    if (config->this_size!=sizeof(*config) || out->this_size!=sizeof(*out)) return ERR_STRUCT_SIZE;
    int32_t width=config->max_frame_width, height=config->max_frame_height;
    if (width<=0 || height<=0) { width=config->max_level>=150 ? 3840 : 1920; height=config->max_level>=150 ? 2160 : 1080; }
    uint64_t frame=(uint64_t)align((uint32_t)width,256)*align((uint32_t)height,16)*3/2;
    uint64_t padded=((frame+255)&~UINT64_C(255))+0x4000;
    uint64_t surfaces=(uint64_t)(config->max_dpb_frame_count>0 ? config->max_dpb_frame_count : 8)+2;
    out->cpu_memory=NULL; out->cpu_gpu_memory=NULL;
    out->cpu_memory_size=16u<<20;
    out->cpu_gpu_memory_size=padded*surfaces+(8u<<20);
    out->max_frame_buffer_size=padded;
    out->frame_buffer_alignment=0x100;
    return 0;
}
static ABI int32_t videodec_create(const ConfigInfo *config, const ResourceInfo *resource, Ctrl *out) {
    if (!config || !resource || !out) return ERR_ARGUMENT_POINTER;
    if (config->this_size!=sizeof(*config) || resource->this_size!=sizeof(*resource)) return ERR_STRUCT_SIZE;
    const AVCodec *codec=avcodec_find_decoder(AV_CODEC_ID_H264);
    Decoder *d=calloc(1,sizeof(*d));
    if (!codec || !d) { free(d); return ERR_API_FAIL; }
    d->codec=avcodec_alloc_context3(codec);
    d->packet=av_packet_alloc();
    d->frame=av_frame_alloc();
    d->nv12=av_frame_alloc();
    if (d->codec) d->codec->flags|=AV_CODEC_FLAG_COPY_OPAQUE; /* attached data follows its picture */
    if (!d->codec || !d->packet || !d->frame || !d->nv12 || avcodec_open2(d->codec,codec,NULL)<0) {
        avcodec_free_context(&d->codec); av_packet_free(&d->packet); av_frame_free(&d->frame); av_frame_free(&d->nv12);
        free(d); return ERR_API_FAIL;
    }
    out->this_size=sizeof(*out); out->handle=d; out->version=1;
    return 0;
}

/* The decoded picture into the game's buffer: luma, then interleaved chroma, both with a pitch
 * of the width aligned to 64 and the height aligned to 16 (the last rows repeated). */
static int32_t copy_nv12(Decoder *d, unsigned char *luma, uint64_t size, uint32_t *width_out, uint32_t *pitch_out, uint32_t *rows_out) {
    AVFrame *f=d->frame;
    if (f->format!=AV_PIX_FMT_NV12) {
        av_frame_unref(d->nv12);
        d->nv12->format=AV_PIX_FMT_NV12; d->nv12->width=f->width; d->nv12->height=f->height;
        d->sws=sws_getCachedContext(d->sws,f->width,f->height,(enum AVPixelFormat)f->format,
                                    f->width,f->height,AV_PIX_FMT_NV12,SWS_FAST_BILINEAR,NULL,NULL,NULL);
        if (!d->sws || av_frame_get_buffer(d->nv12,0)<0 ||
            sws_scale(d->sws,(const uint8_t *const *)f->data,f->linesize,0,f->height,d->nv12->data,d->nv12->linesize)<0)
            return ERR_API_FAIL;
        f=d->nv12;
    }
    uint32_t width=(uint32_t)f->width, height=(uint32_t)f->height;
    uint32_t pitch=align(width,64), rows=align(height,16);
    if ((uint64_t)pitch*rows*3/2>size) return ERR_FRAME_BUFFER_SIZE;
    unsigned char *chroma=luma+(size_t)pitch*rows;
    for (uint32_t y=0;y<rows;++y) {
        uint32_t from=y<height ? y : height-1;
        memcpy(luma+(size_t)y*pitch,f->data[0]+(size_t)from*(size_t)f->linesize[0],width);
        if (y<rows/2) {
            uint32_t c=y<height/2 ? y : height/2-1;
            memcpy(chroma+(size_t)y*pitch,f->data[1]+(size_t)c*(size_t)f->linesize[1],width);
        }
    }
    *width_out=width; *pitch_out=pitch; *rows_out=rows;
    ++pictures_decoded;
    return 0;
}
static int32_t picture(Decoder *d, FrameBuffer *buffer, PictureInfo *info) {
    uint32_t width, pitch, rows;
    int32_t r=copy_nv12(d,buffer->frame_buffer,buffer->frame_buffer_size,&width,&pitch,&rows);
    if (r) return r;
    info->is_valid=1; info->codec_type=0;
    info->frame_width=align(width,16); info->frame_pitch=pitch; info->frame_height=rows;
    info->is_error_pic=0;
    info->pts_data=(uint64_t)d->frame->pts;
    info->attached_data=(uint64_t)(uintptr_t)d->frame->opaque;
    memset(&info->codec,0,sizeof(info->codec));
    info->codec.avc.colour_primaries=(uint8_t)d->frame->color_primaries;
    info->codec.avc.crop_right=pitch-width;
    info->codec.avc.crop_bottom=rows-(uint32_t)d->frame->height;
    return 0;
}
static int32_t receive(Decoder *d, FrameBuffer *buffer, PictureInfo *info) {
    av_frame_unref(d->frame);
    int r=avcodec_receive_frame(d->codec,d->frame);
    if (r==AVERROR(EAGAIN) || r==AVERROR_EOF) return 0;
    return r<0 ? ERR_API_FAIL : picture(d,buffer,info);
}
static int32_t check(Ctrl *ctrl, FrameBuffer *buffer, PictureInfo *info) {
    if (!ctrl || !buffer || !info) return ERR_ARGUMENT_POINTER;
    if (ctrl->this_size!=sizeof(*ctrl) || buffer->this_size!=sizeof(*buffer) || info->this_size!=sizeof(*info)) return ERR_STRUCT_SIZE;
    if (!ctrl->handle) return ERR_HANDLE;
    if (!buffer->frame_buffer) return ERR_FRAME_BUFFER_POINTER;
    info->is_valid=0;
    return 0;
}
static ABI int32_t videodec_decode(Ctrl *ctrl, const InputData *input, FrameBuffer *buffer, PictureInfo *info) {
    if (!input) return ERR_ARGUMENT_POINTER;
    int32_t r=check(ctrl,buffer,info);
    if (r) return r;
    if (!input->au_data) return ERR_AU_POINTER;
    if (!input->au_size || input->au_size>INT32_MAX) return ERR_AU_SIZE;
    Decoder *d=ctrl->handle;
    d->packet->data=input->au_data; d->packet->size=(int)input->au_size;
    d->packet->pts=(int64_t)input->pts_data; d->packet->dts=(int64_t)input->dts_data;
    d->packet->opaque=(void *)(uintptr_t)input->attached_data;
    int sent=avcodec_send_packet(d->codec,d->packet);
    if (sent==AVERROR_EOF) { avcodec_flush_buffers(d->codec); sent=avcodec_send_packet(d->codec,d->packet); } /* after a flush */
    d->packet->data=NULL; d->packet->size=0;
    return sent<0 ? ERR_API_FAIL : receive(d,buffer,info);
}
/* One buffered picture per call; the game calls it until no picture comes back. */
static ABI int32_t videodec_flush(Ctrl *ctrl, FrameBuffer *buffer, PictureInfo *info) {
    int32_t r=check(ctrl,buffer,info);
    if (r) return r;
    Decoder *d=ctrl->handle;
    avcodec_send_packet(d->codec,NULL); /* AVERROR_EOF once draining: the rest still comes out */
    return receive(d,buffer,info);
}
static ABI int32_t videodec_reset(Ctrl *ctrl) {
    if (!ctrl) return ERR_ARGUMENT_POINTER;
    if (!ctrl->handle) return ERR_HANDLE;
    avcodec_flush_buffers(((Decoder *)ctrl->handle)->codec);
    return 0;
}
static ABI int32_t videodec_delete(Ctrl *ctrl) {
    if (!ctrl) return ERR_ARGUMENT_POINTER;
    Decoder *d=ctrl->handle;
    if (!d) return ERR_HANDLE;
    avcodec_free_context(&d->codec); av_packet_free(&d->packet); av_frame_free(&d->frame); av_frame_free(&d->nv12);
    sws_freeContext(d->sws);
    free(d);
    ctrl->handle=NULL;
    return 0;
}

/* libSceVideodec2 (newer SDKs; CRI movies in Unicorn Overlord): the same decoder with other
 * structures. Layouts and memory sizes follow shadPS4's videodec2. The picture information sits
 * in the frame buffer right after the picture (sceVideodec2GetPictureInfo reads it there). */
#define ERR2_ARGUMENT_POINTER ((int32_t)0x811D0102)
#define ERR2_STRUCT_SIZE ((int32_t)0x811D0101)
#define ERR2_DECODER_INSTANCE ((int32_t)0x811D0103)
#define ERR2_CONFIG_INFO ((int32_t)0x811D0200)
#define ERR2_MEMORY_POINTER ((int32_t)0x811D0105)
#define ERR2_FRAME_BUFFER_SIZE ((int32_t)0x811D0106)
#define ERR2_ACCESS_UNIT_POINTER ((int32_t)0x811D010E)
#define ERR2_ACCESS_UNIT_SIZE ((int32_t)0x811D010D)
#define ERR2_COMPUTE_PIPE_ID ((int32_t)0x811D0201)
#define ERR2_COMPUTE_QUEUE_ID ((int32_t)0x811D0202)
#define ERR2_API_FAIL ((int32_t)0x811D0100)
#define ERR2_CODEC_TYPE ((int32_t)0x811D0204)
#define CODEC2_AVC 1u
#define CODEC2_HEVC 974921u

typedef struct {
    uint64_t this_size;
    uint32_t resource_type, codec_type, profile, max_level;
    int32_t max_frame_width, max_frame_height, max_dpb_frame_count;
    uint32_t decode_pipeline_depth;
    void *compute_queue;
    uint64_t cpu_affinity_mask;
    int32_t cpu_thread_priority;
    uint8_t optimize_progressive_video, check_memory_type, reserved0, reserved1;
    void *extra_config_info;
} Config2;
typedef struct {
    uint64_t this_size, cpu_memory_size;
    void *cpu_memory;
    uint64_t gpu_memory_size;
    void *gpu_memory;
    uint64_t cpu_gpu_memory_size;
    void *cpu_gpu_memory;
    uint64_t max_frame_buffer_size;
    uint32_t frame_buffer_alignment, reserved0;
} Memory2;
typedef struct {
    uint64_t this_size;
    uint8_t is_valid, is_error_frame, picture_count;
    uint32_t codec_type, frame_width, frame_pitch, frame_height;
    void *frame_buffer;
    uint64_t frame_buffer_size;
    uint32_t frame_format, frame_pitch_in_bytes; /* only in the 0x38-byte version */
} Output2;
typedef struct { uint64_t this_size; void *frame_buffer; uint64_t frame_buffer_size; uint8_t is_accepted; } FrameBuffer2;
typedef struct { uint64_t this_size, cpu_gpu_memory_size; void *cpu_gpu_memory; } ComputeMemory2;
typedef struct { uint64_t this_size; uint16_t compute_pipe_id, compute_queue_id; uint8_t check_memory_type, reserved0; uint16_t reserved1; } ComputeConfig2;
/* OrbisVideodec2AvcPictureInfo up to the crop offsets; the HEVC structure starts the same way. */
typedef struct {
    uint64_t this_size;
    uint8_t is_valid;
    uint64_t pts_data, dts_data, attached_data;
    uint8_t idr_pictureflag, profile_idc, level_idc;
    uint32_t pic_width_in_mbs_minus1, pic_height_in_map_units_minus1;
    uint8_t frame_mbs_only_flag, frame_cropping_flag;
    uint32_t crop_left, crop_right, crop_top, crop_bottom;
} AvcPicture2;
_Static_assert(sizeof(Config2)==0x48 && sizeof(Memory2)==0x48 && sizeof(Output2)==0x38 && sizeof(FrameBuffer2)==0x20 &&
               sizeof(ComputeMemory2)==0x18 && sizeof(ComputeConfig2)==0x10 && sizeof(InputData)==0x30 &&
               offsetof(AvcPicture2,attached_data)==0x20 && offsetof(AvcPicture2,crop_left)==0x38,"Videodec2 guest layouts");
#define AVC_PICTURE2_SIZE 0x78u
#define HEVC_PICTURE2_SIZE 0xB8u
#define VIDEODEC2_MEMORY (16u<<20)

typedef struct { Decoder d; uint32_t codec_type; } Decoder2;

static ABI int32_t videodec2_query_compute(ComputeMemory2 *info) {
    if (!info) return ERR2_ARGUMENT_POINTER;
    if (info->this_size!=sizeof(*info)) return ERR2_STRUCT_SIZE;
    info->cpu_gpu_memory=NULL; info->cpu_gpu_memory_size=VIDEODEC2_MEMORY;
    return 0;
}
static ABI int32_t videodec2_allocate_queue(const ComputeConfig2 *config, const ComputeMemory2 *memory, void **queue) {
    if (!config || !memory || !queue) return ERR2_ARGUMENT_POINTER;
    if (config->this_size!=sizeof(*config) || memory->this_size!=sizeof(*memory)) return ERR2_STRUCT_SIZE;
    if (config->reserved0 || config->reserved1) return ERR2_CONFIG_INFO;
    if (config->compute_pipe_id>4) return ERR2_COMPUTE_PIPE_ID;
    if (config->compute_queue_id>7) return ERR2_COMPUTE_QUEUE_ID;
    if (!memory->cpu_gpu_memory) return ERR2_MEMORY_POINTER;
    *queue=memory->cpu_gpu_memory; /* the real library returns a pointer into that memory */
    return 0;
}
static ABI int32_t videodec2_release_queue(void *queue) { (void)queue; return 0; }
static ABI int32_t videodec2_query_decoder(const Config2 *config, Memory2 *out) {
    if (!config || !out) return ERR2_ARGUMENT_POINTER;
    if (config->this_size!=sizeof(*config) || out->this_size!=sizeof(*out)) return ERR2_STRUCT_SIZE;
    int32_t width=config->max_frame_width, height=config->max_frame_height;
    if (width<=0 || height<=0) { width=config->max_level>=150 ? 3840 : 1920; height=config->max_level>=150 ? 2160 : 1080; }
    uint64_t frame=(uint64_t)align((uint32_t)width,64)*align((uint32_t)height,16)*3/2;
    out->cpu_memory=NULL; out->gpu_memory=NULL; out->cpu_gpu_memory=NULL;
    out->cpu_memory_size=out->gpu_memory_size=out->cpu_gpu_memory_size=VIDEODEC2_MEMORY;
    out->max_frame_buffer_size=((frame+255)&~UINT64_C(255))+0x4000; /* room for the picture information */
    out->frame_buffer_alignment=0x100;
    return 0;
}
static ABI int32_t videodec2_create(const Config2 *config, const Memory2 *memory, Decoder2 **out) {
    if (!config || !memory || !out) return ERR2_ARGUMENT_POINTER;
    if (config->this_size!=sizeof(*config) || memory->this_size!=sizeof(*memory)) return ERR2_STRUCT_SIZE;
    if (config->codec_type!=CODEC2_AVC && config->codec_type!=CODEC2_HEVC) return ERR2_CODEC_TYPE;
    const AVCodec *codec=avcodec_find_decoder(config->codec_type==CODEC2_AVC ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC);
    Decoder2 *d2=calloc(1,sizeof(*d2));
    if (!codec || !d2) { free(d2); return ERR2_API_FAIL; }
    Decoder *d=&d2->d;
    d2->codec_type=config->codec_type;
    d->codec=avcodec_alloc_context3(codec);
    d->packet=av_packet_alloc();
    d->frame=av_frame_alloc();
    d->nv12=av_frame_alloc();
    if (d->codec) d->codec->flags|=AV_CODEC_FLAG_COPY_OPAQUE;
    if (!d->codec || !d->packet || !d->frame || !d->nv12 || avcodec_open2(d->codec,codec,NULL)<0) {
        avcodec_free_context(&d->codec); av_packet_free(&d->packet); av_frame_free(&d->frame); av_frame_free(&d->nv12);
        free(d2); return ERR2_API_FAIL;
    }
    *out=d2;
    return 0;
}
static ABI int32_t videodec2_delete(Decoder2 *d2) {
    if (!d2) return ERR2_DECODER_INSTANCE;
    Decoder *d=&d2->d;
    avcodec_free_context(&d->codec); av_packet_free(&d->packet); av_frame_free(&d->frame); av_frame_free(&d->nv12);
    sws_freeContext(d->sws);
    free(d2);
    return 0;
}
static int32_t picture2(Decoder2 *d2, FrameBuffer2 *buffer, Output2 *out) {
    Decoder *d=&d2->d;
    uint32_t info_size=d2->codec_type==CODEC2_AVC ? AVC_PICTURE2_SIZE : HEVC_PICTURE2_SIZE;
    if (buffer->frame_buffer_size<info_size) return ERR2_FRAME_BUFFER_SIZE;
    uint32_t width, pitch, rows;
    int32_t r=copy_nv12(d,buffer->frame_buffer,buffer->frame_buffer_size-info_size,&width,&pitch,&rows);
    if (r) return r==ERR_FRAME_BUFFER_SIZE ? ERR2_FRAME_BUFFER_SIZE : ERR2_API_FAIL;
    buffer->is_accepted=1;
    out->is_valid=1; out->is_error_frame=0; out->picture_count=1;
    out->codec_type=d2->codec_type;
    out->frame_width=align(width,16); out->frame_pitch=pitch; out->frame_height=rows;
    out->frame_buffer=buffer->frame_buffer;
    out->frame_buffer_size=(uint64_t)pitch*rows*3/2;
    if (out->this_size==sizeof(*out)) { out->frame_format=0; out->frame_pitch_in_bytes=pitch; }
    unsigned char *at=(unsigned char *)buffer->frame_buffer+out->frame_buffer_size;
    memset(at,0,info_size);
    AvcPicture2 *info=(AvcPicture2 *)at; /* the HEVC fields up to attached_data are the same */
    info->this_size=info_size; info->is_valid=1;
    info->pts_data=(uint64_t)d->frame->pts;
    info->dts_data=(uint64_t)d->frame->pkt_dts;
    info->attached_data=(uint64_t)(uintptr_t)d->frame->opaque;
    if (d2->codec_type==CODEC2_AVC) { info->crop_right=pitch-width; info->crop_bottom=rows-(uint32_t)d->frame->height; }
    return 0;
}
static int32_t receive2(Decoder2 *d2, FrameBuffer2 *buffer, Output2 *out) {
    av_frame_unref(d2->d.frame);
    int r=avcodec_receive_frame(d2->d.codec,d2->d.frame);
    if (r==AVERROR(EAGAIN) || r==AVERROR_EOF) return 0;
    return r<0 ? ERR2_API_FAIL : picture2(d2,buffer,out);
}
static int32_t check2(Decoder2 *d2, FrameBuffer2 *buffer, Output2 *out) {
    if (!d2) return ERR2_DECODER_INSTANCE;
    if (!buffer || !out) return ERR2_ARGUMENT_POINTER;
    if (buffer->this_size!=sizeof(*buffer) || (out->this_size|8)!=sizeof(*out)) return ERR2_STRUCT_SIZE;
    buffer->is_accepted=0; out->is_valid=0; out->picture_count=0;
    return 0;
}
static ABI int32_t videodec2_decode(Decoder2 *d2, const InputData *input, FrameBuffer2 *buffer, Output2 *out) {
    int32_t r=check2(d2,buffer,out);
    if (r) return r;
    if (!input) return ERR2_ARGUMENT_POINTER;
    if (input->this_size!=sizeof(*input)) return ERR2_STRUCT_SIZE;
    if (!input->au_data) return ERR2_ACCESS_UNIT_POINTER;
    if (!input->au_size || input->au_size>INT32_MAX) return ERR2_ACCESS_UNIT_SIZE;
    Decoder *d=&d2->d;
    d->packet->data=input->au_data; d->packet->size=(int)input->au_size;
    d->packet->pts=(int64_t)input->pts_data; d->packet->dts=(int64_t)input->dts_data;
    d->packet->opaque=(void *)(uintptr_t)input->attached_data;
    int sent=avcodec_send_packet(d->codec,d->packet);
    if (sent==AVERROR_EOF) { avcodec_flush_buffers(d->codec); sent=avcodec_send_packet(d->codec,d->packet); }
    d->packet->data=NULL; d->packet->size=0;
    return sent<0 ? ERR2_API_FAIL : receive2(d2,buffer,out);
}
static ABI int32_t videodec2_flush(Decoder2 *d2, FrameBuffer2 *buffer, Output2 *out) {
    int32_t r=check2(d2,buffer,out);
    if (r) return r;
    avcodec_send_packet(d2->d.codec,NULL);
    return receive2(d2,buffer,out);
}
static ABI int32_t videodec2_reset(Decoder2 *d2) {
    if (!d2) return ERR2_DECODER_INSTANCE;
    avcodec_flush_buffers(d2->d.codec);
    return 0;
}
static ABI int32_t videodec2_picture_info(const Output2 *out, void *first, void *second) {
    (void)second;
    if (!out) return ERR2_ARGUMENT_POINTER;
    if ((out->this_size|8)!=sizeof(*out)) return ERR2_STRUCT_SIZE;
    if (!out->picture_count || !first) return 0;
    const unsigned char *info=(const unsigned char *)out->frame_buffer+out->frame_buffer_size;
    uint64_t size=*(const uint64_t *)first, have=*(const uint64_t *)info;
    if (size>have) size=have;
    if (size>8) memcpy((unsigned char *)first+8,info+8,size-8); /* the caller's size field stays */
    return 0;
}

static const RuntimeExport exports[]={
    {"sceVideodecQueryResourceInfo",videodec_query}, {"sceVideodecCreateDecoder",videodec_create},
    {"sceVideodecDecode",videodec_decode}, {"sceVideodecFlush",videodec_flush},
    {"sceVideodecReset",videodec_reset}, {"sceVideodecDeleteDecoder",videodec_delete},
    {"sceVideodec2QueryComputeMemoryInfo",videodec2_query_compute}, {"sceVideodec2AllocateComputeQueue",videodec2_allocate_queue},
    {"sceVideodec2ReleaseComputeQueue",videodec2_release_queue}, {"sceVideodec2QueryDecoderMemoryInfo",videodec2_query_decoder},
    {"sceVideodec2CreateDecoder",videodec2_create}, {"sceVideodec2DeleteDecoder",videodec2_delete},
    {"sceVideodec2Decode",videodec2_decode}, {"sceVideodec2Flush",videodec2_flush}, {"sceVideodec2Reset",videodec2_reset},
    {"sceVideodec2GetPictureInfo",videodec2_picture_info}, {"sceVideodec2GetAvcPictureInfo",videodec2_picture_info},
};
uintptr_t runtime_videodec_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
