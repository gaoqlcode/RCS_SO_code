/**
 * 功能测试：优先用 testdata/mini_*，没有再去仓库旁边找真实数据。
 */
#include "rcs/rcs_api.h"
#include "Util.hpp"

#include <cstdio>
#include <string>

// 测试源码在 tests/ 下，上一级是工程根
static std::string repoRoot()
{
    // __FILE__ ≈ .../RCS_SO_code/tests/func_main.cpp
    std::string f = __FILE__;
    size_t p = f.find_last_of("/\\");
    if (p != std::string::npos)
        f = f.substr(0, p); // tests
    p = f.find_last_of("/\\");
    if (p != std::string::npos)
        f = f.substr(0, p); // RCS_SO_code
    return f;
}

static int gPass = 0, gFail = 0;

static bool check(bool ok, const char *name, const std::string &detail = std::string())
{
    if (ok) {
        ++gPass;
        std::printf("[PASS] %s\n", name);
        return true;
    }
    ++gFail;
    std::fprintf(stderr, "[FAIL] %s %s\n", name, detail.c_str());
    return false;
}

static std::string findHrrpDir()
{
    const std::string mini = pathJoin(repoRoot(), "testdata/mini_hrrp");
    if (isDir(mini))
        return mini;
    return std::string();
}

static std::string findMosaicIn()
{
    const std::string mini = pathJoin(repoRoot(), "testdata/mini_mosaic");
    if (isDir(mini))
        return mini;
    return std::string();
}

static Callbacks makeCb()
{
    Callbacks cb;
    cb.onProgress = [](int p, const std::string &m) {
        std::printf("\r[%d%%] %s", p, m.c_str());
        std::fflush(stdout);
    };
    cb.onMessage = [](const std::string &m) { std::printf("\n%s\n", m.c_str()); };
    cb.isCancelled = []() { return false; };
    return cb;
}

static bool testHrrp(const std::string &dataDir)
{
    std::printf("\n=== HRRP ===\n");
    HrrpRequest req;
    req.dataFolder = dataDir;
    req.pol = "HH";
    req.tgtName = "func";
    req.outDir = pathJoin(repoRoot(), "testdata/lib_out/hrrp");
    req.autoPick = true;
    req.cropN = 8192;

    HrrpResponse resp;
    Callbacks cb = makeCb();
    std::string err;
    const bool ok = processHrrp(req, resp, cb, &err);
    std::printf("\n");
    if (!check(ok, "hrrp_process", err))
        return false;
    return check(pathExists(resp.hrrpPath) && pathExists(resp.pdatPath), "hrrp_outputs", resp.hrrpPath);
}

static bool testRcs(const std::string &dataDir)
{
    std::printf("\n=== RCS auto ===\n");
    RcsRequest req;
    req.dataFolder = dataDir;
    req.pol = "HH";
    req.tgtName = "func";
    req.outDir = pathJoin(repoRoot(), "testdata/lib_out/rcs");
    req.autoPick = true;
    req.cropN = 8192;

    RcsResponse resp;
    Callbacks cb = makeCb();
    std::string err;
    const bool ok = processRcs(req, resp, cb, &err);
    std::printf("\n");
    if (!check(ok, "rcs_process", err))
        return false;
    return check(pathExists(resp.pdatPath), "rcs_pdat", resp.pdatPath);
}

static bool testMosaic(const std::string &inDir)
{
    std::printf("\n=== Mosaic ===\n");
    MosaicRequest req;
    req.inFolder = inDir;
    req.outFolder = pathJoin(repoRoot(), "testdata/lib_out/mosaic");
    req.doFlatten = true;
    req.mode = 0;

    MosaicResponse resp;
    Callbacks cb = makeCb();
    std::string err;
    const bool ok = processMosaic(req, resp, cb, &err);
    std::printf("\n");
#if !defined(RCS_ENABLE_MOSAIC) || !RCS_ENABLE_MOSAIC
    // 默认禁用：确认接口拒绝调用
    (void)inDir;
    return check(!ok && err.find("禁用") != std::string::npos, "mosaic_disabled", err);
#else
    if (!check(ok, "mosaic_process", err))
        return false;
    return check(pathExists(resp.mosaicTif), "mosaic_tif", resp.mosaicTif);
#endif
}

int main()
{
    std::printf("rcs_func %s\n", rcsVersion());
    const std::string data1 = findHrrpDir();
    const std::string mosIn = findMosaicIn();
    if (data1.empty()) {
        std::printf("[SKIP] no HRRP data\n");
    } else {
        std::printf("hrrp data: %s\n", data1.c_str());
        testHrrp(data1);
        testRcs(data1);
    }
    if (mosIn.empty()) {
        std::printf("[SKIP] no mosaic input\n");
    } else {
        std::printf("mosaic in: %s\n", mosIn.c_str());
        testMosaic(mosIn);
    }
    std::printf("\n==== %d passed, %d failed ====\n", gPass, gFail);
    return gFail ? 1 : 0;
}
