#include "rcs/rcs_api.h"
#include <cstdio>
#include <string>

static int gPass = 0, gFail = 0;

static bool check(bool ok, const char *name)
{
    if (ok) {
        ++gPass;
        std::printf("[PASS] %s\n", name);
        return true;
    }
    ++gFail;
    std::fprintf(stderr, "[FAIL] %s\n", name);
    return false;
}

int main()
{
    std::printf("rcs_smoke %s\n", rcsVersion());
    check(rcsVersion() != 0 && std::string(rcsVersion()).size() > 0, "version");
    check(listMosaicTiffs("/nonexistent_folder_xyz").empty(), "list_empty_folder");
    std::printf("\n==== %d passed, %d failed ====\n", gPass, gFail);
    return gFail ? 1 : 0;
}
