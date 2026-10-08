#include "BinaryIo.h"
#include "Util.hpp"

bool readBinFile(const std::string &path, std::vector<uint8_t> &out)
{
    return readFileAll(path, out);
}

bool writeBinFile(const std::string &path, const std::vector<uint8_t> &data)
{
    return writeFileAll(path, data);
}
