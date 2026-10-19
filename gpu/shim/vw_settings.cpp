// SPDX-License-Identifier: GPL-2.0-or-later
#include "vw_settings.h"
#include "vw_graphics.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace VwSettings {

namespace {

const char* Path() {
    const char* env = std::getenv("VW_CONFIG");
    return env && env[0] ? env : "vw.ini";
}

std::mutex text_mutex;
std::map<std::string, std::string> texts; // key_map, pad_map, button_icons
constexpr const char* TextKeys[] = {"key_map", "pad_map", "button_icons"};

void Set(Values& v, const std::string& key, const std::string& value) {
    for (const char* text_key : TextKeys) {
        if (key == text_key) {
            std::scoped_lock lock{text_mutex};
            texts[key] = value;
            return;
        }
    }
    const int i = std::atoi(value.c_str());
    static const char* const overlay_keys[] = {"overlay_fps", "overlay_frametime", "overlay_resolution",
                                               "overlay_gpu", "overlay_cpu", "overlay_ram", "overlay_vram"};
    for (unsigned item = 0; item < 7; ++item) {
        if (key == overlay_keys[item]) {
            const unsigned bit = 1u << item;
            v.overlay_items = i ? (v.overlay_items | bit) : (v.overlay_items & ~bit);
            return;
        }
    }
    if (key == "overlay_scale") {
        v.overlay_scale = std::clamp(i, 50, 300);
        return;
    }
    if (key == "overlay_corner") {
        v.overlay_corner = i >= 0 && i <= 3 ? i : 1;
        return;
    }
    if (key == "overlay_opacity") {
        v.overlay_opacity = std::clamp(i, 0, 100);
        return;
    }
    if (key == "overlay_layout") {
        v.overlay_layout = i == 1 ? 1 : 0;
        return;
    }
    if (key == "menu_tab") {
        v.menu_tab = i >= 0 && i <= 3 ? i : 0;
        return;
    }
    if (key == "show_fps") {
        v.show_fps = i != 0;
    } else if (key == "display_mode") {
        v.display_mode = VwGraphics::ParseDisplayMode(value);
    } else if (key == "vsync") {
        v.vsync = i != 0;
    } else if (key == "fps_limit") {
        v.fps_limit = VwGraphics::ParseFpsLimit(i);
    } else if (key == "async_shaders") {
        v.async_shaders = i != 0;
    } else if (key == "fsr1") {
        v.fsr1 = i != 0;
    } else if (key == "rcas") {
        v.rcas = i != 0;
    } else if (key == "rcas_strength") {
        v.rcas_strength = VwGraphics::ParseRcasStrength(value.empty() ? -1 : i);
    } else if (key == "frames_queued") {
        v.frames_queued = VwGraphics::ParseFramesQueued(value.empty() ? -1 : i);
    } else if (key == "render_resolution") {
        v.render_resolution = VwGraphics::ParseResolution(value);
    } else if (key == "deferred_readback") {
        v.deferred_readback = i != 0;
    } else if (key == "stale_readback") {
        v.stale_readback = i != 0;
    } else if (key == "restart_unconfirmed") {
        v.restart_unconfirmed = i != 0;
    }
}

} // namespace

std::string Text(const std::string& key) {
    std::scoped_lock lock{text_mutex};
    const auto it = texts.find(key);
    return it == texts.end() ? std::string{} : it->second;
}

void SetText(const std::string& key, const std::string& value) {
    std::scoped_lock lock{text_mutex};
    texts[key] = value;
}

Values& Get() {
    static Values values;
    return values;
}

void Load() {
    auto& v = Get();
    if (FILE* file = std::fopen(Path(), "r")) {
        char line[1280];
        while (std::fgets(line, sizeof(line), file)) {
            std::string text{line};
            text.erase(text.find_last_not_of(" \t\r\n") + 1);
            const auto eq = text.find('=');
            if (text.empty() || text[0] == '#' || eq == std::string::npos) {
                continue;
            }
            Set(v, text.substr(0, eq), text.substr(eq + 1));
        }
        std::fclose(file);
        std::printf("Settings: %s\n", Path());
    }
    // Environment overrides (scripts, A/B tests).
    const std::pair<const char*, const char*> env_keys[] = {
        {"VW_ASYNC_SHADERS", "async_shaders"}, {"VW_DEFERRED_READBACK", "deferred_readback"},
        {"VW_STALE_READBACK", "stale_readback"}, {"VW_FSR1", "fsr1"},
        {"VW_RCAS", "rcas"},                   {"VW_FRAMES_AHEAD", "frames_queued"},
    };
    for (const auto& [env, key] : env_keys) {
        if (const char* value = std::getenv(env); value && value[0]) {
            Set(v, key, value);
        }
    }
    v.startup_render_resolution = v.render_resolution;
    v.startup_deferred_readback = v.deferred_readback;
    v.startup_stale_readback = v.stale_readback;
}

bool GraphicsNeedRestart() {
    const auto& v = Get();
    return v.render_resolution != v.startup_render_resolution ||
           v.deferred_readback != v.startup_deferred_readback ||
           v.stale_readback != v.startup_stale_readback;
}

void Save() {
    const auto& v = Get();
    FILE* file = std::fopen(Path(), "w");
    if (!file) {
        std::printf("Settings: cannot write %s\n", Path());
        return;
    }
    std::fprintf(file, "# vw settings (in-game menu: Insert / R3+L2)\nshow_fps=%d\n",
                 int(v.show_fps.load()));
    std::fprintf(file,
                 "overlay_fps=%d\noverlay_frametime=%d\noverlay_resolution=%d\noverlay_gpu=%d\n"
                 "overlay_cpu=%d\noverlay_ram=%d\noverlay_vram=%d\noverlay_scale=%d\noverlay_corner=%d\n"
                 "overlay_opacity=%d\noverlay_layout=%d\nmenu_tab=%d\n",
                 int(v.overlay_items >> 0 & 1), int(v.overlay_items >> 1 & 1), int(v.overlay_items >> 2 & 1),
                 int(v.overlay_items >> 3 & 1), int(v.overlay_items >> 4 & 1), int(v.overlay_items >> 5 & 1),
                 int(v.overlay_items >> 6 & 1), v.overlay_scale.load(), v.overlay_corner.load(),
                 v.overlay_opacity.load(), v.overlay_layout.load(), v.menu_tab.load());
    std::fprintf(file,
                 "display_mode=%s\nvsync=%d\nfps_limit=%d\nasync_shaders=%d\n"
                 "fsr1=%d\nrcas=%d\nrcas_strength=%d\nframes_queued=%d\n"
                 "render_resolution=%s\ndeferred_readback=%d\nstale_readback=%d\n"
                 "restart_unconfirmed=%d\n",
                 VwGraphics::DisplayModeKeys[std::clamp(v.display_mode.load(), 0, 2)].data(),
                 int(v.vsync.load()), v.fps_limit.load(), int(v.async_shaders.load()),
                 int(v.fsr1.load()), int(v.rcas.load()), v.rcas_strength.load(), v.frames_queued.load(),
                 VwGraphics::Resolutions[std::clamp(v.render_resolution.load(), 0,
                                                      int(VwGraphics::Resolutions.size()) - 1)]
                     .data(),
                 int(v.deferred_readback.load()), int(v.stale_readback.load()),
                 int(v.restart_unconfirmed.load()));
    {
        std::scoped_lock lock{text_mutex};
        for (const char* key : TextKeys) {
            if (const auto it = texts.find(key); it != texts.end() && !it->second.empty()) {
                std::fprintf(file, "%s=%s\n", key, it->second.c_str());
            }
        }
    }
    std::fclose(file);
}

} // namespace VwSettings
