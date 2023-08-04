#include "asset_var_map.h"

AssetVarMap::AssetVarMap() {}

AssetVarMap::~AssetVarMap() {
    // Clean up the AssetVar objects when the map is destroyed
    for (auto& entry : assetVarMap) {
        for (auto& innerEntry : entry.second) {
            delete innerEntry.second;
        }
    }
}

AssetVar* AssetVarMap::getAssetVar(const std::string& uri, const std::string& assetId) {
    std::shared_lock lock(mutex);
    auto uriIter = assetVarMap.find(uri);
    if (uriIter != assetVarMap.end()) {
        auto assetVarIter = uriIter->second.find(assetId);
        if (assetVarIter != uriIter->second.end()) {
            return assetVarIter->second;
        }
    }
    return nullptr;
}

void AssetVarMap::setAssetVar(const std::string& uri, const std::string& assetId, AssetVar* assetVar) {
    std::unique_lock lock(mutex);
    assetVarMap[uri][assetId] = assetVar;
}