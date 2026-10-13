// vw: the MemoryManager surface used by the video core and GnmDriver.
// Guest memory lives at identical host addresses; the C runtime owns the VMAs.
#pragma once
#include "common/types.h"
#include "common/singleton.h"
#include "core/address_space.h"

#include <cstring>
#include "vw_toggles.h"

namespace Vulkan { class Rasterizer; }
extern "C" const uint64_t* runtime_memory_generation(void);

namespace Core {
/// vw: the guest mapping a thread found last (MemoryManager).
struct LastMapping {
    u64 generation = 1; ///< odd: never equal to a stable table generation
    VAddr start = 0, end = 0;
};

class MemoryManager {
public:
    void SetRasterizer(Vulkan::Rasterizer* rasterizer_);
    AddressSpace& GetAddressSpace() { return impl; }
    bool IsValidGpuMapping(VAddr virtual_addr, u64 size) {
        // The PS4's GPU can only handle 40 bit addresses.
        return virtual_addr + size < 0x100'0000'0000ULL;
    }
    // vw: called for every constant buffer and vertex stream of every draw; the mapping found
    // last is remembered per thread (valid while the VMA table generation is unchanged).
    u64 ClampRangeSize(VAddr virtual_addr, u64 size) {
        return InLastMapping(virtual_addr, size) ? size : ClampRangeSizeSlow(virtual_addr, size);
    }
    void CopySparseMemory(VAddr source, u8* dest, u64 size) {
        if (size <= 64 * 1024 && InLastMapping(source, size)) {
            std::memcpy(dest, reinterpret_cast<const void*>(source), size);
            return;
        }
        CopySparseMemorySlow(source, dest, size);
    }
    bool TryWriteBacking(void* address, const void* data, u64 size);
    /// CPU wrote guest memory outside page tracking (e.g. decoded video frames).
    void InvalidateMemory(VAddr address, u64 size);
    Vulkan::Rasterizer* GetRasterizer() const { return rasterizer; }

private:
    static inline thread_local LastMapping last_mapping{};
    static inline const uint64_t* table_generation = nullptr;
    bool InLastMapping(VAddr address, u64 size) const noexcept {
        if (!table_generation || VwToggle::Disabled(VwToggle::RegionCache)) {
            return false;
        }
        const u64 generation = __atomic_load_n(table_generation, __ATOMIC_ACQUIRE);
        return last_mapping.generation == generation && address >= last_mapping.start &&
               address + size <= last_mapping.end;
    }
    static void NoteMapping(u64 generation, VAddr start, VAddr end) noexcept {
        if (!(generation & 1)) {
            last_mapping = {generation, start, end};
        }
    }
    u64 ClampRangeSizeSlow(VAddr virtual_addr, u64 size);
    void CopySparseMemorySlow(VAddr source, u8* dest, u64 size);
    friend void CopySparseSerial(VAddr source, u8* dest, u64 size);
    AddressSpace impl;
    Vulkan::Rasterizer* rasterizer{};
};
using Memory = Common::Singleton<MemoryManager>;
} // namespace Core
