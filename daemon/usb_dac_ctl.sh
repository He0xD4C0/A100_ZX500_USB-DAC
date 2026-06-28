#!/system/bin/sh
#
# usb_dac_ctl.sh — NW-A105 USB DAC 切换控制脚本
#
# 功能: 动态启用/禁用 UAC2 Gadget, 管理 bridge daemon
#
# 用法:
#   usb_dac_ctl.sh on          # 启用 USB DAC 模式
#   usb_dac_ctl.sh off         # 禁用 USB DAC 模式
#   usb_dac_ctl.sh status      # 查看当前状态
#
# 部署: adb push usb_dac_ctl.sh /vendor/bin/
#       chmod 755 /vendor/bin/usb_dac_ctl.sh
#

set -e

# ============================================================
# 配置
# ============================================================

GADGET_CONFIG="/config/usb_gadget/g1"
UAC2_FUNC="uac2.gs0"
UAC2_LINK="f2"
BRIDGE_BIN="/vendor/bin/uac2_bridge"
BRIDGE_PIDFILE="/data/local/tmp/uac2_bridge.pid"

# ALSA 设备
CXD_CARD=1
CXD_DEVICE=0

# ============================================================
# 工具函数
# ============================================================

log() {
    echo "[usb_dac] $*"
}


is_bridge_running() {
    [ -f "$BRIDGE_PIDFILE" ] && kill -0 $(cat "$BRIDGE_PIDFILE") 2>/dev/null
    return $?
}

# ============================================================
# 启用 USB DAC
# ============================================================

enable_dac() {
    log "Enabling USB DAC mode..."

    # 1. 确保内核模块已加载 (已内建编译，无需加载)

    # 2. 检查 UAC2 函数是否已存在
    if [ -d "$GADGET_CONFIG/functions/$UAC2_FUNC" ]; then
        log "UAC2 function already exists, skipping create"
    else
        log "Creating UAC2 function..."
        mkdir "$GADGET_CONFIG/functions/$UAC2_FUNC"

        # 配置立体声, Hi-Res
        echo 3 > "$GADGET_CONFIG/functions/$UAC2_FUNC/p_chmask"    # 声道掩码: 立体声
        echo 3 > "$GADGET_CONFIG/functions/$UAC2_FUNC/c_chmask"
        echo 192000 > "$GADGET_CONFIG/functions/$UAC2_FUNC/p_srate" # Hi-Res 采样率
        echo 192000 > "$GADGET_CONFIG/functions/$UAC2_FUNC/c_srate"
        echo 24 > "$GADGET_CONFIG/functions/$UAC2_FUNC/p_ssize"    # 24 bit
        echo 24 > "$GADGET_CONFIG/functions/$UAC2_FUNC/c_ssize"

        log "UAC2 function created"
    fi

    # 3. 绑定 UAC2 到配置 (如果尚未绑定)
    if [ -L "$GADGET_CONFIG/configs/b.1/$UAC2_LINK" ]; then
        log "UAC2 already linked to config"
    else
        log "Binding UAC2 to USB config..."
        ln -s "$GADGET_CONFIG/functions/$UAC2_FUNC" \
              "$GADGET_CONFIG/configs/b.1/$UAC2_LINK"

        # 触发 USB 重新枚举
        echo "" > "$GADGET_CONFIG/UDC" 2>/dev/null  # unbind
        sleep 1
        echo ci_hdrc.0 > "$GADGET_CONFIG/UDC"       # rebind

        log "UAC2 bound, USB re-enumerated"
        # 等系统识别 UAC2
        sleep 2
    fi

    # 4. 启动 bridge daemon
    if is_bridge_running; then
        log "Bridge already running"
    else
        log "Starting bridge daemon..."
        # 检测 UAC2 card 编号
        UAC2_CARD=$(cat /proc/asound/cards 2>/dev/null | \
                    grep -c "UAC\|Gadget\|uac2" 2>/dev/null || echo -n "")

        # 尝试常见的 card#2
        $BRIDGE_BIN start -r 44100 -p 1024 &
        BRIDGE_PID=$!
        echo $BRIDGE_PID > "$BRIDGE_PIDFILE"

        # 等待确认启动
        sleep 1
        if is_bridge_running; then
            log "Bridge started (pid $BRIDGE_PID)"
        else
            log "WARNING: Bridge may have failed to start"
        fi
    fi

    log "USB DAC mode ACTIVE"
    log "Connect A105 to PC and select 'USB Audio Device'"
}

# ============================================================
# 禁用 USB DAC
# ============================================================

disable_dac() {
    log "Disabling USB DAC mode..."

    # 1. 停止 bridge daemon
    if is_bridge_running; then
        log "Stopping bridge..."
        $BRIDGE_BIN stop 2>/dev/null || kill -TERM $(cat "$BRIDGE_PIDFILE") 2>/dev/null || true
        rm -f "$BRIDGE_PIDFILE"
        sleep 1
    else
        log "Bridge not running"
    fi

    # 2. 解绑 UAC2
    if [ -L "$GADGET_CONFIG/configs/b.1/$UAC2_LINK" ]; then
        log "Unbinding UAC2..."
        # unbind-rebind 让 USB 重枚举
        echo "" > "$GADGET_CONFIG/UDC" 2>/dev/null
        rm "$GADGET_CONFIG/configs/b.1/$UAC2_LINK"
        echo ci_hdrc.0 > "$GADGET_CONFIG/UDC" 2>/dev/null
        log "UAC2 unbound"
    else
        log "UAC2 not bound"
    fi

    # 3. 清理 UAC2 函数
    if [ -d "$GADGET_CONFIG/functions/$UAC2_FUNC" ]; then
        rmdir "$GADGET_CONFIG/functions/$UAC2_FUNC" 2>/dev/null || true
        log "UAC2 function removed"
    fi

    # 4. 卸载模块 (不改 — 保留模块已备下次使用)

    log "USB DAC mode DISABLED"
}

# ============================================================
# 状态查询
# ============================================================

show_status() {
    echo "======================================"
    echo " NW-A105 USB DAC 状态"
    echo "======================================"

    echo ""
    echo "-- ALSA 设备 --"
    cat /proc/asound/cards 2>/dev/null

    echo ""
    echo "-- UAC2 模块 --"
    if is_uac2_loaded; then
        echo "  usb_f_uac2:   LOADED"
        lsmod | grep uac2
    else
        echo "  usb_f_uac2:   NOT LOADED"
    fi

    echo ""
    echo "-- Bridge Daemon --"
    if is_bridge_running; then
        PID=$(cat "$BRIDGE_PIDFILE" 2>/dev/null)
        echo "  uac2_bridge:  RUNNING (pid $PID)"
    else
        echo "  uac2_bridge:  STOPPED"
    fi

    echo ""
    echo "-- Gadget Functions --"
    for f in "$GADGET_CONFIG/configs/b.1"/*; do
        if [ -L "$f" ]; then
            echo "  $(basename $f) → $(readlink $f)"
        fi
    done

    echo ""
    echo "-- UAC2 函数参数 --"
    if [ -d "$GADGET_CONFIG/functions/$UAC2_FUNC" ]; then
        echo "  p_chmask:  $(cat $GADGET_CONFIG/functions/$UAC2_FUNC/p_chmask 2>/dev/null)"
        echo "  p_srate:   $(cat $GADGET_CONFIG/functions/$UAC2_FUNC/p_srate 2>/dev/null)"
        echo "  p_ssize:   $(cat $GADGET_CONFIG/functions/$UAC2_FUNC/p_ssize 2>/dev/null)"
    else
        echo "  UAC2 function not created"
    fi

    echo ""
    echo "-- USB 连接状态 --"
    cat /sys/class/android_usb/android0/state 2>/dev/null
    echo ""

    if is_bridge_running; then
        echo "-- 数据统计 --"
        $BRIDGE_BIN status 2>/dev/null || true
    fi

    echo "======================================"
}

# ============================================================
# 主逻辑
# ============================================================

case "${1:-help}" in
    on|enable|start)
        enable_dac
        ;;
    off|disable|stop)
        disable_dac
        ;;
    status|state)
        show_status
        ;;
    *)
        echo "用法: $0 {on|off|status}"
        echo ""
        echo "  on      启用 USB DAC 模式 (A105 作为 PC 的 DAC)"
        echo "  off     禁用 USB DAC 模式 (恢复 ADB/MTP)"
        echo "  status  查看当前状态"
        exit 1
        ;;
esac
