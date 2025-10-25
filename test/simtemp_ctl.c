// File: simtemp_ctl.c
// Purpose: Minimal, maintainable user-space helper to validate sysfs + char device
// Build:   gcc -O2 -Wall -Wextra -std=c11 -o simtemp_ctl simtemp_ctl.c
// Usage:   See 'print_usage()' or run without args.
// Notes:
//  - Paths are overridable via env vars: SIMTEMP_SYSFS, SIMTEMP_DEV
//  - Handles partial reads from /dev/simtemp
//  - Provides poll/epoll checks (POLLIN/POLLPRI)
//  - All commentary in English per requirement.

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static const char *default_sysfs = "/sys/class/misc/simtemp";
static const char *default_dev   = "/dev/simtemp";

static const char *sysfs_base(void) {
    const char *p = getenv("SIMTEMP_SYSFS");
    return p && *p ? p : default_sysfs;
}
static const char *dev_path(void) {
    const char *p = getenv("SIMTEMP_DEV");
    return p && *p ? p : default_dev;
}

static void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); va_end(ap);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s read-attr <name>\n"
        "  %s write-attr <name> <value>\n"
        "  %s read-dev [bytes] [count]\n"
        "  %s poll [timeout_ms]\n"
        "  %s epoll [timeout_ms]\n"
        "  %s rate [duration_s]\n"
        "  %s mean [samples]\n"
        "  %s wait-threshold [timeout_ms]\n"
        "\n"
        "Env:\n"
        "  SIMTEMP_SYSFS=/sys/class/simtemp/simtemp0/device (override)\n"
        "  SIMTEMP_DEV=/dev/simtemp (override)\n",
        prog, prog, prog, prog, prog, prog, prog, prog
    );
}

static void sysfs_path(char *out, size_t outsz, const char *name) {
    snprintf(out, outsz, "%s/%s", sysfs_base(), name);
}

static int write_file_str(const char *path, const char *s) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -errno;
    ssize_t n = write(fd, s, strlen(s));
    int saved = (n < 0) ? -errno : 0;
    close(fd);
    return saved ? saved : 0;
}

static int read_file_str(const char *path, char *buf, size_t bufsz) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -errno;
    ssize_t n = read(fd, buf, bufsz - 1);
    int saved = (n < 0) ? -errno : 0;
    if (n >= 0) { buf[n] = '\0'; }
    close(fd);
    return saved ? saved : (int)n;
}

static uint64_t now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

static int cmd_read_attr(int argc, char **argv) {
    if (argc < 1) die("read-attr requires <name>");
    char path[512], buf[512];
    sysfs_path(path, sizeof(path), argv[0]);
    int rc = read_file_str(path, buf, sizeof(buf));
    if (rc < 0) die("read-attr failed: %s -> %s", path, strerror(-rc));
    printf("%s", buf);
    return 0;
}

static int cmd_write_attr(int argc, char **argv) {
    if (argc < 2) die("write-attr requires <name> <value>");
    char path[512];
    sysfs_path(path, sizeof(path), argv[0]);
    int rc = write_file_str(path, argv[1]);
    if (rc < 0) {
        // Return errno text (useful for T4)
        fprintf(stderr, "write-attr error (%s): %s\n", path, strerror(-rc));
        return 2; // non-zero so scripts can detect failure
    }
    return 0;
}

static ssize_t robust_read(int fd, void *buf, size_t count, int timeout_ms) {
    // Reads up to 'count' bytes, allowing partials, with optional blocking poll.
    // Returns number of bytes read (>=0) or -1 on error / -2 on timeout.
    struct pollfd p = {.fd = fd, .events = POLLIN | POLLPRI};
    int pret = poll(&p, 1, timeout_ms);
    if (pret == 0) return -2; // timeout
    if (pret < 0) return -1;

    ssize_t n = read(fd, buf, count);
    return n; // may be < count (partial read)
}

static void hex_dump(const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        printf("%02x%s", b[i], ((i+1)%16==0) ? "\n" : " ");
    }
    if (n % 16) puts("");
}

static int cmd_read_dev(int argc, char **argv) {
    size_t bytes = 32; // unknown ABI: read a chunk and show content & size
    size_t count = 1;
    if (argc >= 1) bytes = (size_t)strtoul(argv[0], NULL, 10);
    if (argc >= 2) count = (size_t)strtoul(argv[1], NULL, 10);

    int fd = open(dev_path(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) die("open(%s): %s", dev_path(), strerror(errno));

    uint8_t *buf = malloc(bytes);
    if (!buf) die("malloc");

    for (size_t i = 0; i < count; i++) {
        ssize_t n = robust_read(fd, buf, bytes, 3000);
        if (n == -2) {
            fprintf(stderr, "read-dev timeout\n"); free(buf); close(fd); return 3;
        } else if (n < 0) {
            fprintf(stderr, "read-dev error: %s\n", strerror(errno)); free(buf); close(fd); return 4;
        }
        printf("READ[%zu]: %zd bytes\n", i, n);
        hex_dump(buf, (size_t)n);
        fflush(stdout);
    }
    free(buf); close(fd);
    return 0;
}

static const char *event_mask_to_str(short revents, char *out, size_t n) {
    // Build a human-readable mask string
    out[0] = '\0';
    if (revents & POLLIN)  strncat(out, "POLLIN ", n-1);
    if (revents & POLLPRI) strncat(out, "POLLPRI ", n-1);
    if (revents & POLLERR) strncat(out, "POLLERR ", n-1);
    if (revents & POLLHUP) strncat(out, "POLLHUP ", n-1);
    if (revents & POLLNVAL)strncat(out, "POLLNVAL ", n-1);
    return out;
}

static int cmd_poll(int argc, char **argv) {
    int timeout = (argc >= 1) ? atoi(argv[0]) : 5000;
    int fd = open(dev_path(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) die("open(%s): %s", dev_path(), strerror(errno));

    struct pollfd p = {.fd = fd, .events = POLLIN | POLLPRI};
    int rc = poll(&p, 1, timeout);
    if (rc == 0) { printf("TIMEOUT\n"); close(fd); return 1; }
    if (rc < 0)  { perror("poll"); close(fd); return 2; }

    char m[128]; printf("EVENTS: %s\n", event_mask_to_str(p.revents, m, sizeof m));
    close(fd);
    return 0;
}

static int cmd_epoll(int argc, char **argv) {
    int timeout = (argc >= 1) ? atoi(argv[0]) : 5000;
    int fd = open(dev_path(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) die("open(%s): %s", dev_path(), strerror(errno));

    int ep = epoll_create1(EPOLL_CLOEXEC);
    if (ep < 0) die("epoll_create1: %s", strerror(errno));

    struct epoll_event ev = { .events = EPOLLIN | EPOLLPRI, .data.fd = fd };
    if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev) < 0) die("epoll_ctl: %s", strerror(errno));

    struct epoll_event out;
    int rc = epoll_wait(ep, &out, 1, timeout);
    if (rc == 0) { printf("TIMEOUT\n"); close(ep); close(fd); return 1; }
    if (rc < 0)  { perror("epoll_wait"); close(ep); close(fd); return 2; }

    char m[128]; short re = 0;
    if (out.events & EPOLLIN)  re |= POLLIN;
    if (out.events & EPOLLPRI) re |= POLLPRI;
    printf("EVENTS: %s\n", event_mask_to_str(re, m, sizeof m));

    close(ep); close(fd);
    return 0;
}

// Measure ~samples/sec for duration
static int cmd_rate(int argc, char **argv) {
    int duration_s = (argc >= 1) ? atoi(argv[0]) : 2;
    int fd = open(dev_path(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) die("open(%s): %s", dev_path(), strerror(errno));

    uint8_t buf[64];
    uint64_t end_ms = now_ms() + (uint64_t)duration_s * 1000ULL;
    int count = 0;

    while (now_ms() < end_ms) {
        ssize_t n = robust_read(fd, buf, sizeof(buf), 1500);
        if (n >= 0) count++;
    }
    printf("COUNT=%d DURATION_S=%d RATE=%.2f samples/sec\n",
           count, duration_s, (double)count / (double)duration_s);
    close(fd);
    return 0;
}

// Compute a simple mean from N samples (assuming first 4 bytes might be time & next 4 temp in mC is unknown ABI)
// We do not assume a specific struct; we try to parse signed 32-bit at offset 4 (temp mC).
static int cmd_mean(int argc, char **argv) {
    int samples = (argc >= 1) ? atoi(argv[0]) : 16;
    int fd = open(dev_path(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) die("open(%s): %s", dev_path(), strerror(errno));

    int64_t sum = 0; int got = 0;
    for (int i = 0; i < samples; i++) {
        uint8_t b[16];
        ssize_t n = robust_read(fd, b, sizeof(b), 3000);
        if (n < 8) continue; // need at least 8 bytes to read a s32 at offset 4
        int32_t temp_mc;
        memcpy(&temp_mc, &b[4], sizeof(temp_mc)); // assume little-endian host & uapi
        sum += temp_mc; got++;
    }
    if (got == 0) { fprintf(stderr, "mean: no samples\n"); close(fd); return 2; }
    printf("MEAN_mC=%lld N=%d\n", (long long)(sum/got), got);
    close(fd);
    return 0;
}

static int cmd_wait_threshold(int argc, char **argv) {
    int timeout = (argc >= 1) ? atoi(argv[0]) : 3000;
    // Expect POLLPRI within timeout
    int fd = open(dev_path(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) die("open(%s): %s", dev_path(), strerror(errno));

    struct pollfd p = {.fd = fd, .events = POLLIN | POLLPRI};
    int rc = poll(&p, 1, timeout);
    if (rc == 0) { printf("TIMEOUT\n"); close(fd); return 1; }
    if (rc < 0)  { perror("poll"); close(fd); return 2; }

    bool has_pri = (p.revents & POLLPRI);
    char m[128]; printf("EVENTS: %s\n", event_mask_to_str(p.revents, m, sizeof m));
    close(fd);
    return has_pri ? 0 : 3;
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(argv[0]); return 1; }
    const char *cmd = argv[1];
    if      (!strcmp(cmd, "read-attr"))       return cmd_read_attr(argc-2, &argv[2]);
    else if (!strcmp(cmd, "write-attr"))      return cmd_write_attr(argc-2, &argv[2]);
    else if (!strcmp(cmd, "read-dev"))        return cmd_read_dev(argc-2, &argv[2]);
    else if (!strcmp(cmd, "poll"))            return cmd_poll(argc-2, &argv[2]);
    else if (!strcmp(cmd, "epoll"))           return cmd_epoll(argc-2, &argv[2]);
    else if (!strcmp(cmd, "rate"))            return cmd_rate(argc-2, &argv[2]);
    else if (!strcmp(cmd, "mean"))            return cmd_mean(argc-2, &argv[2]);
    else if (!strcmp(cmd, "wait-threshold"))  return cmd_wait_threshold(argc-2, &argv[2]);
    else { print_usage(argv[0]); return 1; }
}
