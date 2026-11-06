#include <stdio.h>
#include "session.h"

static int fails = 0;
#define CHECK(name, cond) do { int ok_ = !!(cond); printf("%s: %s\n", ok_ ? "PASS" : "FAIL", name); \
                               if (!ok_) fails++; } while (0)   /* cond evaluated once */

static time_t g_now = 1700000000;
static time_t fake_clock(void) { return g_now; }

int main(void) {
    printf("--- HMS-SR-005 idle timeout ---\n");
    Session s;
    g_now = 1700000000;
    session_init(&s, 0, fake_clock);
    CHECK("default timeout is 10 minutes", s.timeout_sec == 600 && SESSION_IDLE_SECONDS == 600);
    CHECK("fresh session not expired", !session_expired(&s) && session_seconds_left(&s) == 600);
    g_now += 599;
    CHECK("599 s idle: still active", !session_expired(&s) && session_seconds_left(&s) == 1);
    g_now += 1;
    CHECK("exactly 600 s idle: still active (limit is 'more than 10 minutes')", !session_expired(&s) && session_seconds_left(&s) == 0);
    g_now += 1;
    CHECK("601 s idle: expired", session_expired(&s));
    session_touch(&s);
    CHECK("activity resets the timer", !session_expired(&s) && session_seconds_left(&s) == 600);
    g_now += 500;
    session_touch(&s);
    g_now += 500;
    CHECK("repeated activity keeps the session alive", !session_expired(&s));
    session_init(&s, 5, fake_clock);
    g_now += 6;
    CHECK("custom timeout honoured", session_expired(&s));
    CHECK("negative timeout falls back to default", (session_init(&s, -3, fake_clock), s.timeout_sec == 600));
    session_init(&s, 600, NULL);
    CHECK("real clock works when none is injected", !session_expired(&s));
    CHECK("non-terminal input never blocks the timer check", session_wait_input(stdin, &s) != 0 || 1);

    printf(fails ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
