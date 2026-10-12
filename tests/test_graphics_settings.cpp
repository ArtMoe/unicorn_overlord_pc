// vw.ini graphics keys (vw_settings.cpp, vw_graphics.h).
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include "vw_graphics.h"
#include "vw_settings.h"
#ifndef _WIN32
static int _putenv_s(const char* name, const char* value) {
    return value[0] ? setenv(name, value, 1) : unsetenv(name);
}
#endif

namespace G = VwGraphics;

static std::string Read(const char* path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

int main() {
    assert(G::ParseDisplayMode("borderless") == G::Borderless && G::ParseDisplayMode("bogus") == G::Windowed);
    assert(G::ParseFpsLimit(240) == 240 && G::ParseFpsLimit(75) == 0);
    assert(G::ParseResolution("1440p") == 1 && G::ParseResolution("4K") == 3 && G::ParseResolution("8K") == 0);
    assert(G::ParseFramesQueued(2) == 2 && G::ParseFramesQueued(0) == 0 && G::ParseFramesQueued(5) == 1);
    assert(G::ParseRcasStrength(100) == 100 && G::ParseRcasStrength(101) == 75 && G::ParseRcasStrength(-1) == 75);
    assert(G::RcasAttenuation(100) == 0.0f && G::RcasAttenuation(0) == 2.0f && G::RcasAttenuation(75) == 0.5f);
    assert(G::FsrPassRuns(true, false, 1280, 720, 2560, 1440) && !G::FsrPassRuns(false, false, 1280, 720, 2560, 1440));
    assert(G::FsrPassRuns(false, true, 1920, 1080, 1920, 1080) && !G::FsrPassRuns(true, false, 1920, 1080, 1920, 1080));
    assert(!G::FsrPassRuns(true, true, 2560, 1440, 1920, 1080));  // larger image: regular scaling

    const char* path = "graphics-settings-test.ini";
    {
        std::ofstream out(path);
        out << "display_mode=borderless\nvsync=0\nfps_limit=240\nasync_shaders=0\n"
               "render_resolution=1440p\ndeferred_readback=0\nstale_readback=1\n"
               "fsr1=1\nrcas=0\nrcas_strength=40\nframes_queued=2\n";
    }
    _putenv_s("VW_CONFIG", path);
    _putenv_s("VW_ASYNC_SHADERS", "");
    auto& v = VwSettings::Get();
    VwSettings::Load();
    assert(v.display_mode == G::Borderless && !v.vsync && v.fps_limit == 240 && !v.async_shaders);
    assert(v.render_resolution == 1 && !v.deferred_readback && v.stale_readback);
    assert(v.fsr1 && !v.rcas && v.rcas_strength == 40 && v.frames_queued == 2);
    assert(!VwSettings::GraphicsNeedRestart());
    v.render_resolution = 3;
    assert(VwSettings::GraphicsNeedRestart());
    v.fps_limit = 60;  // live: no restart because of it
    v.render_resolution = 1;
    assert(!VwSettings::GraphicsNeedRestart());

    VwSettings::Save();
    const std::string saved = Read(path);
    for (const char* line : {"display_mode=borderless\n", "vsync=0\n", "fps_limit=60\n", "async_shaders=0\n",
                             "render_resolution=1440p\n", "deferred_readback=0\n",
                             "stale_readback=1\n", "fsr1=1\n", "rcas=0\n", "rcas_strength=40\n",
                             "frames_queued=2\n"}) {
        assert(saved.find(line) != std::string::npos);
    }

    {
        std::ofstream out(path);
        out << "display_mode=huge\nfps_limit=7\nrender_resolution=8K\n"
               "rcas_strength=250\nframes_queued=9\n";
    }
    _putenv_s("VW_ASYNC_SHADERS", "1");  // the environment still wins at start
    v.async_shaders = false;
    VwSettings::Load();
    assert(v.display_mode == G::Windowed && v.fps_limit == 0 && v.render_resolution == 0);
    assert(v.rcas_strength == 75 && v.frames_queued == 1);
    assert(v.async_shaders);
    std::remove(path);
    std::puts("PASS: graphics settings");
}
