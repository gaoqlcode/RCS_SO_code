#ifndef BINARYIO_H
#define BINARYIO_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// 小端读写。雷达原始头是 LE，别按本机字节序硬读。

inline uint16_t readU16LE(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

inline uint32_t readU32LE(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

inline float readF32LE(const uint8_t *p)
{
    uint32_t u = readU32LE(p);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

inline int16_t readI16LE(const uint8_t *p)
{
    return static_cast<int16_t>(readU16LE(p));
}

inline void writeU16LE(uint8_t *p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xff);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
}

inline void writeU32LE(uint8_t *p, uint32_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xff);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xff);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xff);
}

inline void writeF32LE(uint8_t *p, float v)
{
    uint32_t u;
    std::memcpy(&u, &v, sizeof(u));
    writeU32LE(p, u);
}

bool readBinFile(const std::string &path, std::vector<uint8_t> &out);
bool writeBinFile(const std::string &path, const std::vector<uint8_t> &data);

#endif
