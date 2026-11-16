// Exercise the actual TDLib poll backend on QNX without an account or network.
#include "td/utils/port/detail/Poll.h"
#include "td/utils/port/detail/PollableFd.h"
#include "td/utils/Observer.h"
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <cstdio>

class Observer : public td::ObserverBase {
public:
    void notify() final {}
};
static double now() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1000000000.0;
}
int main() {
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) != 0) return 2;
    td::PollableFdInfo info{td::NativeFd(pair[0])};
    Observer observer;
    td::detail::Poll poll;
    poll.init();
    poll.subscribe(info.extract_pollable_fd(&observer), td::PollFlags::Read() | td::PollFlags::Write());
    poll.run(0);
    bool writable = info.sync_with_poll().can_write();
    double start = now(); poll.run(150); double idle = now() - start;
    // Clearing the read flag after draining must rearm readable notifications.
    char data = 'x'; write(pair[1], &data, 1); poll.run(150);
    bool readable = info.sync_with_poll().can_read();
    read(pair[0], &data, 1); info.clear_flags(td::PollFlags::Read());
    start = now(); poll.run(150); double drained = now() - start;
    close(pair[1]); poll.run(0);
    bool closed = info.sync_with_poll().can_close();
    start = now(); poll.run(150); double terminal = now() - start;
    // Clear must release the descriptor lock and observer for later reuse.
    poll.clear();
    poll.subscribe(info.extract_pollable_fd(&observer), td::PollFlags::Read());
    poll.unsubscribe(info.get_pollable_fd_ref());
    bool passed = writable && readable && closed && idle >= .1 && drained >= .1 && terminal >= .1;
    std::printf("writable=%d readable=%d closed=%d idle_wait_ms=%.0f drained_wait_ms=%.0f closed_wait_ms=%.0f result=%s\n",
        writable, readable, closed, idle * 1000, drained * 1000, terminal * 1000, passed ? "passed" : "failed");
    return passed ? 0 : 1;
}
