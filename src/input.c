#include "input.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int input_read_line(FILE *in, char *buf, size_t sz) {
    if (!buf || sz == 0) return INPUT_EOF;
    buf[0] = '\0';
    if (!in) return INPUT_EOF;
    size_t n = 0;
    int c, too_long = 0, got_any = 0;
    while ((c = fgetc(in)) != EOF) {
        got_any = 1;
        if (c == '\n') break;
        if (c == '\r') continue;
        if (n + 1 < sz) buf[n++] = (char)c; else too_long = 1;
    }
    buf[n] = '\0';
    if (c == EOF && !got_any) return INPUT_EOF;
    return too_long ? INPUT_TOO_LONG : INPUT_OK;
}

static int all_digits(const char *s, int count) {
    for (int i = 0; i < count; i++) if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

int input_valid_date(const char *s) {
    if (!s || strlen(s) != 10 || s[4] != '-' || s[7] != '-') return 0;
    if (!all_digits(s, 4) || !all_digits(s + 5, 2) || !all_digits(s + 8, 2)) return 0;
    int y = atoi(s), m = atoi(s + 5), d = atoi(s + 8);
    if (y < 1900 || y > 2100 || m < 1 || m > 12 || d < 1) return 0;
    static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    int max = dim[m - 1] + (m == 2 && leap);
    return d <= max;
}

int input_valid_time(const char *s) {
    if (!s || strlen(s) != 5 || s[2] != ':') return 0;
    if (!all_digits(s, 2) || !all_digits(s + 3, 2)) return 0;
    int h = atoi(s), m = atoi(s + 3);
    return h <= 23 && m <= 59;
}

int input_valid_name(const char *s, size_t maxlen) {
    if (!s) return 0;
    size_t len = strlen(s);
    if (len == 0 || len > maxlen) return 0;
    int letters = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalpha(c)) letters++;
        else if (c != ' ' && c != '.' && c != '\'' && c != '-') return 0;
    }
    return letters > 0;
}

int input_valid_phone(const char *s) {
    if (!s) return 0;
    size_t len = strlen(s);
    if (len < 10 || len > 13) return 0;
    size_t i = (s[0] == '+') ? 1 : 0;
    if (len - i < 10) return 0;
    for (; i < len; i++) if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

int input_parse_int(const char *s, long min, long max, long *out) {
    if (!s || !*s) return 0;
    /* strtol would quietly accept leading spaces and '+'; we only allow digits with an optional '-' */
    if (!(isdigit((unsigned char)s[0]) || (s[0] == '-' && isdigit((unsigned char)s[1])))) return 0;
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || end == s) return 0;
    if (v < min || v > max) return 0;
    if (out) *out = v;
    return 1;
}

/* Reads one line, retrying on over-long input; returns 0 ok, -1 EOF. */
static int ask_line(FILE *in, FILE *out, const char *prompt, char *buf, size_t sz, int *too_long) {
    fprintf(out, "%s", prompt);
    fflush(out);
    int rc = input_read_line(in, buf, sz);
    *too_long = (rc == INPUT_TOO_LONG);
    return rc == INPUT_EOF ? -1 : 0;
}

int input_ask_int(FILE *in, FILE *out, const char *prompt, long min, long max, long *val) {
    char line[32];
    for (int tries = 0; tries < 3; tries++) {
        int too_long;
        if (ask_line(in, out, prompt, line, sizeof line, &too_long) != 0) return -1;
        if (!too_long && input_parse_int(line, min, max, val)) return 0;
        fprintf(out, "  Please enter a whole number from %ld to %ld.\n", min, max);
    }
    return -1;
}

int input_ask_date(FILE *in, FILE *out, const char *prompt, char *buf, size_t sz) {
    for (int tries = 0; tries < 3; tries++) {
        int too_long;
        if (ask_line(in, out, prompt, buf, sz, &too_long) != 0) return -1;
        if (!too_long && input_valid_date(buf)) return 0;
        fprintf(out, "  Please enter a real date as YYYY-MM-DD, for example 2026-11-03.\n");
    }
    return -1;
}

int input_ask_time(FILE *in, FILE *out, const char *prompt, char *buf, size_t sz) {
    for (int tries = 0; tries < 3; tries++) {
        int too_long;
        if (ask_line(in, out, prompt, buf, sz, &too_long) != 0) return -1;
        if (!too_long && input_valid_time(buf)) return 0;
        fprintf(out, "  Please enter a time as HH:MM, for example 09:30.\n");
    }
    return -1;
}
