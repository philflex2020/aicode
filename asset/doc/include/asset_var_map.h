#ifndef ASSET_VAR_MAP_H
#define ASSET_VAR_MAP_H

#include "asset.h"
#include <map>
#include <string>
#include <shared_mutex>

class AssetVarMap {
public:
    AssetVarMap();
    ~AssetVarMap();

    AssetVar* getAssetVar(const std::string& uri, const std::string& assetId);
    void setAssetVar(const std::string& uri, const std::string& assetId, AssetVar* assetVar);

private:
    std::map<std::string, std::map<std::string, AssetVar*>> assetVarMap;
    mutable std::shared_mutex mutex;
};

#endif // ASSET_VAR_MAP_H