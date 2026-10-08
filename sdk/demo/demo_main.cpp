#include "rcs/rcs_api.h"

#include <cstdio>
#include <iostream>
#include <string>

// 最小开发 demo：打印版本；可选跑一趟 HRRP（自动选峰）

static Callbacks makeCb()
{
    Callbacks cb;
    cb.onProgress = [](int p, const std::string &msg) {
        std::cerr << "\r[" << p << "%] " << msg << "          " << std::flush;
    };
    cb.onMessage = [](const std::string &msg) { std::cerr << "\n" << msg << "\n"; };
    cb.isCancelled = []() { return false; };
    return cb;
}

static void printUsage(const char *exe)
{
    std::cerr
        << "用法:\n"
        << "  " << exe << "\n"
        << "      只打印库版本\n"
        << "  " << exe << " hrrp <数据目录> <输出目录> [极化]\n"
        << "      跑 HRRP（autoPick，无需界面选点）\n"
        << "  " << exe << " mosaic <图目录> <输出目录>\n"
        << "      自动配准拼接\n";
}

static int runHrrp(const std::string &inDir, const std::string &outDir, const std::string &pol)
{
    HrrpRequest req;
    req.dataFolder = inDir;
    req.outDir = outDir;
    req.pol = pol;
    req.autoPick = true; // demo 不交互

    HrrpResponse resp;
    Callbacks cb = makeCb();
    std::string err;
    if (!processHrrp(req, resp, cb, &err)) {
        std::cerr << "\nHRRP 失败: " << err << "\n";
        return 1;
    }
    std::cerr << "\n完成\n  hrrp: " << resp.hrrpPath << "\n  pdat: " << resp.pdatPath << "\n";
    if (!resp.rangeM.empty()) {
        std::cerr << "  曲线点数: " << resp.rangeM.size() << "\n";
    }
    return 0;
}

static int runMosaic(const std::string &inDir, const std::string &outDir)
{
    MosaicRequest req;
    req.inFolder = inDir;
    req.outFolder = outDir;
    req.mode = 0; // 自动配准
    req.doFlatten = true;

    MosaicResponse resp;
    Callbacks cb = makeCb();
    std::string err;
    if (!processMosaic(req, resp, cb, &err)) {
        std::cerr << "\n拼接失败: " << err << "\n";
        return 1;
    }
    std::cerr << "\n完成\n  mosaic: " << resp.mosaicTif << "\n";
    if (!resp.flattenTif.empty())
        std::cerr << "  flatten: " << resp.flattenTif << "\n";
    return 0;
}

int main(int argc, char **argv)
{
    std::cout << "rcs_proc 版本: " << rcsVersion() << "\n";

    if (argc < 2)
        return 0;

    const std::string cmd = argv[1];
    if (cmd == "hrrp") {
        if (argc < 4) {
            printUsage(argv[0]);
            return 2;
        }
        const std::string pol = (argc >= 5) ? argv[4] : "HH";
        return runHrrp(argv[2], argv[3], pol);
    }
    if (cmd == "mosaic") {
        if (argc < 4) {
            printUsage(argv[0]);
            return 2;
        }
        return runMosaic(argv[2], argv[3]);
    }

    printUsage(argv[0]);
    return 2;
}
