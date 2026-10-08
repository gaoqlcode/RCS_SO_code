#ifndef MOSAICALGORITHM_H
#define MOSAICALGORITHM_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct GrayPreview {
    int width = 0;
    int height = 0;
    int fullW = 0;
    int fullH = 0;
    std::vector<uint8_t> pixels;
};

struct MosaicResult {
    std::string mosaicTif;
    std::string offsetsCsv;
    std::string flattenTif;
    std::string displayTif; // MOSAIC_显示.tif（拼后明暗调整，原值 MOSAIC.tif 不动）
    std::vector<int> seamCols;
};

class MosaicAlgorithm {
public:
    /** 列出输入 TIFF（排除 MOSAIC*），按文件名数字排序 */
    static std::vector<std::string> listTiffFiles(const std::string &folder);

    /**
     * @brief 与 MATLAB prepareInput 一致：有 TIFF 则直接用；仅有 RAW 时自动判尺寸并转成 TIFF
     * @param rawNrHint/rawNaHint 手动指定尺寸(>0)；0=自动（文件名 / 增益包络）
     * @param tiffPaths 输出可用的 TIFF 路径列表
     */
    static bool prepareInputFiles(const std::string &inFolder, const std::string &outFolder,
                                  std::vector<std::string> &tiffPaths,
                                  std::function<void(int, const std::string &)> progress,
                                  std::function<bool()> cancelled,
                                  std::string *err = nullptr,
                                  int rawNrHint = 0, int rawNaHint = 0);

    /**
     * @brief 读 uint16 灰度 TIFF 并生成 8bit 预览（线性映射到 0..255）
     * @param step 抽稀步长（如 8）
     */
    static bool loadGrayPreview(const std::string &path, int step, GrayPreview &preview,
                                std::string *err = nullptr);

    /** 自动配准 + 融合（方位向） */
    static bool run(const std::string &inFolder, const std::string &outFolder,
                    bool doFlatten, double medOut,
                    MosaicResult &result,
                    std::function<void(int, const std::string &)> progress,
                    std::function<bool()> cancelled,
                    std::string *err = nullptr,
                    int rawNrHint = 0, int rawNaHint = 0);

    /**
     * @brief 按给定绝对偏移融合（手动摆位后调用）
     * offR/offC 长度=图数，单位为全分辨率像素
     */
    static bool runWithOffsets(const std::string &inFolder, const std::string &outFolder,
                               const std::vector<int> &offR, const std::vector<int> &offC,
                               bool doFlatten, double medOut,
                               MosaicResult &result,
                               std::function<void(int, const std::string &)> progress,
                               std::function<bool()> cancelled,
                               std::string *err = nullptr,
                               int rawNrHint = 0, int rawNaHint = 0);

    static bool flattenTiff(const std::string &inTif, const std::string &outTif, double medOut,
                            const std::vector<int> &seamCols,
                            std::function<void(int, const std::string &)> progress,
                            std::string *err = nullptr);
};

#endif
