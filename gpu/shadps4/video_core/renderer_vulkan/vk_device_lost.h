// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdio>
#include <cstdlib>

namespace Vulkan {
[[noreturn]] inline void DeviceLost(const char* operation) {
    std::fprintf(stderr, "GPU crash: the graphics device was lost (%s). "
                         "Attach this run's log to your bug report.\n", operation);
    std::fflush(nullptr);
    // Do not wait for a failed device or run GPU teardown from its worker thread.
    std::_Exit(23);
}
}
