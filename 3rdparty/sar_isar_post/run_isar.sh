#!/bin/sh
# 用法: ./run_isar.sh <数据目录> [输出目录] [其他参数...]
DIR=$(cd "$(dirname "$0")" && pwd)
[ $# -ge 1 ] || { echo "用法: $0 <数据目录> [输出目录] [参数...]"; exit 1; }
INPUT="$1"; shift
OUTPUT="${1:-$INPUT}"
[ $# -ge 1 ] && shift
exec "$DIR/bin/sarIsarPostProcess" --kind isar --input "$INPUT" --output "$OUTPUT" "$@"
