#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include "../src/runtime_audio.c"

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

/* PS4 8ch order: L R C LFE SL SR BL BR. */
static void fill(float *buffer, const float frame[8]) {
    for (int f=0;f<256;++f) memcpy(buffer+f*8,frame,sizeof(float)*8);
}
static int near(float a, float b) { return fabsf(a-b)<1e-4f; }

int main(void) {
    char path[]="/tmp/vw-audio-test-XXXXXX";
    int fd=mkstemp(path);
    assert(fd>=0);
    close(fd);
    /* SDL's disk driver writes what the device plays. It would take the port's 8 channels;
       opening it in stereo first stands in for a stereo WASAPI endpoint (mix format). */
    setenv("SDL_AUDIO_DRIVER","disk",1);
    setenv("SDL_AUDIO_DISK_OUTPUT_FILE",path,1);
    assert(SDL_InitSubSystem(SDL_INIT_AUDIO));
    const SDL_AudioSpec stereo={SDL_AUDIO_F32,2,48000};
    SDL_AudioDeviceID device=SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&stereo);
    assert(device);
    assert(audio_init()==0);
    int32_t handle=audio_open(0,0,0,256,48000,5); /* 8ch float */
    assert(handle>=0);
    assert(ports[handle & 0xff].downmix);

    static float buffer[256*8];
    static const float quiet[8]={0.5f,-0.25f,0.2f,0.9f,0.1f,0,0,0.1f};
    static const float loud[8]={0.9f,0,0.9f,0,0,0,0,0};
    fill(buffer,quiet);
    for (int i=0;i<40;++i) assert(audio_output(handle,buffer)==256*8);
    fill(buffer,loud);
    for (int i=0;i<40;++i) assert(audio_output(handle,buffer)==256*8);
    static const float silence[256*8];
    for (int i=0;i<20;++i) assert(audio_output(handle,silence)==256*8);
    usleep(200000);
    assert(audio_close(handle)==0);
    SDL_CloseAudioDevice(device);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    FILE *f=fopen(path,"rb");
    assert(f);
    float frame[2];
    int saw_quiet=0, saw_loud=0;
    while (fread(frame,sizeof(float),2,f)==2) {
        /* L = FL + 0.707 (C + SL + BL), R = FR + 0.707 (C + SR + BR), LFE dropped. */
        if (near(frame[0],0.5f+0.7071f*0.3f) && near(frame[1],-0.25f+0.7071f*0.3f)) ++saw_quiet;
        else if (near(frame[0],1.0f) && near(frame[1],0.7071f*0.9f)) ++saw_loud; /* L clamped */
        else assert(near(frame[0],0) && near(frame[1],0));
    }
    fclose(f);
    unlink(path);
    printf("audio test: %d mixed frames, %d clamped frames\n",saw_quiet,saw_loud);
    assert(saw_quiet>=256*30 && saw_loud>=256*30);
    puts("audio test passed");
    return 0;
}
