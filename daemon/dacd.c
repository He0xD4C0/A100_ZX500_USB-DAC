/*
 * dacd.c — NW-A105 USB DAC 原生守护进程
 *
 * 单一进程取代 usb_dac_ctl.sh + uac2_bridge.c：
 *   - 原生 C 操作 configfs（零 shell）
 *   - 集成 PCM 桥接循环（fork 子进程）
 *   - Unix Domain Socket IPC（Activity 通过 socket 发指令）
 *   - 严格状态机，防止不一致操作
 *   - 子进程崩溃自动检测
 *
 * 编译（NDK）:
 *   $NDK/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk \
 *                   APP_ABI=arm64-v8a APP_PLATFORM=android-28
 *
 * 部署:
 *   adb push dacd /vendor/bin/
 *   adb push init.usb_dac.rc /vendor/etc/init/
 *   reboot
 *
 * 协议（文本行，换行分隔）:
 *   → ENABLE [keep_adb=0|1] [low_power=0|1]
 *   ← OK RUNNING rate=44100 ch=2 fmt=S16_LE
 *   → DISABLE
 *   ← OK OFF
 *   → STATUS
 *   ← OK RUNNING rate=44100 ch=2 fmt=S16_LE uptime=3600
 *   ← ERROR <message>
 *
 * Copyright 2026 — GPL v3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/select.h>

#include <tinyalsa/asoundlib.h>

/* ================================================================
 * 常量
 * ================================================================ */

#define DACD_SOCKET_PATH        "/data/local/tmp/dacd.sock"
#define DACD_PIDFILE            "/data/local/tmp/dacd.pid"
#define GADGET_CONFIG           "/config/usb_gadget/g1"
#define UAC2_FUNC_NAME          "uac2.gs0"
#define UAC2_LINK_NAME          "f2"
#define ADB_FUNC_PATH           "ffs.adb"
#define ADB_LINK_NAME           "f1"
#define UDC_PATH                "/config/usb_gadget/g1/UDC"
#define UDC_NAME                "ci_hdrc.0"

#define UAC2_CARD_DEFAULT       2
#define UAC2_DEVICE_DEFAULT     0
#define CXD_CARD_DEFAULT        1
#define CXD_DEVICE_HIRES        0
#define CXD_DEVICE_LOWPOWER     2

#define PERIOD_SIZE             1024
#define PERIOD_COUNT            4

#define MAX_CLIENTS             8
#define MAX_CMD_LEN             256
#define MAX_RESP_LEN            512
#define EPOLL_MAX_EVENTS        16

/* ================================================================
 * 状态机
 * ================================================================ */

typedef enum {
    STATE_OFF,
    STATE_STARTING,
    STATE_RUNNING,
    STATE_STOPPING,
    STATE_ERROR
} dacd_state_t;

static const char *state_str(dacd_state_t s) {
    switch (s) {
    case STATE_OFF:      return "OFF";
    case STATE_STARTING: return "STARTING";
    case STATE_RUNNING:  return "RUNNING";
    case STATE_STOPPING: return "STOPPING";
    case STATE_ERROR:    return "ERROR";
    default:             return "UNKNOWN";
    }
}

/* ================================================================
 * 全局状态
 * ================================================================ */

static volatile sig_atomic_t g_running  = 1;
static dacd_state_t          g_state   = STATE_OFF;
static pid_t                 g_bridge_pid = -1;
static int                   g_sock_fd = -1;
static int                   g_epoll_fd = -1;

/* 运行时参数（由 ENABLE 指令设置） */
static bool  g_keep_adb   = true;
static bool  g_low_power  = false;
static unsigned int g_cxd_device = CXD_DEVICE_HIRES;

/* 协商后的音频参数 */
static unsigned int    g_audio_rate     = 0;
static unsigned int    g_audio_channels = 0;
static enum pcm_format g_audio_format   = PCM_FORMAT_S16_LE;
static time_t          g_start_time     = 0;

/* 上一个错误消息 */
static char g_last_error[256];

/* ================================================================
 * 日志
 * ================================================================ */

#define LOG_TAG "dacd"
#define log_info(fmt, ...)  fprintf(stdout, "[" LOG_TAG "] " fmt "\n", ##__VA_ARGS__)
#define log_error(fmt, ...) fprintf(stderr, "[" LOG_TAG "] ERROR: " fmt "\n", ##__VA_ARGS__)

/* ================================================================
 * 文件系统辅助函数
 * ================================================================ */

/** 写字符串到 sysfs/configfs 文件，带错误检查 */
static bool write_str(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "Cannot open %s: %s", path, strerror(errno));
        log_error("%s", g_last_error);
        return false;
    }
    ssize_t len = (ssize_t)strlen(value);
    ssize_t n   = write(fd, value, len);
    close(fd);
    if (n != len) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "Write to %s failed: %s", path, strerror(errno));
        log_error("%s", g_last_error);
        return false;
    }
    return true;
}

/** 读取 sysfs/configfs 文件内容到 buffer */
static bool read_str(const char *path, char *buf, size_t len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;
    ssize_t n = read(fd, buf, len - 1);
    close(fd);
    if (n < 0) return false;
    if (n > 0 && buf[n-1] == '\n') n--;
    buf[n] = '\0';
    return true;
}

/** 路径是否指向存在的符号链接 */
static bool is_link(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
}

/** 路径是否为目录 */
static bool is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* ================================================================
 * configfs / USB Gadget 操作
 * ================================================================ */

/**
 * 创建 UAC2 函数实例。
 * 声明最大能力（192kHz/24bit/立体声），实际格式由 USB 主机协商。
 */
static bool uac2_create(void) {
    char path[256];

    snprintf(path, sizeof(path), "%s/functions/%s",
             GADGET_CONFIG, UAC2_FUNC_NAME);

    if (is_dir(path)) {
        log_info("UAC2 function already exists, reusing");
        return true;
    }

    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "mkdir %s: %s", path, strerror(errno));
        log_error("%s", g_last_error);
        return false;
    }

    /* 立体声 */
    snprintf(path, sizeof(path), "%s/functions/%s/p_chmask",
             GADGET_CONFIG, UAC2_FUNC_NAME);
    if (!write_str(path, "3")) return false;

    snprintf(path, sizeof(path), "%s/functions/%s/c_chmask",
             GADGET_CONFIG, UAC2_FUNC_NAME);
    if (!write_str(path, "3")) return false;

    /* 最高采样率 */
    snprintf(path, sizeof(path), "%s/functions/%s/p_srate",
             GADGET_CONFIG, UAC2_FUNC_NAME);
    if (!write_str(path, "192000")) return false;

    snprintf(path, sizeof(path), "%s/functions/%s/c_srate",
             GADGET_CONFIG, UAC2_FUNC_NAME);
    if (!write_str(path, "192000")) return false;

    /* 最高位深 */
    snprintf(path, sizeof(path), "%s/functions/%s/p_ssize",
             GADGET_CONFIG, UAC2_FUNC_NAME);
    if (!write_str(path, "24")) return false;

    snprintf(path, sizeof(path), "%s/functions/%s/c_ssize",
             GADGET_CONFIG, UAC2_FUNC_NAME);
    if (!write_str(path, "24")) return false;

    log_info("UAC2 function created");
    return true;
}

/** 删除 UAC2 函数实例 */
static void uac2_remove(void) {
    char path[256];
    snprintf(path, sizeof(path), "%s/functions/%s",
             GADGET_CONFIG, UAC2_FUNC_NAME);
    if (is_dir(path)) {
        rmdir(path);
        log_info("UAC2 function removed");
    }
}

/** 绑定 UAC2 到 USB 配置 */
static bool uac2_bind(void) {
    char link_path[256], target[256];
    snprintf(link_path, sizeof(link_path), "%s/configs/b.1/%s",
             GADGET_CONFIG, UAC2_LINK_NAME);
    snprintf(target, sizeof(target), "%s/functions/%s",
             GADGET_CONFIG, UAC2_FUNC_NAME);

    if (is_link(link_path)) {
        log_info("UAC2 already bound");
        return true;
    }

    if (symlink(target, link_path) != 0 && errno != EEXIST) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "symlink %s: %s", link_path, strerror(errno));
        log_error("%s", g_last_error);
        return false;
    }
    log_info("UAC2 bound to config");
    return true;
}

/** 解绑 UAC2 */
static void uac2_unbind(void) {
    char link_path[256];
    snprintf(link_path, sizeof(link_path), "%s/configs/b.1/%s",
             GADGET_CONFIG, UAC2_LINK_NAME);
    if (is_link(link_path)) {
        unlink(link_path);
        log_info("UAC2 unbound from config");
    }
}

/** 移除 ADB 函数链接 */
static void adb_unbind(void) {
    char link_path[256];
    snprintf(link_path, sizeof(link_path), "%s/configs/b.1/%s",
             GADGET_CONFIG, ADB_LINK_NAME);
    if (is_link(link_path)) {
        unlink(link_path);
        log_info("ADB unbound");
    }
}

/** 恢复 ADB 函数链接 */
static void adb_restore(void) {
    char link_path[256], target[256];
    snprintf(link_path, sizeof(link_path), "%s/configs/b.1/%s",
             GADGET_CONFIG, ADB_LINK_NAME);
    snprintf(target, sizeof(target), "%s/functions/%s",
             GADGET_CONFIG, ADB_FUNC_PATH);

    if (is_link(link_path)) {
        log_info("ADB already present");
        return;
    }

    if (symlink(target, link_path) != 0 && errno != EEXIST) {
        log_error("Cannot restore ADB: %s", strerror(errno));
    } else {
        log_info("ADB restored");
    }
}

/** 触发 USB 重新枚举（先 unbind 再 bind） */
static bool usb_reenumerate(void) {
    if (!write_str(UDC_PATH, ""))    /* unbind */
        return false;
    usleep(500000);                  /* 500ms */
    if (!write_str(UDC_PATH, UDC_NAME)) /* bind */
        return false;
    log_info("USB re-enumerated");
    return true;
}

/* ================================================================
 * ALSA / PCM 音频检测
 * ================================================================ */

static enum pcm_format parse_format(const char *s) {
    if (strstr(s, "S32_LE"))  return PCM_FORMAT_S32_LE;
    if (strstr(s, "S24_LE"))  return PCM_FORMAT_S24_LE;
    if (strstr(s, "S24_3LE")) return PCM_FORMAT_S24_3LE;
    if (strstr(s, "S16_LE"))  return PCM_FORMAT_S16_LE;
    return PCM_FORMAT_S16_LE;
}

/**
 * 从 /proc/asound/card<N>/pcm0c/sub0/hw_params 读取协商后的格式。
 * 仅在 PC 连接并启动串流后有效。
 */
static bool detect_uac2_params(unsigned int uac2_card,
                               unsigned int *rate,
                               unsigned int *channels,
                               enum pcm_format *format) {
    char path[128];
    snprintf(path, sizeof(path),
             "/proc/asound/card%u/pcm0c/sub0/hw_params", uac2_card);

    FILE *f = fopen(path, "r");
    if (!f) return false;

    bool valid = false;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned int v; char token[64];
        if (sscanf(line, "rate: %u", &v) == 1) {
            *rate = v; valid = true;
        } else if (sscanf(line, "channels: %u", &v) == 1) {
            *channels = v;
        } else if (sscanf(line, "format: %63s", token) == 1) {
            *format = parse_format(token);
        }
    }
    fclose(f);
    return valid;
}

/* ================================================================
 * Bridge 子进程 — PCM 桥接循环
 *
 * 在 fork 的子进程中运行，与主进程通过信号通信：
 *   主 → 子: SIGTERM 停止
 *   子 → 主: SIGCHLD 退出通知
 * ================================================================ */

static int bridge_loop(unsigned int card_in,   unsigned int device_in,
                       unsigned int card_out,  unsigned int device_out) {
    /* ── 等待 PC 连接并协商格式 ──────────────────────────────────── */
    log_info("[bridge] Waiting for UAC2 format negotiation on card%u...", card_in);

    unsigned int rate = 44100, channels = 2;
    enum pcm_format format = PCM_FORMAT_S16_LE;
    int wait_sec = 0;

    while (g_running) {
        if (detect_uac2_params(card_in, &rate, &channels, &format))
            break;
        if (++wait_sec % 10 == 0)
            log_info("[bridge] Still waiting... (%ds)", wait_sec);
        sleep(1);
    }
    if (!g_running) return 0;

    log_info("[bridge] Negotiated: %uHz %uch %ubit",
             rate, channels, (unsigned int)pcm_format_to_bits(format));

    /* ── 配置 PCM ────────────────────────────────────────────────── */
    struct pcm_config pcm_cfg;
    memset(&pcm_cfg, 0, sizeof(pcm_cfg));
    pcm_cfg.channels       = channels;
    pcm_cfg.rate           = rate;
    pcm_cfg.period_size    = PERIOD_SIZE;
    pcm_cfg.period_count   = PERIOD_COUNT;
    pcm_cfg.format         = format;
    pcm_cfg.start_threshold  = 0;
    pcm_cfg.stop_threshold   = 0;
    pcm_cfg.silence_threshold = 0;

    unsigned int frame_size = (pcm_format_to_bits(format) / 8) * channels;
    unsigned int buf_frames = PERIOD_SIZE;
    unsigned int buf_bytes  = buf_frames * frame_size;

    char *buffer = malloc(buf_bytes);
    if (!buffer) {
        log_error("[bridge] OOM");
        return -ENOMEM;
    }

    /* ── 打开 PCM 设备 ───────────────────────────────────────────── */
    struct pcm *pcm_in = pcm_open(card_in, device_in, PCM_IN, &pcm_cfg);
    if (!pcm_in || !pcm_is_ready(pcm_in)) {
        log_error("[bridge] Cannot open UAC2 capture: %s",
                  pcm_in ? pcm_get_error(pcm_in) : "null");
        free(buffer);
        return -ENODEV;
    }

    struct pcm *pcm_out = pcm_open(card_out, device_out, PCM_OUT, &pcm_cfg);
    if (!pcm_out || !pcm_is_ready(pcm_out)) {
        log_error("[bridge] Cannot open CXD playback: %s",
                  pcm_out ? pcm_get_error(pcm_out) : "null");
        pcm_close(pcm_in);
        free(buffer);
        return -ENODEV;
    }

    log_info("[bridge] Running: card%u→card%u dev%u, %uHz %uch %ubit",
             card_in, card_out, device_out, rate, channels,
             (unsigned int)pcm_format_to_bits(format));

    /* ── 桥接主循环 ──────────────────────────────────────────────── */
    unsigned long periods = 0;
    while (g_running) {
        int n = pcm_readi(pcm_in, buffer, buf_frames);
        if (n < 0) {
            if (n == -EPIPE) { pcm_prepare(pcm_in); continue; }
            log_error("[bridge] Read error: %d (%s)", n, strerror(-n));
            break;
        }
        if (n == 0) continue;

        int w = pcm_writei(pcm_out, buffer, (unsigned int)n);
        if (w < 0) {
            if (w == -EPIPE) { pcm_prepare(pcm_out); continue; }
            log_error("[bridge] Write error: %d (%s)", w, strerror(-w));
            break;
        }
        periods++;
    }

    log_info("[bridge] Stopped after %lu periods", periods);

    pcm_close(pcm_out);
    pcm_close(pcm_in);
    free(buffer);
    return 0;
}

/** 启动 bridge（fork 子进程） */
static bool bridge_start(void) {
    if (g_bridge_pid > 0) {
        log_error("Bridge already running (pid %d)", g_bridge_pid);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "fork failed: %s", strerror(errno));
        log_error("%s", g_last_error);
        return false;
    }

    if (pid == 0) {
        /* ── 子进程：运行桥接循环 ──────────────────────────────── */
        signal(SIGTERM, SIG_DFL);  /* 恢复默认，让 SIGTERM 杀掉进程 */
        signal(SIGINT,  SIG_DFL);

        int ret = bridge_loop(UAC2_CARD_DEFAULT, UAC2_DEVICE_DEFAULT,
                              CXD_CARD_DEFAULT, g_cxd_device);
        _exit(ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    /* ── 父进程 ──────────────────────────────────────────────────── */
    g_bridge_pid = pid;
    log_info("Bridge child started (pid %d)", pid);
    return true;
}

/** 停止 bridge（发 SIGTERM，等待退出） */
static void bridge_stop(void) {
    if (g_bridge_pid <= 0) return;

    log_info("Stopping bridge (pid %d)...", g_bridge_pid);
    kill(g_bridge_pid, SIGTERM);

    /* 等待最多 3 秒 */
    for (int i = 0; i < 30; i++) {
        int status; pid_t r = waitpid(g_bridge_pid, &status, WNOHANG);
        if (r == g_bridge_pid) {
            log_info("Bridge exited with status %d",
                     WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            g_bridge_pid = -1;
            return;
        }
        usleep(100000);
    }

    /* 强制杀掉 */
    log_info("Bridge didn't exit, sending SIGKILL");
    kill(g_bridge_pid, SIGKILL);
    waitpid(g_bridge_pid, NULL, 0);
    g_bridge_pid = -1;
}

/** 检查 bridge 是否存活 */
static bool bridge_alive(void) {
    if (g_bridge_pid <= 0) return false;
    return kill(g_bridge_pid, 0) == 0;
}

/* ================================================================
 * 顶层操作：启用 / 禁用
 * ================================================================ */

static bool dac_enable(void) {
    if (g_state == STATE_RUNNING) {
        snprintf(g_last_error, sizeof(g_last_error), "Already running");
        return false;
    }
    if (g_state == STATE_STARTING) {
        snprintf(g_last_error, sizeof(g_last_error), "Start in progress");
        return false;
    }

    g_state = STATE_STARTING;
    log_info("Enabling USB DAC (keep_adb=%d, low_power=%d)",
             g_keep_adb, g_low_power);

    /* 1. 创建 UAC2 函数 */
    if (!uac2_create()) { g_state = STATE_ERROR; return false; }

    /* 2. 绑定 UAC2 */
    if (!uac2_bind())  { g_state = STATE_ERROR; return false; }

    /* 3. 如果不保留 ADB，移除 ADB */
    if (!g_keep_adb) {
        adb_unbind();
    }

    /* 4. 触发 USB 枚举 */
    if (!usb_reenumerate()) { g_state = STATE_ERROR; return false; }

    /* 5. 等待 UAC2 声卡出现 */
    log_info("Waiting for UAC2 ALSA card...");
    sleep(2);

    /* 6. 启动 bridge 子进程 */
    g_cxd_device = g_low_power ? CXD_DEVICE_LOWPOWER : CXD_DEVICE_HIRES;
    if (!bridge_start()) {
        uac2_unbind();
        if (!g_keep_adb) adb_restore();
        usb_reenumerate();
        g_state = STATE_ERROR;
        return false;
    }

    /* 7. bridge 子进程内部会等待 PC 协商格式，先标记为 RUNNING */
    g_state = STATE_RUNNING;
    g_start_time = time(NULL);
    log_info("USB DAC mode ACTIVE (bridge pid %d)", g_bridge_pid);
    return true;
}

static bool dac_disable(void) {
    if (g_state == STATE_OFF) {
        snprintf(g_last_error, sizeof(g_last_error), "Already off");
        return false;
    }

    g_state = STATE_STOPPING;
    log_info("Disabling USB DAC...");

    /* 1. 停止 bridge */
    bridge_stop();

    /* 2. 解绑 UAC2 */
    uac2_unbind();

    /* 3. 清理 UAC2 函数 */
    uac2_remove();

    /* 4. 恢复 ADB（如果之前移除了） */
    if (!g_keep_adb) {
        adb_restore();
    }

    /* 5. 重新枚举 */
    usb_reenumerate();

    g_state       = STATE_OFF;
    g_start_time  = 0;
    g_audio_rate  = 0;
    log_info("USB DAC mode DISABLED");
    return true;
}

/* ================================================================
 * 状态查询（自动刷新运行时参数）
 * ================================================================ */

static void refresh_audio_params(void) {
    detect_uac2_params(UAC2_CARD_DEFAULT,
                       &g_audio_rate, &g_audio_channels, &g_audio_format);
}

static void build_status_response(char *buf, size_t len) {
    refresh_audio_params();

    char params[128] = "";
    if (g_audio_rate > 0) {
        snprintf(params, sizeof(params),
                 " rate=%u ch=%u fmt=%s",
                 g_audio_rate, g_audio_channels,
                 pcm_format_to_bits(g_audio_format) == 32 ? "S32_LE" :
                 pcm_format_to_bits(g_audio_format) == 24 ? "S24_LE" : "S16_LE");
    }

    long uptime = (g_start_time > 0) ? (long)(time(NULL) - g_start_time) : 0;
    bool alive  = bridge_alive();

    if (g_state == STATE_RUNNING && !alive) {
        /* bridge 意外退出 */
        g_state = STATE_ERROR;
        snprintf(g_last_error, sizeof(g_last_error),
                 "Bridge process died unexpectedly");
    }

    snprintf(buf, len, "OK %s bridge=%s%s uptime=%ld\n",
             state_str(g_state),
             alive ? "alive" : "dead",
             params,
             uptime);
}

/* ================================================================
 * 协议解析（文本行协议）
 * ================================================================ */

static void handle_command(int client_fd, const char *cmd) {
    char resp[MAX_RESP_LEN];

    /* 去除尾随空白 */
    char cmd_buf[MAX_CMD_LEN];
    strncpy(cmd_buf, cmd, sizeof(cmd_buf) - 1);
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';
    size_t clen = strlen(cmd_buf);
    while (clen > 0 && (cmd_buf[clen-1] == '\n' || cmd_buf[clen-1] == '\r'))
        cmd_buf[--clen] = '\0';

    log_info("CMD: '%s'", cmd_buf);

    /* ── ENABLE [keep_adb=0|1] [low_power=0|1] ──────────────────── */
    if (strncmp(cmd_buf, "ENABLE", 6) == 0) {
        /* 解析可选参数 */
        g_keep_adb  = (strstr(cmd_buf, "keep_adb=0") == NULL); /* 默认 true */
        g_low_power = (strstr(cmd_buf, "low_power=1") != NULL);

        if (dac_enable()) {
            build_status_response(resp, sizeof(resp));
        } else {
            snprintf(resp, sizeof(resp), "ERROR %s\n", g_last_error);
        }
        goto send_resp;
    }

    /* ── DISABLE ────────────────────────────────────────────────── */
    if (strncmp(cmd_buf, "DISABLE", 7) == 0) {
        if (dac_disable()) {
            build_status_response(resp, sizeof(resp));
        } else {
            snprintf(resp, sizeof(resp), "ERROR %s\n", g_last_error);
        }
        goto send_resp;
    }

    /* ── STATUS ─────────────────────────────────────────────────── */
    if (strncmp(cmd_buf, "STATUS", 6) == 0) {
        build_status_response(resp, sizeof(resp));
        goto send_resp;
    }

    /* ── PING ───────────────────────────────────────────────────── */
    if (strncmp(cmd_buf, "PING", 4) == 0) {
        snprintf(resp, sizeof(resp), "OK PONG\n");
        goto send_resp;
    }

    /* ── UNKNOWN ────────────────────────────────────────────────── */
    snprintf(resp, sizeof(resp), "ERROR Unknown command: %s\n", cmd_buf);

send_resp:
    ssize_t rlen = (ssize_t)strlen(resp);
    ssize_t sent = send(client_fd, resp, (size_t)rlen, MSG_NOSIGNAL);
    if (sent < 0) {
        log_error("send failed: %s", strerror(errno));
    }
}

/* ================================================================
 * SOCKET 服务器
 * ================================================================ */

/** 一行协议：以 \n 分隔，最大 MAX_CMD_LEN 字节 */
typedef struct {
    int  fd;
    char buf[MAX_CMD_LEN];
    int  pos;
} client_t;

static client_t g_clients[MAX_CLIENTS];

static void client_init(void) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        g_clients[i].fd = -1;
}

static int client_add(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd < 0) {
            g_clients[i].fd  = fd;
            g_clients[i].pos = 0;
            memset(g_clients[i].buf, 0, sizeof(g_clients[i].buf));
            return i;
        }
    }
    return -1;
}

static void client_remove(int idx) {
    if (idx < 0 || idx >= MAX_CLIENTS) return;
    close(g_clients[idx].fd);
    g_clients[idx].fd = -1;
}

/** 处理客户端数据：按换行符拆分命令 */
static void client_data(int idx) {
    client_t *c = &g_clients[idx];
    char tmp[512];
    ssize_t n = recv(c->fd, tmp, sizeof(tmp), 0);

    if (n <= 0) {
        client_remove(idx);
        return;
    }

    for (ssize_t i = 0; i < n; i++) {
        if (c->pos >= (int)(MAX_CMD_LEN - 1)) {
            /* 行过长，丢弃缓冲区 */
            c->pos = 0;
            continue;
        }
        if (tmp[i] == '\n') {
            c->buf[c->pos] = '\0';
            if (c->pos > 0)
                handle_command(c->fd, c->buf);
            c->pos = 0;
        } else {
            c->buf[c->pos++] = tmp[i];
        }
    }
}

static bool socket_create(void) {
    struct sockaddr_un addr;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        log_error("socket: %s", strerror(errno));
        return false;
    }

    /* 清除旧 socket 文件 */
    unlink(DACD_SOCKET_PATH);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DACD_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("bind %s: %s", DACD_SOCKET_PATH, strerror(errno));
        close(fd);
        return false;
    }

    /* 开放权限，允许 Activity 连接 */
    chmod(DACD_SOCKET_PATH, 0666);

    if (listen(fd, 5) < 0) {
        log_error("listen: %s", strerror(errno));
        close(fd);
        return false;
    }

    g_sock_fd = fd;
    log_info("Listening on %s", DACD_SOCKET_PATH);
    return true;
}

/* ================================================================
 * 信号处理
 * ================================================================ */

static void signal_handler(int sig) {
    switch (sig) {
    case SIGTERM:
    case SIGINT:
        log_info("Received signal %d, shutting down", sig);
        g_running = 0;
        break;
    case SIGCHLD:
        /* 收割 bridge 子进程 */
        while (1) {
            int status; pid_t pid = waitpid(-1, &status, WNOHANG);
            if (pid <= 0) break;
            if (pid == g_bridge_pid) {
                log_info("Bridge child %d exited", pid);
                g_bridge_pid = -1;
            }
        }
        break;
    }
}

static void setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}

/* ================================================================
 * 守护进程化
 * ================================================================ */

static void daemonize(void) {
    pid_t pid = fork();
    if (pid < 0)  { perror("fork"); exit(EXIT_FAILURE); }
    if (pid > 0)  { _exit(EXIT_SUCCESS); }  /* 父进程退出 */

    setsid();
    umask(0);

    /* 重定向标准流到 /dev/null */
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > 2) close(devnull);
    }

    /* 写 PID 文件 */
    FILE *pf = fopen(DACD_PIDFILE, "w");
    if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }
}

/* ================================================================
 * epoll 主循环
 * ================================================================ */

static int run_event_loop(void) {
    g_epoll_fd = epoll_create1(0);
    if (g_epoll_fd < 0) {
        log_error("epoll_create1: %s", strerror(errno));
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events  = EPOLLIN;
    ev.data.fd = g_sock_fd;
    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, g_sock_fd, &ev) < 0) {
        log_error("epoll_ctl for socket: %s", strerror(errno));
        return -1;
    }

    struct epoll_event events[EPOLL_MAX_EVENTS];
    log_info("Entering event loop");

    while (g_running) {
        int nfds = epoll_wait(g_epoll_fd, events, EPOLL_MAX_EVENTS, 2000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            log_error("epoll_wait: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == g_sock_fd) {
                /* 新连接 */
                int cfd = accept(g_sock_fd, NULL, NULL);
                if (cfd >= 0) {
                    int idx = client_add(cfd);
                    if (idx >= 0) {
                        memset(&ev, 0, sizeof(ev));
                        ev.events  = EPOLLIN;
                        ev.data.fd = cfd;
                        if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, cfd, &ev) < 0) {
                            client_remove(idx);
                        }
                    } else {
                        close(cfd); /* too many clients */
                    }
                }
            } else {
                /* 客户端数据 */
                int idx = -1;
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (g_clients[j].fd == fd) { idx = j; break; }
                }
                if (idx >= 0) client_data(idx);
            }
        }
    }

    return 0;
}

/* ================================================================
 * 清理
 * ================================================================ */

static void cleanup(void) {
    log_info("Cleaning up...");

    /* 停止 bridge */
    if (g_bridge_pid > 0) {
        bridge_stop();
    }

    /* 如果当前在运行，关闭 DAC */
    if (g_state == STATE_RUNNING || g_state == STATE_STARTING) {
        dac_disable();
    }

    /* 关闭所有客户端 */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd >= 0) {
            close(g_clients[i].fd);
            g_clients[i].fd = -1;
        }
    }

    if (g_sock_fd >= 0)  { close(g_sock_fd); g_sock_fd = -1; }
    if (g_epoll_fd >= 0) { close(g_epoll_fd); g_epoll_fd = -1; }

    unlink(DACD_SOCKET_PATH);
    unlink(DACD_PIDFILE);

    log_info("Goodbye.");
}

/* ================================================================
 * 主入口
 * ================================================================ */

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "NW-A105 USB DAC 原生守护进程。\n"
        "监听 %s，接受 Activity 的指令。\n"
        "\n"
        "Options:\n"
        "  -f        前台运行（不 daemonize，日志输出到 stderr）\n"
        "  -h        帮助\n",
        prog, DACD_SOCKET_PATH);
}

int main(int argc, char **argv) {
    bool foreground = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) foreground = true;
        else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]); return EXIT_SUCCESS;
        }
    }

    /* 守护进程化 */
    if (!foreground) daemonize();

    /* 初始化 */
    client_init();
    setup_signals();

    log_info("dacd starting (pid=%d, foreground=%d)", getpid(), foreground);

    if (!socket_create()) {
        log_error("Failed to create socket");
        return EXIT_FAILURE;
    }

    /* 事件循环 */
    int ret = run_event_loop();

    cleanup();
    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
