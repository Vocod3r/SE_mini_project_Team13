#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include "session.h"
#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#endif

static time_t now_of(const Session *s) { return s->clock ? s->clock() : time(NULL); }

void session_init(Session *s, int timeout_sec, time_t (*clock)(void)) {
    s->timeout_sec = timeout_sec > 0 ? timeout_sec : SESSION_IDLE_SECONDS;
    s->clock = clock;
    s->last_active = now_of(s);
}

void session_touch(Session *s) { s->last_active = now_of(s); }

int session_expired(const Session *s) {
    return (now_of(s) - s->last_active) > s->timeout_sec;
}

int session_seconds_left(const Session *s) {
    long left = s->timeout_sec - (long)(now_of(s) - s->last_active);
    return left < 0 ? 0 : (int)left;
}

int session_wait_input(FILE *in, const Session *s) {
#ifndef _WIN32
    if (!in) return -1;
    int fd = fileno(in);
    if (fd < 0) return -1;
    if (!isatty(fd)) return 1;
    int left = session_seconds_left(s);
    if (left <= 0) return 0;
    struct pollfd p = { .fd = fd, .events = POLLIN };
    int r = poll(&p, 1, left * 1000);
    if (r < 0) return -1;
    return r == 0 ? 0 : 1;
#else
    (void)s; (void)in;
    return 1;
#endif
}
