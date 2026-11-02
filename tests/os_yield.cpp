// OSYieldThread's shortcut (nothing ready at the caller's priority or above) against the full SelectThread(TRUE) it skips:
// from the same scheduler state, both must leave the same current thread, run queues (contents and order), queue bits and
// reschedule hint. The real scheduler, included like tests/os_irq.cpp does.
#include "../platform/src/os/os_thread.cpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" void port_log(const char*, ...) {}
extern "C" int64_t port_host_time_ns() { return 0; }
extern "C" void port_fatal(const char*, ...) { std::abort(); }
extern "C" void* port_low_alloc(size_t) { std::abort(); }
extern "C" void __OSUnlockAllMutex(OSThread*) { std::abort(); }

static unsigned checks;
static void require(bool value, const char* message) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "yield FAILED: %s\n", message);
        std::exit(1);
    }
}

struct Snapshot {
    std::vector<std::vector<OSThread*>> queues;
    u32 bits;
    BOOL hint;
    OSThread* current;
    std::vector<int> states;
    bool operator==(const Snapshot& o) const {
        return queues == o.queues && bits == o.bits && hint == o.hint && current == o.current && states == o.states;
    }
};
static OSThread sOthers[6];
static Snapshot snap() {
    Snapshot s;
    for (auto& q : RunQueue) {
        std::vector<OSThread*> v;
        for (OSThread* t = q.head; t; t = t->link.next) v.push_back(t);
        s.queues.push_back(v);
    }
    s.bits = RunQueueBits;
    s.hint = RunQueueHint;
    s.current = __OSCurrentThread;
    s.states.push_back(DefaultThread.state);
    for (OSThread& t : sOthers) s.states.push_back(t.state);
    return s;
}
// A scheduler state: the default thread running at `priority`, the others ready at `ready` (-1: not ready).
static void setUp(int priority, std::initializer_list<int> ready, s32 reschedule, BOOL hint) {
    for (auto& q : RunQueue) OSInitThreadQueue(&q);
    RunQueueBits = 0;
    tlsSelf = &DefaultThread;
    __OSCurrentThread = &DefaultThread;
    sCpuOwner = &DefaultThread;
    DefaultThread.state = OS_THREAD_STATE_RUNNING;
    DefaultThread.priority = priority;
    DefaultThread.queue = nullptr;
    int i = 0;
    for (int p : ready) {
        OSThread& t = sOthers[i++];
        t.state = p < 0 ? 0 : OS_THREAD_STATE_READY;
        t.priority = p < 0 ? 0 : p;
        t.queue = nullptr;
        if (p >= 0) SetRun(&t);
    }
    for (; i < 6; ++i) sOthers[i].state = 0;
    Reschedule = reschedule;
    RunQueueHint = hint;
    sIntEnabled = true;
}

int main() {
    struct Case {
        int priority;
        std::initializer_list<int> ready;
        s32 reschedule;
        BOOL hint;
    };
    const Case cases[] = {
        {16, {}, 0, FALSE},           {16, {}, 0, TRUE},           {16, {17, 20, 31}, 0, TRUE},
        {16, {31, 17, 17, 25}, 0, FALSE}, {0, {5, 1, 31}, 0, TRUE}, {30, {31}, 0, TRUE},
        {16, {20}, 1, TRUE},          {16, {}, 2, FALSE},           {16, {18, -1, 19}, 0, TRUE},
    };
    unsigned shortcuts = 0;
    for (const Case& c : cases) {
        setUp(c.priority, c.ready, c.reschedule, c.hint);
        require(__cntlzw(RunQueueBits) > (u32)c.priority, "the cases are ones the shortcut takes");
        OSYieldThread();
        const Snapshot shortcut = snap();
        setUp(c.priority, c.ready, c.reschedule, c.hint);
        {
            const BOOL enabled = OSDisableInterrupts();
            SelectThread(TRUE);
            OSRestoreInterrupts(enabled);
        }
        const Snapshot full = snap();
        require(shortcut == full, "the shortcut leaves the scheduler as SelectThread(TRUE) does");
        require(full.current == &DefaultThread && DefaultThread.state == OS_THREAD_STATE_RUNNING, "the yielding thread keeps the CPU");
        ++shortcuts;
    }
    // A thread ready at the same priority, or above it, is not a shortcut case (OSYieldThread must run SelectThread).
    setUp(16, {16}, 0, TRUE);
    require(!(__cntlzw(RunQueueBits) > 16u), "an equal-priority thread takes the full path");
    setUp(16, {3}, 0, TRUE);
    require(!(__cntlzw(RunQueueBits) > 16u), "a higher-priority thread takes the full path");
    std::printf("PASS: OSYieldThread's shortcut matches SelectThread(TRUE) in %u scheduler states (%u checks)\n", shortcuts, checks);
}
