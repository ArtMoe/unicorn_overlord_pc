// SPDX-License-Identifier: GPL-2.0-or-later
// vw: Anime4K (bloc97, MIT) before FSR 1: its restore model, then its x2 upscale when the window
// is larger. The models' passes are generated from the mpv shaders (tools/anime4k.py).
#pragma once

#include <span>
#include <vector>

#include "common/types.h"
#include "video_core/renderer_vulkan/host_passes/anime4k_models.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/texture_cache/image.h"

namespace Vulkan::HostPasses {

class Anime4kPass {
public:
    void Create(vk::Device device, VmaAllocator allocator);

    /// Runs the preset (VwGraphics::Anime4k) on input, read as ShaderReadOnlyOptimal; returns
    /// the result in that layout and sets size to its size, or returns input when off. linear:
    /// input samples as linear light (an sRGB view).
    vk::ImageView Render(vk::CommandBuffer cmdbuf, vk::ImageView input, vk::Extent2D& size,
                         vk::Extent2D output_size, int preset, bool linear);

private:
    struct Target {
        VideoCore::UniqueImage image;
        vk::UniqueImageView view;
    };
    struct Model {
        std::span<const Anime4kShader> shaders;
        std::vector<vk::UniquePipeline> pipelines; // built on first use
        std::vector<Target> targets;               // one per pass, for input_size
        vk::Extent2D input_size{};
    };

    vk::ImageView Run(vk::CommandBuffer cmdbuf, Model& model, vk::ImageView input,
                      vk::Extent2D size, bool linear);
    void Prepare(Model& model, vk::Extent2D size);

    vk::Device device{};
    VmaAllocator allocator{};
    vk::UniqueSampler sampler{};
    vk::UniqueDescriptorSetLayout descriptor_set_layout{};
    vk::UniquePipelineLayout pipeline_layout{};
    Model restore_s{RESTORE_S}, restore_m{RESTORE_M}, upscale_s{UPSCALE_S}, upscale_m{UPSCALE_M};
};

} // namespace Vulkan::HostPasses
