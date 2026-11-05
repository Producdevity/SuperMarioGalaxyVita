// The movie reader (platform/src/dvd/movie_stream.h) against a file in memory: every frame arrives byte for byte, at any size, across
// the ring's wrap, with a slow card, past a seek, and a failing card hands the frame back to the plain path.
#include "../platform/src/dvd/movie_stream.h"

#include <assert.h>
#include <stdio.h>

#include <atomic>
#include <random>
#include <vector>

using port_dvd::MovieFile;
using port_dvd::MovieStream;

static std::vector<uint8_t> gFile;
static std::atomic<int> gDelayUs{0}, gFailAfter{-1}, gOpens{0}, gCloses{0};
static std::atomic<int> gStarted{0};

static int fileOpen(const char*) { ++gOpens; return 7; }
static long fileRead(int fd, void* dst, size_t size, uint64_t offset) {
    assert(fd == 7);
    if (gDelayUs.load()) std::this_thread::sleep_for(std::chrono::microseconds(gDelayUs.load()));
    if (gFailAfter.load() == 0) return -1;
    if (gFailAfter.load() > 0) --gFailAfter;
    if (offset >= gFile.size()) return 0;
    const size_t n = std::min(size, gFile.size() - static_cast<size_t>(offset));
    memcpy(dst, gFile.data() + offset, n);
    return static_cast<long>(n);
}
static void fileClose(int) { ++gCloses; }
static void threadStart() { ++gStarted; }

static bool same(uint64_t offset, const std::vector<uint8_t>& got) { return memcmp(gFile.data() + offset, got.data(), got.size()) == 0; }

int main() {
    std::mt19937 rng(1);
    gFile.resize(24u << 20);
    for (auto& b : gFile) b = static_cast<uint8_t>(rng());
    MovieStream s(MovieFile{fileOpen, fileRead, fileClose, threadStart});
    std::vector<uint8_t> frame(MovieStream::kRing / 2);

    // not begun: nothing is served
    assert(!s.Serve(3, 0, frame.data(), 100));

    // the whole file in frames of the movies' sizes, the wrap crossed many times
    s.Begin(3, "a", gFile.size(), 1000);
    uint64_t at = 1000;
    unsigned frames = 0;
    while (at < gFile.size()) {
        const size_t size = std::min<size_t>(14000 + rng() % 38000, gFile.size() - at);
        frame.resize(size);
        assert(s.Serve(3, at, frame.data(), size));
        assert(same(at, frame));
        at += size;
        ++frames;
    }
    assert(gStarted.load() == 1);
    std::printf("  %u frames byte for byte, %u reads of %.0f KiB\n", frames, s.Stats().reads, s.Stats().bytes / 1024.0 / s.Stats().reads);

    // a slow card: the player waits, nothing is lost
    gDelayUs = 3000;
    s.Begin(3, "a", gFile.size(), 0);
    at = 0;
    for (int i = 0; i < 400; i++) {
        frame.resize(40000);
        assert(s.Serve(3, at, frame.data(), 40000));
        assert(same(at, frame));
        at += 40000;
    }
    gDelayUs = 0;

    // what the ring moved past, another file, a jump far ahead: not served, the plain path reads it
    frame.resize(1000);
    assert(!s.Serve(3, 0, frame.data(), 1000));
    assert(!s.Serve(4, at, frame.data(), 1000));
    assert(!s.Serve(3, at + (64u << 20), frame.data(), 1000));
    assert(!s.Serve(3, gFile.size() - 10, frame.data(), 1000));  // past the end of the file

    // a small step past the player is still the same stream (a frame skipped)
    frame.resize(1000);
    assert(s.Serve(3, at + 5000, frame.data(), 1000));
    assert(same(at + 5000, frame));

    // a new stream while the old one is being read: only the new one's bytes come out
    gDelayUs = 2000;
    for (int i = 0; i < 40; i++) {
        const uint64_t from = rng() % (gFile.size() / 2);
        s.Begin(3 + (i & 1), "b", gFile.size(), from);
        frame.resize(30000);
        assert(s.Serve(3 + (i & 1), from, frame.data(), 30000));
        assert(same(from, frame));
    }
    gDelayUs = 0;
    assert(gOpens.load() > 2 && gCloses.load() == gOpens.load() - 1);

    // a failing card: the waiting frame goes back to the plain path, and the next stream works
    s.Begin(3, "a", gFile.size(), 0);
    gFailAfter = 2;
    at = 0;
    bool failed = false;
    for (int i = 0; i < 2000 && !failed; i++) {
        frame.resize(30000);
        if (s.Serve(3, at, frame.data(), 30000)) {
            assert(same(at, frame));
            at += 30000;
        } else {
            failed = true;
        }
    }
    assert(failed);
    gFailAfter = -1;
    s.Begin(3, "a", gFile.size(), at);
    frame.resize(30000);
    assert(s.Serve(3, at, frame.data(), 30000));
    assert(same(at, frame));

    // the end of the file: read to the last byte, then asleep
    s.Begin(3, "a", gFile.size(), gFile.size() - 100000);
    frame.resize(100000);
    assert(s.Serve(3, gFile.size() - 100000, frame.data(), 100000));
    assert(same(gFile.size() - 100000, frame));

    std::puts("PASS: movie reader");
    return 0;
}
