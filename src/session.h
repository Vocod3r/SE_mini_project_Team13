#ifndef SESSION_H
#define SESSION_H
#include <time.h>

/* HMS-SR-005: log a staff member out after 10 minutes without activity. */
#define SESSION_IDLE_SECONDS 600

typedef struct {
    time_t last_active;
    int    timeout_sec;
    time_t (*clock)(void);   /* injectable so tests do not have to wait 10 real minutes */
} Session;

void session_init(Session *s, int timeout_sec, time_t (*clock)(void));  /* clock NULL = real time */
void session_touch(Session *s);          /* call on every user action */
int  session_expired(const Session *s);  /* 1 once idle for longer than the timeout */
int  session_seconds_left(const Session *s);

/* Waits for input on `in`. Returns 1 when input is ready, 0 when the idle time ran out,
 * -1 on error. On a real terminal this wakes up at the deadline; for pipes and files it
 * returns 1 immediately and the expiry is checked after the line is read. */
int  session_wait_input(FILE *in, const Session *s);
#endif
