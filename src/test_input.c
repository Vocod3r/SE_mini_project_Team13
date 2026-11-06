#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input.h"

static int fails = 0;
#define CHECK(name, cond) do { int ok_ = !!(cond); printf("%s: %s\n", ok_ ? "PASS" : "FAIL", name); \
                               if (!ok_) fails++; } while (0)   /* cond evaluated once */

static FILE *feed(const char *text) {
    FILE *f = tmpfile();
    fputs(text, f);
    rewind(f);
    return f;
}

int main(void) {
    printf("--- HMS-SR-004 bounded reads ---\n");
    char buf[16];
    FILE *f = feed("hello\nthis line is far too long for the buffer\nnext\r\n");
    CHECK("short line read", input_read_line(f, buf, sizeof buf) == INPUT_OK && strcmp(buf, "hello") == 0);
    CHECK("long line flagged, not overflowed", input_read_line(f, buf, sizeof buf) == INPUT_TOO_LONG && strlen(buf) == 15);
    CHECK("rest of long line discarded, next line intact", input_read_line(f, buf, sizeof buf) == INPUT_OK && strcmp(buf, "next") == 0);
    CHECK("EOF reported", input_read_line(f, buf, sizeof buf) == INPUT_EOF);
    fclose(f);
    f = feed("last line without newline");
    char big[64];
    CHECK("final line without newline is returned", input_read_line(f, big, sizeof big) == INPUT_OK && strcmp(big, "last line without newline") == 0);
    fclose(f);
    CHECK("NULL / zero-size buffers are safe", input_read_line(NULL, buf, sizeof buf) == INPUT_EOF &&
                                                 input_read_line(stdin, NULL, 0) == INPUT_EOF);

    printf("--- dates and times ---\n");
    CHECK("valid date", input_valid_date("2026-11-03"));
    CHECK("leap day accepted in leap year", input_valid_date("2028-02-29") && input_valid_date("2000-02-29"));
    CHECK("leap day refused otherwise", !input_valid_date("2027-02-29") && !input_valid_date("1900-02-29"));
    CHECK("30/31 day months", input_valid_date("2026-04-30") && !input_valid_date("2026-04-31") && input_valid_date("2026-12-31"));
    CHECK("bad dates refused", !input_valid_date("2026-13-01") && !input_valid_date("2026-00-10") &&
                               !input_valid_date("2026-11-00") && !input_valid_date("2026-1-03") &&
                               !input_valid_date("26-11-03") && !input_valid_date("2026/11/03") &&
                               !input_valid_date("") && !input_valid_date(NULL) &&
                               !input_valid_date("2026-11-03 ") && !input_valid_date("abcd-ef-gh") &&
                               !input_valid_date("2026-11-03'; DROP TABLE patients;--"));
    CHECK("valid times", input_valid_time("00:00") && input_valid_time("09:30") && input_valid_time("23:59"));
    CHECK("bad times refused", !input_valid_time("24:00") && !input_valid_time("12:60") && !input_valid_time("9:30") &&
                               !input_valid_time("0930") && !input_valid_time("") && !input_valid_time(NULL) && !input_valid_time("ab:cd"));

    printf("--- names, phones, numbers ---\n");
    CHECK("valid names", input_valid_name("Asha Rao", 40) && input_valid_name("Dr. D'Souza-Iyer", 40));
    CHECK("bad names refused", !input_valid_name("", 40) && !input_valid_name("   ", 40) && !input_valid_name("R2D2", 40) &&
                               !input_valid_name("Bob; DROP TABLE", 40) && !input_valid_name(NULL, 40) &&
                               !input_valid_name("A very very long name indeed", 10));
    CHECK("valid phones", input_valid_phone("9876543210") && input_valid_phone("+919876543210"));
    CHECK("bad phones refused", !input_valid_phone("12345") && !input_valid_phone("98765x3210") && !input_valid_phone("") &&
                                !input_valid_phone(NULL) && !input_valid_phone("+12345") && !input_valid_phone("98765432101234"));
    long v = 0;
    CHECK("int parse ok", input_parse_int("42", 1, 100, &v) && v == 42);
    CHECK("int parse refuses junk, range, overflow", !input_parse_int("4x", 1, 100, &v) && !input_parse_int("101", 1, 100, &v) &&
                                                      !input_parse_int("0", 1, 100, &v) && !input_parse_int("", 1, 100, &v) &&
                                                      !input_parse_int(" 5", 1, 100, &v) && !input_parse_int(NULL, 1, 100, &v) &&
                                                      !input_parse_int("99999999999999999999999", 1, 100, &v));

    printf("--- prompts re-ask politely ---\n");
    FILE *out = tmpfile();
    f = feed("abc\n-5\n7\n");
    CHECK("int prompt accepts the third try", input_ask_int(f, out, "N: ", 1, 10, &v) == 0 && v == 7);
    fclose(f);
    f = feed("x\ny\nz\n5\n");
    CHECK("int prompt gives up after 3 bad tries", input_ask_int(f, out, "N: ", 1, 10, &v) == -1);
    fclose(f);
    char d[16];
    f = feed("tomorrow\n2026-02-30\n2026-11-03\n");
    CHECK("date prompt accepts the third try", input_ask_date(f, out, "D: ", d, sizeof d) == 0 && strcmp(d, "2026-11-03") == 0);
    fclose(f);
    f = feed("25:00\n09:30\n");
    CHECK("time prompt accepts the second try", input_ask_time(f, out, "T: ", d, sizeof d) == 0 && strcmp(d, "09:30") == 0);
    fclose(f);
    f = feed("");
    CHECK("prompt stops at EOF", input_ask_int(f, out, "N: ", 1, 10, &v) == -1);
    fclose(f);
    rewind(out);
    char msg[512] = {0};
    size_t got = fread(msg, 1, sizeof msg - 1, out);
    msg[got] = '\0';
    CHECK("error messages tell the user what to type", strstr(msg, "YYYY-MM-DD") && strstr(msg, "whole number"));
    fclose(out);

    printf("--- random input never crashes (SR-004 fuzz check) ---\n");
    srand(12345);
    int survived = 0;
    for (int i = 0; i < 20000; i++) {
        char s[80];
        int len = rand() % 79;
        for (int k = 0; k < len; k++) s[k] = (char)(1 + rand() % 255);
        s[len] = '\0';
        long out_v;
        (void)input_valid_date(s); (void)input_valid_time(s); (void)input_valid_name(s, 40);
        (void)input_valid_phone(s); (void)input_parse_int(s, -50, 50, &out_v);
        FILE *g = tmpfile();
        fwrite(s, 1, (size_t)len, g);
        rewind(g);
        char tiny[8];
        (void)input_read_line(g, tiny, sizeof tiny);
        fclose(g);
        survived++;
    }
    CHECK("20000 random strings handled", survived == 20000);

    printf(fails ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
