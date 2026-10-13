/* libSceVideodec: H.264 access units to NV12 pictures (tests/data/tiny.h264: 6 frames of
 * 64x48 testsrc, made with ffmpeg -c:v libx264 -profile:v main -f h264). */
#undef NDEBUG /* the checks are asserts; release builds define NDEBUG */
#include "../src/runtime_videodec.c"
#include <assert.h>
#include <stdio.h>

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) { (void)table; (void)count; (void)nid; return 0; }

static unsigned char frame_buffer[1<<16];
static int pictures, first_luma_sum, seen;

static void check_picture(const PictureInfo *p) {
    if (!p->is_valid) return;
    assert(p->frame_width==64 && p->frame_pitch==64 && p->frame_height==48 && !p->is_error_pic);
    /* B-frames: pictures come in display order, each with its own access unit's data. */
    assert(p->attached_data>=1000 && p->attached_data<1006 && !(seen>>(p->attached_data-1000)&1));
    seen|=1<<(p->attached_data-1000);
    if (!pictures++) for (int i=0;i<64*48;++i) first_luma_sum+=frame_buffer[i];
}

int main(int argc, char **argv) {
    FILE *f=fopen(argc>1 ? argv[1] : "tests/data/tiny.h264","rb");
    assert(f);
    static unsigned char stream[1<<16];
    size_t size=fread(stream,1,sizeof(stream),f);
    fclose(f);

    ConfigInfo config={sizeof(ConfigInfo),0,0,0,64,48,4,0};
    ResourceInfo resource={sizeof(ResourceInfo),0,NULL,0,NULL,0,0};
    assert(videodec_query(&config,&resource)==0 && resource.max_frame_buffer_size>=64*48*3/2);
    assert(videodec_query(NULL,&resource)==ERR_ARGUMENT_POINTER);
    config.this_size=8;
    assert(videodec_query(&config,&resource)==ERR_STRUCT_SIZE);
    config.this_size=sizeof(ConfigInfo);

    Ctrl ctrl={0};
    assert(videodec_create(&config,&resource,&ctrl)==0 && ctrl.handle && ctrl.this_size==sizeof(Ctrl));
    FrameBuffer buffer={sizeof(FrameBuffer),frame_buffer,sizeof(frame_buffer)};
    PictureInfo picture; memset(&picture,0,sizeof(picture)); picture.this_size=sizeof(picture);

    /* Access units as the game's demuxer delivers them. */
    const AVCodec *codec=avcodec_find_decoder(AV_CODEC_ID_H264);
    AVCodecParserContext *parser=av_parser_init(AV_CODEC_ID_H264);
    AVCodecContext *parse_context=avcodec_alloc_context3(codec);
    const unsigned char *p=stream;
    int units=0;
    for (int left=(int)size;;) {
        uint8_t *au; int au_size, flushing=!left; /* a call without input returns the last unit */
        int used=av_parser_parse2(parser,parse_context,&au,&au_size,p,left,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);
        p+=used; left-=used;
        if (au_size) {
            InputData input={sizeof(InputData),au,(uint64_t)au_size,(uint64_t)units,(uint64_t)units,1000+(uint64_t)units};
            assert(videodec_decode(&ctrl,&input,&buffer,&picture)==0);
            check_picture(&picture);
            ++units;
        }
        if (flushing && !au_size) break;
    }
    av_parser_close(parser);
    avcodec_free_context(&parse_context);
    assert(units==6);
    for (int i=0;i<8;++i) {
        assert(videodec_flush(&ctrl,&buffer,&picture)==0);
        check_picture(&picture);
    }
    assert(pictures==6 && seen==0x3f && first_luma_sum>0);

    /* Errors are codes, not crashes. */
    InputData empty={sizeof(InputData),stream,0,0,0,0};
    assert(videodec_decode(&ctrl,&empty,&buffer,&picture)==ERR_AU_SIZE);
    empty.au_data=NULL; empty.au_size=4;
    assert(videodec_decode(&ctrl,&empty,&buffer,&picture)==ERR_AU_POINTER);
    assert(videodec_decode(NULL,&empty,&buffer,&picture)==ERR_ARGUMENT_POINTER);
    FrameBuffer small={sizeof(FrameBuffer),frame_buffer,16};
    assert(videodec_reset(&ctrl)==0);
    InputData whole={sizeof(InputData),stream,(uint64_t)size,0,0,0};
    int32_t r=videodec_decode(&ctrl,&whole,&small,&picture);
    assert(r==0 || r==ERR_FRAME_BUFFER_SIZE);
    assert(!(r==0 && picture.is_valid));

    assert(videodec_delete(&ctrl)==0 && !ctrl.handle);
    assert(videodec_delete(&ctrl)==ERR_HANDLE);

    /* libSceVideodec2: the same stream; the picture information follows each picture. */
    Config2 config2; memset(&config2,0,sizeof(config2));
    config2.this_size=sizeof(config2); config2.codec_type=CODEC2_AVC; config2.max_frame_width=64; config2.max_frame_height=48;
    Memory2 memory2; memset(&memory2,0,sizeof(memory2)); memory2.this_size=sizeof(memory2);
    assert(videodec2_query_decoder(&config2,&memory2)==0 && memory2.max_frame_buffer_size>=64*48*3/2+AVC_PICTURE2_SIZE);
    Decoder2 *d2=NULL;
    assert(videodec2_create(&config2,&memory2,&d2)==0 && d2);
    FrameBuffer2 buffer2={sizeof(FrameBuffer2),frame_buffer,memory2.max_frame_buffer_size,0};
    Output2 out2; memset(&out2,0,sizeof(out2)); out2.this_size=sizeof(out2);
    parser=av_parser_init(AV_CODEC_ID_H264);
    parse_context=avcodec_alloc_context3(codec);
    p=stream;
    int pictures2=0, seen2=0;
    for (int left=(int)size, unit=0;;) {
        uint8_t *au; int au_size, flushing=!left;
        int used=av_parser_parse2(parser,parse_context,&au,&au_size,p,left,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);
        p+=used; left-=used;
        int done=flushing && !au_size;
        for (int drain=0; drain<(done ? 8 : 1); ++drain) {
            if (au_size) {
                InputData input={sizeof(InputData),au,(uint64_t)au_size,(uint64_t)unit,(uint64_t)unit,2000+(uint64_t)unit};
                assert(videodec2_decode(d2,&input,&buffer2,&out2)==0);
                ++unit;
            } else if (done) {
                assert(videodec2_flush(d2,&buffer2,&out2)==0);
            } else break;
            if (!out2.is_valid) continue;
            assert(buffer2.is_accepted && out2.picture_count==1 && out2.codec_type==CODEC2_AVC);
            assert(out2.frame_width==64 && out2.frame_pitch==64 && out2.frame_height==48 && out2.frame_pitch_in_bytes==64);
            assert(out2.frame_buffer==frame_buffer && out2.frame_buffer_size==64*48*3/2);
            AvcPicture2 info; memset(&info,0,sizeof(info)); info.this_size=sizeof(info);
            assert(videodec2_picture_info(&out2,&info,NULL)==0 && info.this_size==sizeof(info) && info.is_valid);
            assert(info.attached_data>=2000 && info.attached_data<2006 && !(seen2>>(info.attached_data-2000)&1));
            seen2|=1<<(info.attached_data-2000);
            ++pictures2;
        }
        if (done) break;
    }
    av_parser_close(parser);
    avcodec_free_context(&parse_context);
    assert(pictures2==6 && seen2==0x3f);
    InputData empty2={sizeof(InputData),stream,0,0,0,0};
    assert(videodec2_decode(d2,&empty2,&buffer2,&out2)==ERR2_ACCESS_UNIT_SIZE);
    assert(videodec2_decode(NULL,&empty2,&buffer2,&out2)==ERR2_DECODER_INSTANCE);
    out2.this_size=0x30; /* the older output structure */
    assert(videodec2_flush(d2,&buffer2,&out2)==0);
    assert(videodec2_reset(d2)==0 && videodec2_delete(d2)==0);

    puts("videodec test: ok");
    return 0;
}
