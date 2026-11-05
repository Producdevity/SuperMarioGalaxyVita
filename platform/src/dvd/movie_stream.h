// Movies (.thp) are read ahead of the player on a thread of their own, into a ring. The player asks for one frame at a time, the
// next only after the one before has arrived, and the frames are of every size (14 KB at the start of PrologueA, 44 KB in busy
// scenes, 2.6 MB/s). Read by the disc worker in between frames, a slow card read held up every frame behind it: 6.5 % of the
// frames of PrologueB were not there on their turn and their sound came out as silence (console, 5 Nov).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace port_dvd {

struct MovieFile {
    int (*open)(const char* path);
    long (*read)(int fd, void* dst, size_t size, uint64_t offset);  // <= 0: the end of what can be read
    void (*close)(int fd);
    void (*threadStart)();  // on the reader thread, before its first read
};

struct MovieStats {
    unsigned hits = 0, waits = 0, reads = 0;
    uint64_t bytes = 0;
    int64_t readNs = 0, readMaxNs = 0, waitNs = 0, waitMaxNs = 0;
};

class MovieStream {
public:
    static constexpr size_t kRing = 2u << 20;
    static constexpr size_t kChunk = 128u << 10;
    static constexpr size_t kJump = 256u << 10;  // a frame this far past what was read is still waited for, not a seek
    static constexpr int kWaitMs = 1000;

    explicit MovieStream(const MovieFile& file) : mFile(file) {}

    // Starts reading `path` (which must outlive the stream) at `offset`; the first call starts the reader thread.
    void Begin(uint32_t id, const char* path, uint64_t size, uint64_t offset) {
        std::unique_lock<std::mutex> lock(mMutex);
        if (!mRing) {
            mRing = static_cast<uint8_t*>(malloc(kRing));
            if (!mRing) return;
            std::thread([this] { Reader(); }).detach();
        }
        ++mGeneration;
        mId = id;
        mPath = path;
        mSize = size;
        mStart = mEnd = offset;
        mFailed = false;
        mWake.notify_all();
    }

    // Copies [offset, offset + size) of file `id` to dst and lets the ring move past it. False: not this stream's (no copy made).
    bool Serve(uint32_t id, uint64_t offset, void* dst, size_t size) {
        std::unique_lock<std::mutex> lock(mMutex);
        if (mId != id || !mRing || mFailed || size > kRing / 2 || offset < mStart || offset > mEnd + kJump) return false;
        const uint64_t want = offset + size;
        if (want > mSize) return false;
        if (want > mEnd) {
            const auto t0 = std::chrono::steady_clock::now();
            const uint64_t generation = mGeneration;
            mStart = std::min(offset, mEnd);  // (room for the reader: what is before this frame is not wanted any more)
            mWake.notify_all();
            const bool there = mArrived.wait_for(lock, std::chrono::milliseconds(kWaitMs),
                                                 [&] { return mEnd >= want || mFailed || mGeneration != generation; });
            const int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
            ++mStats.waits;
            mStats.waitNs += ns;
            mStats.waitMaxNs = std::max(mStats.waitMaxNs, ns);
            if (!there || mFailed || mGeneration != generation) return false;
        }
        const size_t at = static_cast<size_t>(offset % kRing), first = std::min(size, kRing - at);
        memcpy(dst, mRing + at, first);
        memcpy(static_cast<uint8_t*>(dst) + first, mRing, size - first);
        mStart = want;
        ++mStats.hits;
        mWake.notify_all();
        return true;
    }

    MovieStats Stats() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mStats;
    }

private:
    void Reader() {
        if (mFile.threadStart) mFile.threadStart();
        int fd = -1;
        uint64_t open = 0;  // the generation fd belongs to
        std::unique_lock<std::mutex> lock(mMutex);
        for (;;) {
            mWake.wait(lock, [&] { return !mFailed && mId != kNone && mEnd < mSize && mEnd - mStart + 1 <= kRing - kChunk / 4; });
            const uint64_t generation = mGeneration, from = mEnd;
            const size_t at = static_cast<size_t>(from % kRing);
            size_t size = static_cast<size_t>(std::min<uint64_t>({kChunk, mSize - from, kRing - (mEnd - mStart), kRing - at}));
            const char* path = mPath;
            lock.unlock();
            if (open != generation) {
                if (fd >= 0) mFile.close(fd);
                fd = mFile.open(path);
                open = generation;
            }
            const auto t0 = std::chrono::steady_clock::now();
            const long got = fd >= 0 ? mFile.read(fd, mRing + at, size, from) : -1;
            const int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
            lock.lock();
            if (generation != mGeneration) continue;  // (a new stream began meanwhile: this read is not part of it)
            if (got <= 0) {
                mFailed = true;
            } else {
                mEnd += static_cast<uint64_t>(got);
                ++mStats.reads;
                mStats.bytes += static_cast<uint64_t>(got);
                mStats.readNs += ns;
                mStats.readMaxNs = std::max(mStats.readMaxNs, ns);
            }
            mArrived.notify_all();
        }
    }

    static constexpr uint32_t kNone = ~0u;
    MovieFile mFile;
    std::mutex mMutex;
    std::condition_variable mWake, mArrived;
    uint8_t* mRing = nullptr;
    uint32_t mId = kNone;
    const char* mPath = nullptr;
    uint64_t mSize = 0, mStart = 0, mEnd = 0, mGeneration = 0;
    bool mFailed = false;
    MovieStats mStats;
};

}  // namespace port_dvd
