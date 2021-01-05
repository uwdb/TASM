#ifndef LIGHTDB_TILEUTILITIES_H
#define LIGHTDB_TILEUTILITIES_H

#include "Files.h"

static void DeleteTiles(const std::string &catalogEntryName) {
    static std::string basePath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/";
    auto catalogPath = basePath + catalogEntryName;
    for (auto &dir : std::filesystem::directory_iterator(catalogPath)) {
        if (!dir.is_directory() || dir.path().filename() == lightdb::catalog::TmpTileFiles::tmp())
            continue;

        auto dirName = dir.path().filename().string();
        auto lastSep = dirName.rfind("-");
        auto tileVersion = std::stoul(dirName.substr(lastSep + 1));
        if (!tileVersion)
            continue;
        else
            std::filesystem::remove_all(dir.path());
    }
}

static void DeleteTilesPastNum(const std::string &catalogEntryName, unsigned int maxOriginalTileNum) {
    static std::string basePath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/";
    auto catalogPath = basePath + catalogEntryName;

    for (auto &dir : std::filesystem::directory_iterator(catalogPath)) {
        if (!dir.is_directory())
            continue;

        auto dirName = dir.path().filename().string();
        auto lastSep = dirName.rfind("-");
        auto tileVersion = std::stoul(dirName.substr(lastSep + 1));
        if (tileVersion > maxOriginalTileNum)
            std::filesystem::remove_all(dir.path());
    }
}

static void ResetTileNum(const std::string &catalogEntryName, unsigned int maxOriginalTileNum=0) {
    static std::string basePath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/";
    auto tilePath = basePath + catalogEntryName + "/tile-version";

    std::ofstream tileVersion(tilePath);
    tileVersion << maxOriginalTileNum + 1;
    tileVersion.close();
}

#endif //LIGHTDB_TILEUTILITIES_H
