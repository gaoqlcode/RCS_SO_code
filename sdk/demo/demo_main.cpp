#include "rcs/rcs_api.h"

#include <cstdio>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// SDK 命令行示例：验证链接环境，并演示三条业务怎么调。
//
// 做正式界面时建议：
//   - 把 process* 丢到工作线程，别堵 UI；
//   - onProgress / onMessage 里切回主线程刷控件；
//   - 需要人选点时，在回调里弹窗，等用户点完再 return；
//   - 先 autoPick=true 跑通数据和路径，再接选点。
//
// 本 SDK 不含大图拼接接口。
// ---------------------------------------------------------------------------

static Callbacks makeCb()
{
    Callbacks cb;
    cb.onProgress = [](int p, const std::string &msg) {
        std::cerr << "\r[" << p << "%] " << msg << "                    " << std::flush;
    };
    cb.onMessage = [](const std::string &msg) { std::cerr << "\n" << msg << "\n"; };
    cb.onError = [](const std::string &msg) { std::cerr << "\n[错误] " << msg << "\n"; };
    cb.isCancelled = []() { return false; };
    // 选点回调这里不挂：下面几个 run* 都开了 autoPick
    return cb;
}

static void printUsage(const char *exe)
{
    std::cerr
        << "用法:\n"
        << "  " << exe << "\n"
        << "      打印库版本\n"
        << "  " << exe << " hrrp <数据目录> <输出目录> [极化]\n"
        << "      HRRP（自动选峰）\n"
        << "  " << exe << " rcs  <数据目录> <输出目录> [极化]\n"
        << "      RCS（自动选点）\n"
        << "  " << exe << " cw   <数据目录> <输出目录> [极化]\n"
        << "      点频 RCS（自动选点）\n"
        << "\n"
        << "极化默认 HH。数据目录里需要有对应 *_L0_<极化>.dat。\n";
}

static int runHrrp(const std::string &inDir, const std::string &outDir, const std::string &pol)
{
    HrrpRequest req;
    req.dataFolder = inDir;
    req.outDir = outDir;
    req.pol = pol;
    req.tgtName = "目标";
    req.sigmaTheoryDb = 18.25;
    req.cropN = 8192;
    req.autoPick = true;

    HrrpResponse resp;
    Callbacks cb = makeCb();
    std::string err;
    if (!processHrrp(req, resp, cb, &err)) {
        std::cerr << "\nHRRP 失败: " << err << "\n";
        return 1;
    }
    std::cerr << "\nHRRP 完成\n"
              << "  曲线点数: " << resp.rangeM.size() << "\n"
              << "  hrrp: " << resp.hrrpPath << "\n"
              << "  pdat: " << resp.pdatPath << "\n"
              << "  l1:   " << resp.l1Path << "\n";
    // 接界面时：用 resp.rangeM / resp.rcsDbsm（以及 phaseDeg）画一维图
    return 0;
}

static int runRcs(const std::string &inDir, const std::string &outDir, const std::string &pol)
{
    RcsRequest req;
    req.dataFolder = inDir;
    req.outDir = outDir;
    req.pol = pol;
    req.tgtName = "目标";
    req.sigmaTheoryDb = 18.25;
    req.cropN = 8192;
    req.azMode = 2; // 1 条带 / 2 聚束
    req.vSar = 10.0;
    req.autoPick = true;

    RcsResponse resp;
    Callbacks cb = makeCb();
    std::string err;
    if (!processRcs(req, resp, cb, &err)) {
        std::cerr << "\nRCS 失败: " << err << "\n";
        return 1;
    }
    std::cerr << "\nRCS 完成\n"
              << "  目标数: " << resp.targets.size() << "\n"
              << "  rcs:  " << resp.rcsPath << "\n"
              << "  pdat: " << resp.pdatPath << "\n";
    for (size_t i = 0; i < resp.targets.size(); ++i) {
        const RcsTargetItem &t = resp.targets[i];
        std::cerr << "    [" << i << "] valid=" << (t.valid ? 1 : 0)
                  << " RCS=" << t.rcsDbsm << " dBsm"
                  << " az=" << t.azViewDeg << " deg"
                  << " R=" << t.rangeM << " m"
                  << " " << t.label << "\n";
    }
    return 0;
}

static int runCw(const std::string &inDir, const std::string &outDir, const std::string &pol)
{
    CwRcsRequest req;
    req.dataFolder = inDir;
    req.outDir = outDir;
    req.pol = pol;
    req.tgtName = "目标";
    req.sigmaTheoryDb = 18.25;
    req.targetMode = 1;
    req.autoPick = true;

    CwRcsResponse resp;
    Callbacks cb = makeCb();
    std::string err;
    if (!processCwRcs(req, resp, cb, &err)) {
        std::cerr << "\n点频 RCS 失败: " << err << "\n";
        return 1;
    }
    std::cerr << "\n点频 RCS 完成\n"
              << "  频点数: " << resp.freqGHz.size() << "\n"
              << "  l3: " << resp.l3Path << "\n";
    const size_t n = resp.freqGHz.size() < 5 ? resp.freqGHz.size() : 5;
    for (size_t i = 0; i < n; ++i) {
        std::cerr << "    f=" << resp.freqGHz[i] << " GHz, RCS=" << resp.rcsDbsm[i] << " dBsm\n";
    }
    if (resp.freqGHz.size() > n)
        std::cerr << "    ...\n";
    return 0;
}

int main(int argc, char **argv)
{
    std::cout << "rcs_proc 版本: " << rcsVersion() << "\n";

    if (argc < 2)
        return 0;

    const std::string cmd = argv[1];
    if (argc < 4) {
        printUsage(argv[0]);
        return 2;
    }
    const std::string pol = (argc >= 5) ? argv[4] : "HH";

    if (cmd == "hrrp")
        return runHrrp(argv[2], argv[3], pol);
    if (cmd == "rcs")
        return runRcs(argv[2], argv[3], pol);
    if (cmd == "cw" || cmd == "cwrcs" || cmd == "点频")
        return runCw(argv[2], argv[3], pol);

    printUsage(argv[0]);
    return 2;
}
