#include "rcs/rcs_api.h"

#if !defined(RCS_ENABLE_MOSAIC)
#define RCS_ENABLE_MOSAIC 0
#endif

#if RCS_ENABLE_MOSAIC
#include "MosaicAlgorithm.h"
#endif

static bool mosaicDisabled(std::string *err)
{
#if RCS_ENABLE_MOSAIC
    (void)err;
    return false;
#else
    if (err)
        *err = "大场景拼接已禁用";
    return true;
#endif
}

bool processMosaic(const MosaicRequest &req, MosaicResponse &resp, Callbacks &cb, std::string *err)
{
    if (mosaicDisabled(err))
        return false;
#if RCS_ENABLE_MOSAIC
    MosaicResult result;
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
        ok = MosaicAlgorithm::runWithOffsets(req.inFolder, req.outFolder, req.offR, req.offC, req.doFlatten,
                                             req.medOut, result, progress, cancelled, &localErr,
                                             req.rawNrHint, req.rawNaHint);
    } else {
        ok = MosaicAlgorithm::run(req.inFolder, req.outFolder, req.doFlatten, req.medOut, result, progress,
                                  cancelled, &localErr, req.rawNrHint, req.rawNaHint);
    }
    if (!ok) {
        if (err)
            *err = localErr;
        return false;
    }
    resp.mosaicTif = result.mosaicTif;
    resp.offsetsCsv = result.offsetsCsv;
    resp.flattenTif = result.flattenTif;
    resp.displayTif = result.displayTif;
    resp.seamCols = result.seamCols;
    return true;
#else
    (void)req;
    (void)resp;
    (void)cb;
    return false;
#endif
}

bool flattenMosaic(const std::string &inTif, const std::string &outTif, double medOut,
                   const std::vector<int> &seamCols, Callbacks &cb, std::string *err)
{
    if (mosaicDisabled(err))
        return false;
#if RCS_ENABLE_MOSAIC
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
#else
    (void)inTif;
    (void)outTif;
    (void)medOut;
    (void)seamCols;
    (void)cb;
    return false;
#endif
}

std::vector<std::string> listMosaicTiffs(const std::string &folder)
{
#if RCS_ENABLE_MOSAIC
    return MosaicAlgorithm::listTiffFiles(folder);
#else
    (void)folder;
    return std::vector<std::string>();
#endif
}

bool loadMosaicGrayPreview(const std::string &path, int step, GrayImage &preview, int &fullW, int &fullH,
                           std::string *err)
{
    if (mosaicDisabled(err))
        return false;
#if RCS_ENABLE_MOSAIC
    GrayPreview gp;
    if (!MosaicAlgorithm::loadGrayPreview(path, step, gp, err))
        return false;
    preview.width = gp.width;
    preview.height = gp.height;
    preview.pixels = std::move(gp.pixels);
    fullW = gp.fullW;
    fullH = gp.fullH;
    return true;
#else
    (void)path;
    (void)step;
    (void)preview;
    (void)fullW;
    (void)fullH;
    return false;
#endif
}

bool prepareMosaicInputs(const std::string &inFolder, const std::string &outFolder,
                         std::vector<std::string> &tiffPaths, Callbacks &cb, std::string *err,
                         int rawNrHint, int rawNaHint)
{
    if (mosaicDisabled(err))
        return false;
#if RCS_ENABLE_MOSAIC
    auto progress = [&](int p, const std::string &s) {
        if (cb.onProgress)
            cb.onProgress(p, s);
        if (cb.onMessage)
            cb.onMessage(s);
    };
    auto cancelled = [&]() { return cb.isCancelled && cb.isCancelled(); };
    return MosaicAlgorithm::prepareInputFiles(inFolder, outFolder, tiffPaths, progress, cancelled, err,
                                              rawNrHint, rawNaHint);
#else
    (void)inFolder;
    (void)outFolder;
    (void)tiffPaths;
    (void)cb;
    (void)rawNrHint;
    (void)rawNaHint;
    return false;
#endif
}
