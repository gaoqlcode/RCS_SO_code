#include "rcs/rcs_api.h"
#include "MosaicAlgorithm.h"

bool processMosaic(const MosaicRequest &req, MosaicResponse &resp, Callbacks &cb, std::string *err)
{
    MosaicResult result;
    // 产品约定：始终匀光 + 三级 txt；不写 MOSAIC_RCS.tif
    // l3Scale 固定 100：与定标 TIFF DN=dBsm×100 配对（三级 dBsm=DN/100）
    const bool doFlatten = true;
    const double medOut = (req.medOut > 0) ? req.medOut : 0.35;
    result.writeRcsTif = false;
    result.writeL3 = true;
    result.l3Scale = 100.0;
    result.l3Layout = req.l3Layout;
    result.band = req.band.empty() ? "X" : req.band;
    result.pol = req.pol.empty() ? "HH" : req.pol;
    result.l3Header = req.header;
    std::string localErr;
    auto progress = [&](int p, const std::string &s) {
        if (cb.onProgress)
            cb.onProgress(p, s);
        if (cb.onMessage)
            cb.onMessage(s);
    };
    auto cancelled = [&]() { return cb.isCancelled && cb.isCancelled(); };

    bool ok = false;
    if (req.mode == 1) {
        ok = MosaicAlgorithm::runWithOffsets(req.inFolder, req.outFolder, req.offR, req.offC, doFlatten,
                                             medOut, result, progress, cancelled, &localErr,
                                             req.rawNrHint, req.rawNaHint);
    } else {
        ok = MosaicAlgorithm::run(req.inFolder, req.outFolder, doFlatten, medOut, result, progress,
                                  cancelled, &localErr, req.rawNrHint, req.rawNaHint);
    }
    if (!ok) {
        if (err)
            *err = localErr;
        return false;
    }
    resp.mosaicTif = result.mosaicTif;
    resp.flattenTif = result.flattenTif;
    resp.displayTif = result.displayTif;
    resp.rcsTif = result.rcsTif;
    resp.l3Path = result.l3Path;
    resp.seamCols = result.seamCols;
    return true;
}

bool flattenMosaic(const std::string &inTif, const std::string &outTif, double medOut,
                   const std::vector<int> &seamCols, Callbacks &cb, std::string *err)
{
    std::string localErr;
    auto progress = [&](int p, const std::string &s) {
        if (cb.onProgress)
            cb.onProgress(p, s);
    };
    if (!MosaicAlgorithm::flattenTiff(inTif, outTif, medOut, seamCols, progress, &localErr)) {
        if (err)
            *err = localErr;
        return false;
    }
    return true;
}

std::vector<std::string> listMosaicTiffs(const std::string &folder)
{
    return MosaicAlgorithm::listTiffFiles(folder);
}

bool loadMosaicGrayPreview(const std::string &path, int step, GrayImage &preview, int &fullW, int &fullH,
                           std::string *err)
{
    GrayPreview gp;
    if (!MosaicAlgorithm::loadGrayPreview(path, step, gp, err))
        return false;
    preview.width = gp.width;
    preview.height = gp.height;
    preview.pixels = std::move(gp.pixels);
    fullW = gp.fullW;
    fullH = gp.fullH;
    return true;
}

bool prepareMosaicInputs(const std::string &inFolder, const std::string &outFolder,
                         std::vector<std::string> &tiffPaths, Callbacks &cb, std::string *err,
                         int rawNrHint, int rawNaHint)
{
    auto progress = [&](int p, const std::string &s) {
        if (cb.onProgress)
            cb.onProgress(p, s);
        if (cb.onMessage)
            cb.onMessage(s);
    };
    auto cancelled = [&]() { return cb.isCancelled && cb.isCancelled(); };
    return MosaicAlgorithm::prepareInputFiles(inFolder, outFolder, tiffPaths, progress, cancelled, err,
                                              rawNrHint, rawNaHint);
}
