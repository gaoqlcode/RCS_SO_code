成像后处理程序在 engine/bin/sarIsarPostProcess，运行库在 engine/lib。

不需要安装 MATLAB Runtime，也不需要系统 Qt。程序只从本包 engine/lib 加载依赖。

libToZeroDataCpp11.so 必须和 librcs_proc.so 放在同一个 lib 目录，客户仍然只链接 -lrcs_proc。
