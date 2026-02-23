#include "nm/shell.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/fs.h"

static void out_append(char *out, size_t cap, size_t *used, const char *s)
{
    if (out == 0 || cap == 0 || s == 0) {
        return;
    }
    while (*s != '\0' && *used + 1 < cap) {
        out[*used] = *s;
        (*used)++;
        s++;
    }
    out[*used] = '\0';
}

static int str_eq(const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

static size_t str_len(const char *s)
{
    size_t n = 0;
    if (s == 0) {
        return 0;
    }
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static int split_tokens(const char *line, char tokens[][64], int max_tokens)
{
    int count = 0;
    int i = 0;
    while (line[i] != '\0') {
        while (line[i] == ' ' || line[i] == '\t') {
            i++;
        }
        if (line[i] == '\0') {
            break;
        }
        if (count >= max_tokens) {
            return -1;
        }
        int j = 0;
        while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t' && j + 1 < 64) {
            tokens[count][j++] = line[i++];
        }
        tokens[count][j] = '\0';
        while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t') {
            i++;
        }
        count++;
    }
    return count;
}

static int cmd_echo(int argc, char argv[][64], char *out, size_t out_cap, const char *in)
{
    (void)in;
    size_t used = 0;
    for (int i = 1; i < argc; i++) {
        out_append(out, out_cap, &used, argv[i]);
        if (i + 1 < argc) {
            out_append(out, out_cap, &used, " ");
        }
    }
    out_append(out, out_cap, &used, "\n");
    return 0;
}

static int cmd_cat(int argc, char argv[][64], char *out, size_t out_cap, const char *in)
{
    if (argc == 1) {
        size_t used = 0;
        out_append(out, out_cap, &used, in ? in : "");
        return 0;
    }

    int fd = fs_open(argv[1], NM_O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }

    size_t used = 0;
    char buf[128];
    int64_t n;
    while ((n = fs_read(fd, buf, sizeof(buf))) > 0) {
        for (int64_t i = 0; i < n && used + 1 < out_cap; i++) {
            out[used++] = buf[i];
        }
    }
    if (used < out_cap) {
        out[used] = '\0';
    }
    (void)fs_close(fd);
    return 0;
}

static int cmd_ls(int argc, char argv[][64], char *out, size_t out_cap, const char *in)
{
    (void)argc;
    (void)argv;
    (void)in;
    const char *candidates[] = {"/hello.txt", "/motd", "/tmp.log", "/ext2-file", 0};
    size_t used = 0;
    for (int i = 0; candidates[i] != 0; i++) {
        struct nm_stat st;
        if (fs_stat(candidates[i], &st) == 0) {
            out_append(out, out_cap, &used, candidates[i]);
            out_append(out, out_cap, &used, "\n");
        }
    }
    return 0;
}

static int run_simple(const char *line, char *out, size_t out_cap, const char *in)
{
    char argv[16][64];
    int argc = split_tokens(line, argv, 16);
    if (argc <= 0) {
        if (out && out_cap) {
            out[0] = '\0';
        }
        return 0;
    }

    if (str_eq(argv[0], "echo")) {
        return cmd_echo(argc, argv, out, out_cap, in);
    }
    if (str_eq(argv[0], "cat")) {
        return cmd_cat(argc, argv, out, out_cap, in);
    }
    if (str_eq(argv[0], "ls")) {
        return cmd_ls(argc, argv, out, out_cap, in);
    }
    return -1;
}

void shell_init(void)
{
}

int shell_execute_line(const char *line, char *out, size_t out_cap)
{
    if (line == 0 || out == 0 || out_cap == 0) {
        return -1;
    }
    out[0] = '\0';

    char left[128];
    char right[128];
    int li = 0;
    int ri = 0;
    int mode = 0;
    for (size_t i = 0; line[i] != '\0'; i++) {
        if (line[i] == '|' && mode == 0) {
            mode = 1;
            continue;
        }
        if (line[i] == '>' && mode == 0) {
            mode = 2;
            continue;
        }

        if (mode == 0 && li + 1 < (int)sizeof(left)) {
            left[li++] = line[i];
        } else if (mode != 0 && ri + 1 < (int)sizeof(right)) {
            right[ri++] = line[i];
        }
    }
    left[li] = '\0';
    right[ri] = '\0';

    if (mode == 0) {
        return run_simple(left, out, out_cap, 0);
    }

    if (mode == 1) {
        char tmp[512];
        if (run_simple(left, tmp, sizeof(tmp), 0) != 0) {
            return -1;
        }
        return run_simple(right, out, out_cap, tmp);
    }

    char tmp[512];
    if (run_simple(left, tmp, sizeof(tmp), 0) != 0) {
        return -1;
    }

    char toks[8][64];
    int c = split_tokens(right, toks, 8);
    if (c != 1) {
        return -1;
    }

    int fd = fs_open(toks[0], NM_O_CREAT | NM_O_RDWR, 0644);
    if (fd < 0) {
        return -1;
    }
    (void)fs_write(fd, tmp, (uint64_t)str_len(tmp));
    (void)fs_close(fd);
    return 0;
}

int shell_run_script(const char *script, char *out, size_t out_cap)
{
    if (script == 0 || out == 0 || out_cap == 0) {
        return -1;
    }
    out[0] = '\0';

    char line[256];
    int li = 0;
    size_t used = 0;

    for (size_t i = 0;; i++) {
        char ch = script[i];
        if (ch != '\n' && ch != '\0' && li + 1 < (int)sizeof(line)) {
            line[li++] = ch;
            continue;
        }

        line[li] = '\0';
        if (li > 0) {
            char one[512];
            if (shell_execute_line(line, one, sizeof(one)) == 0 && one[0] != '\0') {
                out_append(out, out_cap, &used, one);
            }
        }
        li = 0;

        if (ch == '\0') {
            break;
        }
    }
    return 0;
}
