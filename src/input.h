#ifndef INPUT_H
#define INPUT_H
#include <stdio.h>
#include <stddef.h>

/* HMS-SR-004: every value typed by a user goes through these before it reaches the data layer.
 * No gets/scanf/strcpy anywhere: reads are bounded and validators never trust length. */

#define INPUT_OK        0
#define INPUT_EOF      -1
#define INPUT_TOO_LONG -2   /* line did not fit; the rest was discarded, buf holds the truncated start */

int input_read_line(FILE *in, char *buf, size_t sz);   /* strips the newline, always NUL-terminates */

int input_valid_date(const char *s);      /* YYYY-MM-DD, real calendar date (leap years handled) */
int input_valid_time(const char *s);      /* HH:MM, 00:00-23:59 */
int input_valid_name(const char *s, size_t maxlen);   /* letters, space . ' - ; at least one letter */
int input_valid_phone(const char *s);     /* 10 to 13 chars, digits with an optional leading + */
int input_parse_int(const char *s, long min, long max, long *out);  /* whole string must be a number */

/* Prompt helpers: print the prompt, re-ask up to 3 times with a clear message.
 * Return 0 on success, -1 on EOF or after 3 bad tries. */
int input_ask_int(FILE *in, FILE *out, const char *prompt, long min, long max, long *val);
int input_ask_date(FILE *in, FILE *out, const char *prompt, char *buf, size_t sz);
int input_ask_time(FILE *in, FILE *out, const char *prompt, char *buf, size_t sz);
#endif
