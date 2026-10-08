#!/usr/bin/env bash
# 兼容旧入口 → 本机或指定架构请用 build_linux_static.sh
exec "$(cd "$(dirname "$0")" && pwd)/build_linux_static.sh" x86_64 "$@"
