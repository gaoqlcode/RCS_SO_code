#include "rcs/rcs_api.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static void usage()
{
    std::cerr
        << "rcs_cli " << rcsVersion() << "\n"
        << "Usage:\n"
        << "  rcs_cli hrrp  --in <dat_dir> --pol HH --out <dir> [--sigma 18.25] [--crop 8192]\n"
        << "  rcs_cli rcs    --in <dat_dir> --pol HH --out <dir> [--az-mode 2] [--vsar 10]\n"
        << "  rcs_cli cw-rcs --in <dat_dir> --pol HH --out <dir> [--sigma 18.25]\n"
        << "  rcs_cli mosaic --in <img_dir> --out <dir>   (默认禁用)\n"
        << "  rcs_cli flatten --in <tif> --out <tif>      (默认禁用)\n";
}

static std::string argVal(int argc, char **argv, int &i)
{
    if (i + 1 >= argc)
        return std::string();
    return argv[++i];
}

static Callbacks makeCb()
{
    Callbacks cb;
    cb.onProgress = [](int p, const std::string &m) {
        std::cerr << "\r[" << p << "%] " << m << std::flush;
    };
    cb.onMessage = [](const std::string &m) { std::cerr << "\n" << m << "\n"; };
    cb.onError = [](const std::string &e) { std::cerr << "\n[ERROR] " << e << "\n"; };
    cb.isCancelled = []() { return false; };
    return cb;
}

static int cmdHrrp(int argc, char **argv)
{
    HrrpRequest req;
    req.autoPick = true;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--in")
            req.dataFolder = argVal(argc, argv, i);
        else if (a == "--out")
            req.outDir = argVal(argc, argv, i);
        else if (a == "--pol")
            req.pol = argVal(argc, argv, i);
        else if (a == "--tgt")
            req.tgtName = argVal(argc, argv, i);
        else if (a == "--sigma")
            req.sigmaTheoryDb = std::atof(argVal(argc, argv, i).c_str());
        else if (a == "--crop")
            req.cropN = std::atoi(argVal(argc, argv, i).c_str());
        else {
            std::cerr << "unknown arg: " << a << "\n";
            return 2;
        }
    }
    if (req.dataFolder.empty()) {
        usage();
        return 2;
    }
    HrrpResponse resp;
    auto cb = makeCb();
    std::string err;
    if (!processHrrp(req, resp, cb, &err)) {
        std::cerr << "\nHRRP failed: " << err << "\n";
        return 1;
    }
    std::cerr << "\nOK hrrp=" << resp.hrrpPath << "\n    pdat=" << resp.pdatPath << "\n";
    return 0;
}

static int cmdRcs(int argc, char **argv)
{
    RcsRequest req;
    req.autoPick = true;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--in")
            req.dataFolder = argVal(argc, argv, i);
        else if (a == "--out")
            req.outDir = argVal(argc, argv, i);
        else if (a == "--pol")
            req.pol = argVal(argc, argv, i);
        else if (a == "--tgt")
            req.tgtName = argVal(argc, argv, i);
        else if (a == "--sigma")
            req.sigmaTheoryDb = std::atof(argVal(argc, argv, i).c_str());
        else if (a == "--crop")
            req.cropN = std::atoi(argVal(argc, argv, i).c_str());
        else if (a == "--az-mode")
            req.azMode = std::atoi(argVal(argc, argv, i).c_str());
        else if (a == "--vsar")
            req.vSar = std::atof(argVal(argc, argv, i).c_str());
        else {
            std::cerr << "unknown arg: " << a << "\n";
            return 2;
        }
    }
    if (req.dataFolder.empty()) {
        usage();
        return 2;
    }
    RcsResponse resp;
    auto cb = makeCb();
    std::string err;
    if (!processRcs(req, resp, cb, &err)) {
        std::cerr << "\nRCS failed: " << err << "\n";
        return 1;
    }
    std::cerr << "\nOK pdat=" << resp.pdatPath << "\n    rcs=" << resp.rcsPath << "\n";
    return 0;
}


static int cmdCwRcs(int argc, char **argv)
{
    CwRcsRequest req;
    req.autoPick = true;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--in")
            req.dataFolder = argVal(argc, argv, i);
        else if (a == "--out")
            req.outDir = argVal(argc, argv, i);
        else if (a == "--pol")
            req.pol = argVal(argc, argv, i);
        else if (a == "--tgt")
            req.tgtName = argVal(argc, argv, i);
        else if (a == "--sigma")
            req.sigmaTheoryDb = std::atof(argVal(argc, argv, i).c_str());
        else {
            std::cerr << "unknown arg: " << a << "\n";
            return 2;
        }
    }
    if (req.dataFolder.empty()) {
        usage();
        return 2;
    }
    CwRcsResponse resp;
    auto cb = makeCb();
    std::string err;
    if (!processCwRcs(req, resp, cb, &err)) {
        std::cerr << "\ncw-rcs failed: " << err << "\n";
        return 1;
    }
    std::cerr << "\nOK rcs=" << resp.l3Path << "\n";
    return 0;
}

static int cmdMosaic(int argc, char **argv)
{
    MosaicRequest req;
    req.doFlatten = true;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--in")
            req.inFolder = argVal(argc, argv, i);
        else if (a == "--out")
            req.outFolder = argVal(argc, argv, i);
        else if (a == "--flatten")
            req.doFlatten = true;
        else if (a == "--no-flatten")
            req.doFlatten = false;
        else if (a == "--med")
            req.medOut = std::atof(argVal(argc, argv, i).c_str());
        else {
            std::cerr << "unknown arg: " << a << "\n";
            return 2;
        }
    }
    if (req.inFolder.empty()) {
        usage();
        return 2;
    }
    MosaicResponse resp;
    auto cb = makeCb();
    std::string err;
    if (!processMosaic(req, resp, cb, &err)) {
        std::cerr << "\nMosaic failed: " << err << "\n";
        return 1;
    }
    std::cerr << "\nOK mosaic=" << resp.mosaicTif << "\n";
    return 0;
}

static int cmdFlatten(int argc, char **argv)
{
    std::string in, out;
    double med = 0.35;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--in")
            in = argVal(argc, argv, i);
        else if (a == "--out")
            out = argVal(argc, argv, i);
        else if (a == "--med")
            med = std::atof(argVal(argc, argv, i).c_str());
        else {
            std::cerr << "unknown arg: " << a << "\n";
            return 2;
        }
    }
    if (in.empty()) {
        usage();
        return 2;
    }
    if (out.empty()) {
        const auto dot = in.find_last_of('.');
        out = (dot == std::string::npos) ? (in + "_匀光.tif") : (in.substr(0, dot) + "_匀光.tif");
    }
    auto cb = makeCb();
    std::string err;
    if (!flattenMosaic(in, out, med, {}, cb, &err)) {
        std::cerr << "\nFlatten failed: " << err << "\n";
        return 1;
    }
    std::cerr << "\nOK flatten=" << out << "\n";
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage();
        return 2;
    }
    const std::string cmd = argv[1];
    if (cmd == "hrrp")
        return cmdHrrp(argc, argv);
    if (cmd == "rcs")
        return cmdRcs(argc, argv);
    if (cmd == "cw-rcs" || cmd == "dianpin")
        return cmdCwRcs(argc, argv);
    if (cmd == "mosaic")
        return cmdMosaic(argc, argv);
    if (cmd == "flatten")
        return cmdFlatten(argc, argv);
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        usage();
        return 0;
    }
    usage();
    return 2;
}
