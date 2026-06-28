/*
 * uac2_bridge.c — NW-A105 USB DAC Bridge Daemon
 *
 * 功能: 从 UAC2 Gadget 的 capture PCM 读取音频数据,
 *       桥接到 cxd3778gf 的 playback PCM 输出.
 *
 * 编译: aarch64-linux-gnu-gcc -static -o uac2_bridge uac2_bridge.c
 *         -I./tinyalsa/include ./tinyalsa/src/pcm.c ./tinyalsa/src/mixer.c -lpthread
 * 部署: adb push uac2_bridge /vendor/bin/
 *
 * 用法:
 *   uac2_bridge start [-d CXD_DEVICE] [-v]
 *   uac2_bridge stop
 *   uac2_bridge status
 *
 * 采样率/位深/声道: 从 /proc/asound 自动检测 UAC2 与 PC 协商后的格式,
 *                   无需手动指定.
 *
 * Copyright 2026 — 基于 GPL v2 发布
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>

#include <tinyalsa/asoundlib.h>

/* ================================================================
 * 配置常量
 * ================================================================ */

/* UAC2 gadget 产生的 ALSA 音频采集口 (通常是 card#2) */
#define UAC2_CARD_DEFAULT       2
#define UAC2_DEVICE_DEFAULT     0

/* cxd3778gf 播放 PCM 接口 (card#1) */
#define CXD_CARD_DEFAULT        1
/* device 0: hires-out (Hi-Res), device 2: hires-out-low-power */
#define CXD_DEVICE_DEFAULT      0

/* PCM 缓冲参数 */
#define PERIOD_SIZE_DEFAULT     1024
#define PERIOD_COUNT_DEFAULT    4

/* ================================================================
 * 全局状态
 * ================================================================ */

static volatile sig_atomic_t g_stop = 0;
static struct pcm *g_pcm_in  = NULL;
static struct pcm *g_pcm_out = NULL;

/* ================================================================
 * 信号处理
 * ================================================================ */

static void handle_signal(int sig) { (void)sig; g_stop = 1; }

static void setup_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* ================================================================
 * 自动检测 UAC2 已协商的 PCM 格式
 *
 * 读取 /proc/asound/card<N>/pcm0c/sub0/hw_params，在 PC 连接并
 * 开始串流后该文件会包含实际协商好的 rate/channels/format。
 * 若读取失败（PC 尚未连接或内核不导出），返回 false，调用方
 * 应继续使用默认值并稍后重试。
 * ================================================================ */

typedef struct {
    unsigned int    rate;
    unsigned int    channels;
    enum pcm_format format;
    bool            valid;
} detected_params_t;

static enum pcm_format parse_format(const char *s)
{
    if (strstr(s, "S32_LE")) return PCM_FORMAT_S32_LE;
    if (strstr(s, "S24_LE")) return PCM_FORMAT_S24_LE;
    if (strstr(s, "S24_3LE")) return PCM_FORMAT_S24_3LE;
    if (strstr(s, "S16_LE")) return PCM_FORMAT_S16_LE;
    return PCM_FORMAT_S16_LE; /* 默认 */
}

static detected_params_t detect_uac2_params(unsigned int uac2_card)
{
    detected_params_t p = { .rate = 44100, .channels = 2,
                             .format = PCM_FORMAT_S16_LE, .valid = false };
    char path[128];
    snprintf(path, sizeof(path),
             "/proc/asound/card%u/pcm0c/sub0/hw_params", uac2_card);

    FILE *f = fopen(path, "r");
    if (!f) return p;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned int v;
        char token[64];
        if (sscanf(line, "rate: %u", &v) == 1) {
            p.rate = v; p.valid = true;
        } else if (sscanf(line, "channels: %u", &v) == 1) {
            p.channels = v;
        } else if (sscanf(line, "format: %63s", token) == 1) {
            p.format = parse_format(token);
        }
    }
    fclose(f);

    /* "closed" 状态时文件内容不含 rate 行，valid 保持 false */
    return p;
}

/* ================================================================
 * 配置结构
 * ================================================================ */

struct bridge_config {
    unsigned int    card_in;
    unsigned int    device_in;
    unsigned int    card_out;
    unsigned int    device_out;
    unsigned int    period_size;
    unsigned int    period_count;
    bool            verbose;
    /* 运行时填充（自动检测） */
    unsigned int    rate;
    unsigned int    channels;
    enum pcm_format format;
};

static struct bridge_config default_config(void)
{
    struct bridge_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.card_in     = UAC2_CARD_DEFAULT;
    cfg.device_in   = UAC2_DEVICE_DEFAULT;
    cfg.card_out    = CXD_CARD_DEFAULT;
    cfg.device_out  = CXD_DEVICE_DEFAULT;
    cfg.period_size = PERIOD_SIZE_DEFAULT;
    cfg.period_count = PERIOD_COUNT_DEFAULT;
    cfg.verbose     = false;
    /* rate/channels/format 由 detect_uac2_params() 在运行时填充 */
    cfg.rate        = 44100;
    cfg.channels    = 2;
    cfg.format      = PCM_FORMAT_S16_LE;
    return cfg;
}

static struct pcm_config make_pcm_config(const struct bridge_config *cfg)
{
    struct pcm_config pcm_cfg;
    memset(&pcm_cfg, 0, sizeof(pcm_cfg));
    pcm_cfg.channels     = cfg->channels;
    pcm_cfg.rate         = cfg->rate;
    pcm_cfg.period_size  = cfg->period_size;
    pcm_cfg.period_count = cfg->period_count;
    pcm_cfg.format       = cfg->format;
    pcm_cfg.start_threshold  = 0;
    pcm_cfg.stop_threshold   = 0;
    pcm_cfg.silence_threshold = 0;
    return pcm_cfg;
}

/* ================================================================
 * 核心桥接循环
 * ================================================================ */

static int run_bridge(struct bridge_config *cfg)
{
    /* ── 1. 等待 PC 连接并协商格式 ─────────────────────────────── */
    printf("Waiting for UAC2 format negotiation (card%u)...\n", cfg->card_in);
    int wait_tries = 0;
    while (!g_stop) {
        detected_params_t dp = detect_uac2_params(cfg->card_in);
        if (dp.valid) {
            cfg->rate     = dp.rate;
            cfg->channels = dp.channels;
            cfg->format   = dp.format;
            printf("Detected: %u Hz, %u ch, %u bit\n",
                   cfg->rate, cfg->channels,
                   pcm_format_to_bits(cfg->format));
            break;
        }
        if (++wait_tries % 10 == 0)
            printf("  [%ds] still waiting for PC connection...\n", wait_tries);
        sleep(1);
    }
    if (g_stop) return 0;

    /* ── 2. 打开 PCM 设备 ────────────────────────────────────────── */
    struct pcm_config pcm_cfg = make_pcm_config(cfg);
    unsigned int frame_size   = (pcm_format_to_bits(pcm_cfg.format) / 8) * pcm_cfg.channels;
    unsigned int buf_frames   = cfg->period_size;
    unsigned int buf_bytes    = buf_frames * frame_size;

    char *buffer = malloc(buf_bytes);
    if (!buffer) {
        fprintf(stderr, "OOM: cannot allocate %u bytes\n", buf_bytes);
        return -ENOMEM;
    }

    if (cfg->verbose)
        printf("Opening UAC2 capture: card=%u dev=%u %uHz %uch\n",
               cfg->card_in, cfg->device_in, cfg->rate, cfg->channels);

    g_pcm_in = pcm_open(cfg->card_in, cfg->device_in, PCM_IN, &pcm_cfg);
    if (!g_pcm_in || !pcm_is_ready(g_pcm_in)) {
        fprintf(stderr, "Cannot open UAC2 capture: %s\n",
                g_pcm_in ? pcm_get_error(g_pcm_in) : "null");
        free(buffer);
        return -ENODEV;
    }

    if (cfg->verbose)
        printf("Opening CXD playback: card=%u dev=%u\n",
               cfg->card_out, cfg->device_out);

    g_pcm_out = pcm_open(cfg->card_out, cfg->device_out, PCM_OUT, &pcm_cfg);
    if (!g_pcm_out || !pcm_is_ready(g_pcm_out)) {
        fprintf(stderr, "Cannot open CXD playback: %s\n",
                g_pcm_out ? pcm_get_error(g_pcm_out) : "null");
        pcm_close(g_pcm_in); g_pcm_in = NULL;
        free(buffer);
        return -ENODEV;
    }

    printf("Bridge running: %u Hz, %u ch, %u bit, period=%u, dev_out=%u\n",
           cfg->rate, cfg->channels,
           pcm_format_to_bits(cfg->format), cfg->period_size,
           cfg->device_out);

    /* ── 3. 桥接循环 ─────────────────────────────────────────────── */
    unsigned int periods = 0;
    while (!g_stop) {
        int n = pcm_readi(g_pcm_in, buffer, buf_frames);
        if (n < 0) {
            if (n == -EPIPE) { pcm_prepare(g_pcm_in); continue; }
            fprintf(stderr, "Read error: %d\n", n);
            break;
        }
        int w = pcm_writei(g_pcm_out, buffer, buf_frames);
        if (w < 0) {
            if (w == -EPIPE) { pcm_prepare(g_pcm_out); continue; }
            fprintf(stderr, "Write error: %d\n", w);
            break;
        }
        if (cfg->verbose && ++periods % 1000 == 0)
            printf("  [%u periods]\n", periods);
    }

    printf("Bridge stopped (%u periods).\n", periods);

    pcm_close(g_pcm_out); g_pcm_out = NULL;
    pcm_close(g_pcm_in);  g_pcm_in  = NULL;
    free(buffer);
    return 0;
}

/* ================================================================
 * 停止
 * ================================================================ */

static int stop_bridge(void)
{
    FILE *pf = fopen("/data/local/tmp/uac2_bridge.pid", "r");
    if (!pf) { fprintf(stderr, "Not running (no pidfile)\n"); return -1; }
    pid_t pid;
    int ok = (fscanf(pf, "%d", &pid) == 1);
    fclose(pf);
    if (!ok) { fprintf(stderr, "Invalid pidfile\n"); return -1; }
    unlink("/data/local/tmp/uac2_bridge.pid");
    if (kill(pid, SIGTERM) == 0) { printf("Sent SIGTERM to %d\n", pid); return 0; }
    fprintf(stderr, "kill(%d): %s\n", pid, strerror(errno)); return -1;
}

/* ================================================================
 * 状态
 * ================================================================ */

static int print_status(unsigned int uac2_card)
{
    printf("=== USB DAC Bridge Status ===\n");
    detected_params_t dp = detect_uac2_params(uac2_card);
    if (dp.valid)
        printf("UAC2 format: %u Hz, %u ch, %u bit\n",
               dp.rate, dp.channels, pcm_format_to_bits(dp.format));
    else
        printf("UAC2 format: not negotiated (PC not streaming)\n");

    FILE *pf = fopen("/data/local/tmp/uac2_bridge.pid", "r");
    if (pf) {
        pid_t pid; int ok = (fscanf(pf, "%d", &pid) == 1); fclose(pf);
        if (ok && kill(pid, 0) == 0) printf("Running: YES (pid %d)\n", pid);
        else                          printf("Running: NO (stale pidfile)\n");
    } else {
        printf("Running: NO\n");
    }
    return 0;
}

/* ================================================================
 * 主入口
 * ================================================================ */

static void print_usage(const char *p)
{
    fprintf(stderr,
        "Usage: %s <command> [options]\n"
        "\nCommands:\n"
        "  start    启动桥接 (自动检测 PC 协商格式)\n"
        "  stop     停止桥接\n"
        "  status   查看状态\n"
        "\nOptions (start):\n"
        "  -d DEV   CXD3778GF 输出 device (0=hires-out [默认], 2=low-power)\n"
        "  -v       详细输出\n"
        "  -h       帮助\n"
        "\n示例:\n"
        "  %s start          # Hi-Res 输出, 自动检测格式\n"
        "  %s start -d 2     # 低功耗输出\n"
        "  %s stop\n"
        "  %s status\n",
        p, p, p, p, p);
}

int main(int argc, char **argv)
{
    struct bridge_config cfg = default_config();

    if (argc < 2) { print_usage(argv[0]); return EXIT_FAILURE; }
    const char *cmd = argv[1];

    optind = 2;
    int opt;
    while ((opt = getopt(argc, argv, "d:vh")) != -1) {
        switch (opt) {
        case 'd':
            cfg.device_out = (unsigned int)atoi(optarg);
            break;
        case 'v':
            cfg.verbose = true;
            break;
        case 'h':
            print_usage(argv[0]); return EXIT_SUCCESS;
        default:
            print_usage(argv[0]); return EXIT_FAILURE;
        }
    }

    setup_signal_handlers();

    if (strcmp(cmd, "start") == 0) {
        /* 写 pidfile 后立即进入检测/桥接循环 */
        FILE *pf = fopen("/data/local/tmp/uac2_bridge.pid", "w");
        if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }
        int ret = run_bridge(&cfg);
        unlink("/data/local/tmp/uac2_bridge.pid");
        return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    } else if (strcmp(cmd, "stop") == 0) {
        return stop_bridge() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    } else if (strcmp(cmd, "status") == 0) {
        return print_status(cfg.card_in);

    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]); return EXIT_FAILURE;
    }
}
