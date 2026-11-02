// platform/src/os/os_mem.cpp's address lookups against a plain scan of the region table (port_memory_region), for
// addresses at and around every region's edges (base, uncached alias) and random ones: port_is_native_range,
// port_native_window, port_host_to_phys, port_is_mem1/mem2, port_is_game_memory (tools/test_os_region_lookup.sh).
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "port/port.h"

extern "C" int port_memory_region(unsigned index, void** base, void** alias, uint32_t* physical, uint32_t* size, int* kind);
extern "C" int port_native_window(const void* p, void** base, size_t* bytes);
extern "C" void port_log(const char*, ...) {}
extern "C" void port_fatal(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
    std::abort();
}
extern "C" void port_vgx_notify_write(const void*, size_t) {}

struct Region {
    uintptr_t base, alias;
    uint32_t physical, size;
    int kind;
};
static std::vector<Region> table() {
    std::vector<Region> regions;
    void *base, *alias;
    uint32_t physical, size;
    int kind;
    for (unsigned i = 0; port_memory_region(i, &base, &alias, &physical, &size, &kind); ++i)
        regions.push_back({(uintptr_t)base, (uintptr_t)alias, physical, size, kind});
    return regions;
}

// The lookup as it was: the first entry, in table order, whose base or alias window holds [p, p + bytes).
static const Region* scan(const std::vector<Region>& regions, uintptr_t p, size_t bytes, uintptr_t* start) {
    if (!p) return nullptr;
    auto in = [&](uintptr_t b, uint32_t n) { return p >= b && p - b <= n && bytes <= n - (p - b); };
    for (const Region& r : regions) {
        if (in(r.base, r.size)) { *start = r.base; return &r; }
        if (r.alias && in(r.alias, r.size)) { *start = r.alias; return &r; }
    }
    return nullptr;
}

static unsigned checked;
static void check(const std::vector<Region>& regions, uintptr_t p, size_t bytes) {
    uintptr_t start = 0;
    const Region* r = scan(regions, p, bytes, &start);
    const void* q = (const void*)p;
    assert(port_is_native_range(q, bytes) == (r != nullptr));
    void* window = nullptr;
    size_t windowBytes = 0;
    assert(port_native_window(q, &window, &windowBytes) == (scan(regions, p, 1, &start) != nullptr));
    if (const Region* one = scan(regions, p, 1, &start)) {
        assert((uintptr_t)window == start && windowBytes == one->size);
        assert(port_host_to_phys(q) == one->physical + (uint32_t)(p - start));
        assert(port_is_mem1(q) == (one->kind == 0) && port_is_mem2(q) == (one->kind == 1));
        assert(port_is_game_memory(q) == (one->kind == 0 || one->kind == 1));
    } else {
        assert(!port_is_mem1(q) && !port_is_mem2(q) && !port_is_game_memory(q));
    }
    ++checked;
}

int main() {
    assert(port_mem_init());
    // more entries after MEM1 and MEM2, as the game's thread stacks add them
    for (int i = 0; i < 6; ++i) assert(port_low_alloc(0x3000 + 0x1000 * i));
    const std::vector<Region> regions = table();
    assert(regions.size() >= 9 && regions[0].kind == 0 && regions[1].kind == 1);
    for (const Region& r : regions)
        for (uintptr_t window : {r.base, r.alias}) {
            if (!window) continue;
            for (uintptr_t edge : {window, window + r.size})
                for (int d = -40; d <= 40; ++d)
                    for (size_t bytes : {size_t(0), size_t(1), size_t(4), size_t(32), size_t(r.size), size_t(r.size) + 1})
                        check(regions, edge + d, bytes);
        }
    uint64_t state = 0x9e3779b97f4a7c15ull;
    for (int i = 0; i < 200000; ++i) {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        const Region& r = regions[(state >> 33) % regions.size()];
        const uintptr_t p = (state >> 40) & 1 ? (uintptr_t)(state >> 16) % 0x100000000ull : r.base + (state >> 20) % (r.size + 64);
        check(regions, p, (state >> 8) % 64);
    }
    check(regions, 0, 1);
    std::printf("PASS: address lookups match a plain scan of %zu regions (%u checks)\n", regions.size(), checked);
}
