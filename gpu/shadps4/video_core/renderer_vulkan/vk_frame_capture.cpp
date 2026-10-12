// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/vk_frame_capture.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <format>
#include <mutex>

#include <xxhash.h>

#include "core/memory.h"
#include "shader_recompiler/info.h"
#include "video_core/amdgpu/regs.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/texture_cache/image_info.h"

namespace Vulkan {

namespace {

struct Target {
    VAddr address{};
    vk::Format format{};
    u32 width{}, height{};
    bool operator==(const Target&) const = default;
};

struct Entry {
    bool compute{};
    std::vector<Target> colors;
    Target depth;
    u32 draws{};
    u64 indices{};
    std::vector<u64> shaders; ///< vs/ps pairs (graphics) or cs (compute), first few
    std::vector<std::pair<Target, bool>> sampled; ///< textures (false) and storage images (true)
    std::string note;
    std::vector<std::string> buffers; ///< dumped constants, per draw
};

// GPU thread only.
std::vector<Entry> entries;
std::vector<std::pair<Target, bool>> pending_sampled;
std::vector<std::string> pending_buffers;

void AddBuffers(Entry& entry) {
    // Constants of full-screen passes (post-processing: camera matrices) and of the first draw
    // of each geometry pass.
    if (entry.draws <= 4) {
        for (auto& b : pending_buffers) {
            entry.buffers.push_back(std::move(b));
        }
    }
    pending_buffers.clear();
}
bool pass_open = false;
// VW_CAPTURE_FRAMES: display passes recorded into one capture (default 1).
u32 frames_left = 0;
std::mutex display_mutex;
std::vector<VAddr> display_buffers;
// vw: read on every draw; the game registers a handful of display buffers once.
std::array<std::atomic<VAddr>, 16> display_list{};
std::atomic<u32> display_count{0};

Target ToTarget(const VideoCore::ImageInfo& info) {
    return {info.guest_address, info.pixel_format, info.size.width, info.size.height};
}

std::string Describe(const Target& t) {
    if (!t.address) {
        return "-";
    }
    return std::format("{:#x} {} {}x{}", t.address, vk::to_string(t.format), t.width, t.height);
}

void AddSampled(Entry& entry) {
    for (const auto& s : pending_sampled) {
        if (entry.sampled.size() < 32 &&
            std::ranges::find(entry.sampled, s) == entry.sampled.end()) {
            entry.sampled.push_back(s);
        }
    }
    pending_sampled.clear();
}

void AddShader(Entry& entry, u64 hash) {
    if (entry.shaders.size() < 8 && std::ranges::find(entry.shaders, hash) == entry.shaders.end()) {
        entry.shaders.push_back(hash);
    }
}

std::mutex result_mutex;
std::string result; ///< for the menu (FrameCapture::LastResult)

void SetResult(std::string text) {
    std::printf("Frame capture: %s\n", text.c_str());
    std::scoped_lock lk{result_mutex};
    result = std::move(text);
}

// VW_CAPTURE_DIR, else <user>/captures (VW_GPU_USER_DIR, the saves folder).
std::filesystem::path CaptureDir() {
    if (const char* dir = std::getenv("VW_CAPTURE_DIR"); dir && dir[0]) {
        return dir;
    }
    const char* user = std::getenv("VW_GPU_USER_DIR");
    return std::filesystem::path(user && user[0] ? user : "user") / "captures";
}

void Write(VAddr presented) {
    const auto dir = CaptureDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::time_t now = std::time(nullptr);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
    static u32 sequence = 0;
    const std::string path = (dir / std::format("frame_{}_{}.txt", stamp, ++sequence)).string();
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        std::printf("Frame capture: cannot write %s\n", path.c_str());
        SetResult("Capture failed: cannot write " + path);
        return;
    }
    std::fprintf(f, "presented buffer %#llx\n", static_cast<unsigned long long>(presented));
    u32 index = 0;
    for (const auto& e : entries) {
        if (e.compute) {
            std::fprintf(f, "\n#%u COMPUTE dispatches %u, cs", index++, e.draws);
        } else {
            std::fprintf(f, "\n#%u PASS draws %u (indices %llu), vs/ps", index++, e.draws,
                         static_cast<unsigned long long>(e.indices));
        }
        for (const u64 h : e.shaders) {
            std::fprintf(f, " %016llx", static_cast<unsigned long long>(h));
        }
        std::fprintf(f, "\n");
        for (size_t i = 0; i < e.colors.size(); ++i) {
            std::fprintf(f, "  color%zu %s\n", i, Describe(e.colors[i]).c_str());
        }
        if (!e.compute) {
            std::fprintf(f, "  depth  %s\n", Describe(e.depth).c_str());
        }
        for (const auto& [t, storage] : e.sampled) {
            std::fprintf(f, "  %s %s\n", storage ? "writes " : "samples", Describe(t).c_str());
        }
        if (!e.note.empty()) {
            std::fprintf(f, "  note: %s\n", e.note.c_str());
        }
        for (const auto& b : e.buffers) {
            std::fprintf(f, "%s\n", b.c_str());
        }
    }
    std::fclose(f);
    std::printf("Frame capture: %zu passes written to %s\n", entries.size(), path.c_str());
    SetResult(std::format("Frame captured ({} passes): {}", entries.size(), path));
}

} // namespace

void FrameCapture::OnFlip(VAddr presented_address) {
    last_presented.store(presented_address, std::memory_order_relaxed);
    flips.fetch_add(1, std::memory_order_release);
    static const char* trigger = std::getenv("VW_CAPTURE_TRIGGER");
    if (state.load(std::memory_order_relaxed) != Idle) {
        return;
    }
    bool arm = requested.exchange(false, std::memory_order_acq_rel);
    if (!arm && trigger && std::filesystem::exists(trigger)) {
        std::error_code ec;
        std::filesystem::remove(trigger, ec);
        arm = true;
    }
    if (arm) {
        state.store(Armed, std::memory_order_release);
        std::printf("Frame capture: armed\n");
    }
}

void FrameCapture::Request() {
    SetResult("Capturing the next frame...");
    requested.store(true, std::memory_order_release);
}

std::string FrameCapture::LastResult() {
    std::scoped_lock lk{result_mutex};
    return result;
}

void FrameCapture::Poll() {}

bool FrameCapture::IsDisplayBuffer(VAddr address) {
    const u32 count = display_count.load(std::memory_order_acquire);
    for (u32 i = 0; i < count; ++i) {
        if (display_list[i].load(std::memory_order_relaxed) == address) {
            return true;
        }
    }
    if (count < display_list.size()) {
        return false;
    }
    std::scoped_lock lk{display_mutex};
    return std::ranges::find(display_buffers, address) != display_buffers.end();
}

void FrameCapture::AddDisplayBuffer(VAddr address) {
    std::scoped_lock lk{display_mutex};
    if (std::ranges::find(display_buffers, address) == display_buffers.end()) {
        display_buffers.push_back(address);
        const u32 count = display_count.load(std::memory_order_relaxed);
        if (count < display_list.size()) {
            display_list[count].store(address, std::memory_order_relaxed);
            display_count.store(count + 1, std::memory_order_release);
        }
    }
}

void FrameCapture::BeginPass(const VideoCore::ImageInfo* const* colors, u32 num_colors,
                             const VideoCore::ImageInfo* depth) {
    std::vector<Target> targets;
    bool display = false;
    {
        std::scoped_lock lk{display_mutex};
        for (u32 i = 0; i < num_colors; ++i) {
            targets.push_back(colors[i] ? ToTarget(*colors[i]) : Target{});
            display |= colors[i] && std::ranges::find(display_buffers, colors[i]->guest_address) !=
                                        display_buffers.end();
        }
    }
    // Frame boundaries in the command stream: the pass that writes a display buffer as its only
    // target. vw: a game may also draw scene passes (two targets) into display buffers.
    display = display && targets.size() == 1;
    const bool new_pass = !(pass_open && !entries.empty() && !entries.back().compute &&
                            entries.back().colors == targets);
    if (display && new_pass) {
        if (state.load(std::memory_order_relaxed) == Recording && frames_left > 1) {
            --frames_left;
            entries.push_back({.compute = false, .colors = targets, .note = "--- display frame ---"});
            pass_open = false;
        } else if (state.load(std::memory_order_relaxed) == Recording) {
            Write(last_presented.load(std::memory_order_relaxed));
            entries.clear();
            pending_sampled.clear();
            pending_buffers.clear();
            state.store(Idle, std::memory_order_release);
            return;
        } else {
            entries.clear();
            pending_sampled.clear();
            pending_buffers.clear();
            pass_open = false;
            const char* frames = std::getenv("VW_CAPTURE_FRAMES");
            frames_left = frames ? std::max(1, std::atoi(frames)) : 1;
            state.store(Recording, std::memory_order_release);
        }
    }
    if (state.load(std::memory_order_relaxed) != Recording) {
        pending_sampled.clear();
        pending_buffers.clear();
        return;
    }
    const Target d = depth ? ToTarget(*depth) : Target{};
    if (pass_open && !entries.empty() && !entries.back().compute &&
        entries.back().colors == targets && entries.back().depth == d) {
        return;
    }
    entries.push_back({.compute = false, .colors = std::move(targets), .depth = d});
    pass_open = true;
}

void FrameCapture::Draw(u64 vs_hash, u64 ps_hash, u32 num_indices, u32 num_instances) {
    if (state.load(std::memory_order_relaxed) != Recording || entries.empty() ||
        entries.back().compute) {
        return;
    }
    auto& e = entries.back();
    ++e.draws;
    e.indices += u64(num_indices) * std::max(num_instances, 1u);
    AddShader(e, vs_hash);
    AddShader(e, ps_hash);
    AddSampled(e);
    AddBuffers(e);
}

void FrameCapture::Dispatch(u64 cs_hash, u32 x, u32 y, u32 z) {
    if (state.load(std::memory_order_relaxed) != Recording) {
        pending_sampled.clear();
        pending_buffers.clear();
        return;
    }
    if (!entries.empty() && entries.back().compute && !entries.back().shaders.empty() &&
        entries.back().shaders[0] == cs_hash) {
        ++entries.back().draws;
    } else {
        entries.push_back({.compute = true, .draws = 1, .shaders = {cs_hash}});
        entries.back().note = std::format("groups {}x{}x{}", x, y, z);
    }
    AddSampled(entries.back());
    AddBuffers(entries.back());
    pass_open = false;
}

void FrameCapture::Buffer(u64 stage_hash, u32 slot, VAddr address, const void* data, u64 size) {
    const u64 bytes = std::min<u64>(size, 1024) & ~u64(3);
    std::string out = std::format("  buffer stage {:016x} slot {} at {:#x} size {}:", stage_hash,
                                  slot, address, size);
    const auto* words = static_cast<const float*>(data);
    for (u64 i = 0; i < bytes / 4; ++i) {
        if (i % 8 == 0) {
            out += std::format("\n    [{:3}]", i);
        }
        out += std::format(" {:12.6g}", words[i]);
    }
    pending_buffers.push_back(std::move(out));
}

void FrameCapture::Sampled(const VideoCore::ImageInfo& info, bool storage) {
    if (pending_sampled.size() < 64) {
        pending_sampled.emplace_back(ToTarget(info), storage);
    }
}

void FrameCapture::Note(const char* text) {
    if (!entries.empty()) {
        entries.back().note += text;
    }
}

namespace {
bool Detail() {
    static const bool detail = [] {
        const char* env = std::getenv("VW_CAPTURE_DETAIL");
        return env && env[0] == '1';
    }();
    return detail;
}
/// A hash of guest memory, 0 when the range is not mapped.
u64 MemoryHash(VAddr address, u64 size) {
    if (!address || !size || !Core::Memory::Instance()->IsValidGpuMapping(address, 0)) {
        return 0;
    }
    return XXH3_64bits(reinterpret_cast<const void*>(address), size);
}
} // namespace

void FrameCapture::NoteDraw(const AmdGpu::Regs& regs, u32 num_indices, const Shader::Info* ps) {
    if (!Detail()) {
        return;
    }
    const auto& vp = regs.viewports[0];
    const auto& ss = regs.screen_scissor;
    const auto& gs = regs.generic_scissor;
    const auto& vs0 = regs.viewport_scissors[0];
    const auto& ws = regs.window_scissor;
    Note(std::format("\n  draw prim {} idx {} ps {:08x} vp scale {:.1f},{:.1f} off {:.1f},{:.1f} "
                     "screen {},{}-{},{} generic {},{}-{},{} vpsc {},{}-{},{} window {},{}-{},{}",
                     u32(regs.primitive_type), num_indices, u32(ps ? ps->pgm_hash : 0), vp.xscale,
                     vp.yscale, vp.xoffset, vp.yoffset, s32(ss.top_left_x), s32(ss.top_left_y),
                     s32(ss.bottom_right_x), s32(ss.bottom_right_y), u32(gs.top_left_x),
                     u32(gs.top_left_y), u32(gs.bottom_right_x), u32(gs.bottom_right_y),
                     u32(vs0.top_left_x), u32(vs0.top_left_y), u32(vs0.bottom_right_x),
                     u32(vs0.bottom_right_y), u32(ws.top_left_x), u32(ws.top_left_y),
                     u32(ws.bottom_right_x), u32(ws.bottom_right_y))
             .c_str());
    if (!ps) {
        return;
    }
    for (const auto& desc : ps->images) {
        const auto t = desc.GetSharp(*ps);
        // Up to one byte per texel (BC3), at most 16 MiB.
        const u64 bytes = std::min<u64>(u64(t.width + 1) * (t.height + 1), 16u << 20);
        Note(std::format("\n    tex {:#x} {}x{} pitch {} fmt {}/{} tile {} hash {:016x}", t.Address(),
                         t.width + 1, t.height + 1, t.pitch + 1, u32(t.data_format),
                         u32(t.num_format), u32(t.tiling_index), MemoryHash(t.Address(), bytes))
                 .c_str());
    }
}

void FrameCapture::NoteVertexStreams(std::span<const AmdGpu::Buffer> streams) {
    if (!Detail()) {
        return;
    }
    for (const auto& vb : streams) {
        const u64 hash = MemoryHash(vb.base_address, std::min<u64>(vb.GetSize(), 1u << 20));
        if (!hash) {
            continue;
        }
        const u32 stride = u32(vb.GetStride());
        const u32 count = std::min(4u, stride ? u32(vb.GetSize()) / stride : 0u);
        const auto* bytes = reinterpret_cast<const u8*>(vb.base_address);
        std::string note = std::format("\n    vb stride {} size {} hash {:016x}:", stride,
                                       u32(vb.GetSize()), hash);
        for (u32 i = 0; i < count; ++i) {
            note += " |";
            for (u32 w = 0; w + 4 <= std::min(stride, 32u); w += 4) {
                float f;
                std::memcpy(&f, bytes + i * stride + w, 4);
                note += std::format(" {:.4g}", f);
            }
        }
        Note(note.c_str());
    }
}

} // namespace Vulkan
