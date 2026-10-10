// cli_demo：命令行调用例子，见 README.md

#include "rcs/rcs.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void usage(const char *exe)
{
    std::cerr
        << "\nrcs_cli_demo\n"
        << "用法:\n"
        << "  " << exe << "\n"
        << "  " << exe << " list     <数据目录>\n"
        << "  " << exe << " probe    <数据目录> [极化]\n"
        << "  " << exe << " preview  <数据目录> [极化]\n"
        << "  " << exe << " hrrp-i   <数据目录> <输出目录> [极化]   # 终端确认峰距\n"
        << "  " << exe << " hrrp     <数据目录> <输出目录> [极化]   # autoPick 冒烟\n"
        << "  " << exe << " rcs      <数据目录> <输出目录> [极化]\n"
        << "  " << exe << " cw       <数据目录> <输出目录> [极化]\n"
        << "  " << exe << " sigma0   <raw> <输出> <cal_x,y,w,h> <tgt_x,y,w,h>\n";
}

static bool parseRoi(const char *s, RectRoi &out)
{
    int x = 0, y = 0, w = 0, h = 0;
    if (std::sscanf(s, "%d,%d,%d,%d", &x, &y, &w, &h) != 4)
        return false;
    out = RectRoi(x, y, w, h);
    return out.valid();
}

static Callbacks makeConsoleCallbacks()
{
    Callbacks cb;
    cb.onProgress = [](int p, const std::string &m) {
        std::cerr << "\r[" << p << "%] " << m << "                    " << std::flush;
    };
    cb.onMessage = [](const std::string &m) { std::cerr << "\n[消息] " << m << "\n"; };
    cb.onError = [](const std::string &m) { std::cerr << "\n[错误] " << m << "\n"; };
    cb.isCancelled = []() { return false; };
    return cb;
}

static int cmdList(const std::string &folder)
{
    std::string err;

    std::vector<std::string> pols;
    if (!listAvailablePolarizations(folder, pols, &err)) {
        std::cerr << "listAvailablePolarizations 失败: " << err << "\n";
        return 1;
    }
    std::cout << "极化:";
    for (size_t i = 0; i < pols.size(); ++i)
        std::cout << " " << pols[i];
    std::cout << "\n";

    for (size_t i = 0; i < pols.size(); ++i) {
        std::vector<std::string> files;
        if (!listL0Files(folder, pols[i], files, &err))
            continue;
        std::cout << "  " << pols[i] << "  共 " << files.size() << " 个"
                  << "  eg. " << files.front() << "\n";
    }

    std::vector<std::string> raws;
    if (listImagedRawFiles(folder, raws, 0)) {
        std::cout << "成像 RAW 共 " << raws.size() << " 个";
        if (!raws.empty())
            std::cout << "  eg. " << raws.front();
        std::cout << "\n";
    }
    return 0;
}

static int cmdProbe(const std::string &folder, const std::string &pol)
{
    std::string err;
    L0FolderProbe p;
    if (!probeDataFolder(folder, pol, p, &err)) {
        std::cerr << "probeDataFolder 失败: " << err << "\n";
        return 1;
    }
    std::cout << "极化=" << p.pol << "  文件数=" << p.fileCount << "\n"
              << "  first=" << p.firstFile << "\n"
              << "  last =" << p.lastFile << "\n";
    if (p.hasHeader) {
        std::cout << "  中心频率=" << p.firstHeader.fcGHz << " GHz"
                  << "  带宽=" << p.firstHeader.bwMHz << " MHz\n";
    }

    return 0;
}

static int cmdPreview(const std::string &folder, const std::string &pol)
{
    L0PreviewRequest req;
    req.dataFolder = folder;
    req.pol = pol;
    req.maxFiles = 3; // <=0 全部
    req.removeDC = true;

    Callbacks cb = makeConsoleCallbacks();
    L0PreviewResponse resp;
    std::string err;
    if (!previewL0Folder(req, resp, cb, &err)) {
        std::cerr << "\npreviewL0Folder 失败: " << err << "\n";
        return 1;
    }
    std::cout << "\n预览条目 " << resp.items.size() << " 个（用于画时域/频域曲线）:\n";
    for (size_t i = 0; i < resp.items.size(); ++i) {
        const L0PulsePreviewItem &it = resp.items[i];
        std::cout << "  " << it.fileName << "  点数=" << it.tUs.size()
                  << "  ampPk=" << it.ampPkUs << " us  fPk=" << it.fPkMHz << " MHz\n";
    }
    return 0;
}

// 默认 autoPick=false，用终端代替弹窗接 onCornerConfirm
static int cmdHrrpInteract(const std::string &folder, const std::string &outDir,
                           const std::string &pol)
{
    HrrpRequest req;
    req.dataFolder = folder;
    req.outDir = outDir;
    req.pol = pol;

    Callbacks cb = makeConsoleCallbacks();
    cb.onCornerConfirm = [](const std::vector<double> &R, const std::vector<double> &profileDb,
                            int peakIdx, InteractSelection &sel) -> bool {
        const double suggest =
            (peakIdx >= 0 && peakIdx < static_cast<int>(R.size())) ? R[static_cast<size_t>(peakIdx)]
                                                                   : 0.0;
        std::cerr << "\n[onCornerConfirm] 点数=" << R.size()
                  << " peakIdx=" << peakIdx << " 建议=" << suggest << " m\n"
                  << "确认距离(米)，回车用建议值，cancel 取消\n> " << std::flush;

        std::string line;
        if (!std::getline(std::cin, line))
            return false;
        if (line == "cancel" || line == "CANCEL")
            return false;
        if (line.empty()) {
            sel.cornerRangeOverrideM = -1; // <0 用自动峰
            return true;
        }
        std::istringstream iss(line);
        double v = 0;
        if (!(iss >> v)) {
            std::cerr << "输入无效，沿用自动峰\n";
            sel.cornerRangeOverrideM = -1;
            return true;
        }
        sel.cornerRangeOverrideM = v;
        std::cerr << "已确认距离=" << v << " m\n";
        (void)profileDb;
        return true;
    };

    HrrpResponse resp;
    std::string err;
    std::cerr << "开始 processHrrp（交互）…\n";
    if (!processHrrp(req, resp, cb, &err)) {
        std::cerr << "\n失败: " << err << "\n";
        return 1;
    }
    std::cout << "\nOK " << resp.hrrpPath << " n=" << resp.rangeM.size() << "\n";
    return 0;
}

static int cmdHrrpSmoke(const std::string &folder, const std::string &outDir,
                        const std::string &pol)
{
    HrrpRequest req;
    req.dataFolder = folder;
    req.outDir = outDir;
    req.pol = pol;
    req.autoPick = true;

    Callbacks cb = makeConsoleCallbacks();
    HrrpResponse resp;
    std::string err;
    if (!processHrrp(req, resp, cb, &err)) {
        std::cerr << "\n失败: " << err << "\n";
        return 1;
    }
    std::cout << "\nOK " << resp.hrrpPath << " n=" << resp.rangeM.size() << "\n";
    return 0;
}

static int cmdRcsSmoke(const std::string &folder, const std::string &outDir, const std::string &pol)
{
    RcsRequest req;
    req.dataFolder = folder;
    req.outDir = outDir;
    req.pol = pol;
    req.azMode = rcs::AzSpotlight;
    req.autoPick = true;

    Callbacks cb = makeConsoleCallbacks();
    RcsResponse resp;
    std::string err;
    if (!processRcs(req, resp, cb, &err)) {
        std::cerr << "\n失败: " << err << "\n";
        return 1;
    }
    std::cout << "\nOK " << resp.rcsPath << " targets=" << resp.targets.size() << "\n";
    return 0;
}

static int cmdCwSmoke(const std::string &folder, const std::string &outDir, const std::string &pol)
{
    CwRcsRequest req;
    req.dataFolder = folder;
    req.outDir = outDir;
    req.pol = pol;
    req.autoPick = true;

    Callbacks cb = makeConsoleCallbacks();
    CwRcsResponse resp;
    std::string err;
    if (!processCwRcs(req, resp, cb, &err)) {
        std::cerr << "\n失败: " << err << "\n";
        return 1;
    }
    std::cout << "\nOK " << resp.l3Path << " n=" << resp.freqGHz.size() << "\n";
    return 0;
}

static int cmdSigma0Smoke(const std::string &rawPath, const std::string &outDir, const RectRoi &cal,
                          const RectRoi &tgt)
{
    Sigma0Request req;
    req.rawPath = rawPath;
    req.outDir = outDir;
    req.autoPick = true;
    req.calBufRoi = cal;
    req.targetRois.clear();
    req.targetRois.push_back(tgt);

    std::string err;
    if (!parseImagedRawSize(req.rawPath, req.nr, req.na, &err)) {
        std::cerr << "parseImagedRawSize 失败: " << err << "\n";
        return 2;
    }
    std::cout << "解析尺寸 Nr=" << req.nr << " Na=" << req.na << "\n";

    Callbacks cb = makeConsoleCallbacks();
    Sigma0Response resp;
    if (!processSigma0(req, resp, cb, &err)) {
        std::cerr << "\n失败: " << err << "\n";
        return 1;
    }
    std::cout << "\nOK " << resp.csPath << " K=" << resp.K << "\n";
    return 0;
}

int main(int argc, char **argv)
{
    std::cout << "rcs_proc 版本: " << rcsVersion() << "\n";
    if (argc < 2) {
        usage(argv[0]);
        return 0;
    }

    const std::string cmd = argv[1];

    if (cmd == "list") {
        if (argc < 3) {
            usage(argv[0]);
            return 2;
        }
        return cmdList(argv[2]);
    }
    if (cmd == "probe") {
        if (argc < 3) {
            usage(argv[0]);
            return 2;
        }
        return cmdProbe(argv[2], (argc >= 4) ? argv[3] : "HH");
    }
    if (cmd == "preview") {
        if (argc < 3) {
            usage(argv[0]);
            return 2;
        }
        return cmdPreview(argv[2], (argc >= 4) ? argv[3] : "HH");
    }
    if (cmd == "hrrp-i" || cmd == "hrrp-interact") {
        if (argc < 4) {
            usage(argv[0]);
            return 2;
        }
        return cmdHrrpInteract(argv[2], argv[3], (argc >= 5) ? argv[4] : "HH");
    }
    if (cmd == "hrrp") {
        if (argc < 4) {
            usage(argv[0]);
            return 2;
        }
        return cmdHrrpSmoke(argv[2], argv[3], (argc >= 5) ? argv[4] : "HH");
    }
    if (cmd == "rcs") {
        if (argc < 4) {
            usage(argv[0]);
            return 2;
        }
        return cmdRcsSmoke(argv[2], argv[3], (argc >= 5) ? argv[4] : "HH");
    }
    if (cmd == "cw") {
        if (argc < 4) {
            usage(argv[0]);
            return 2;
        }
        return cmdCwSmoke(argv[2], argv[3], (argc >= 5) ? argv[4] : "HH");
    }
    if (cmd == "sigma0") {
        if (argc < 6) {
            usage(argv[0]);
            return 2;
        }
        RectRoi cal, tgt;
        if (!parseRoi(argv[4], cal) || !parseRoi(argv[5], tgt)) {
            std::cerr << "ROI 格式: x,y,w,h（全分辨率：x=方位列, y=距离行）\n";
            return 2;
        }
        return cmdSigma0Smoke(argv[2], argv[3], cal, tgt);
    }

    usage(argv[0]);
    return 2;
}
