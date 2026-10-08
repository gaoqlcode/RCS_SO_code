# Demo 说明

本目录是二次开发最小例子，只依赖上层 `sdk/include` 与 `sdk/lib`。

## 编译

```bash
cd sdk/demo
cmake -S . -B build
cmake --build build
```

## 运行

```bash
./build/rcs_demo
./build/rcs_demo hrrp ../../testdata/mini_hrrp /tmp/rcs_demo_hrrp HH
./build/rcs_demo mosaic ../../testdata/mini_mosaic /tmp/rcs_demo_mosaic
```

若提示找不到 `.so`：

```bash
export LD_LIBRARY_PATH=$PWD/../lib:$LD_LIBRARY_PATH
```
