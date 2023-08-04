#ifndef ASSET_H
#define ASSET_H

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstring> // Include cstring for std::strlen

class AssetVal {
public:
    enum class Type {
        Int,
        Double,
        CharPtr,
        VoidPtr,
        Bool
    };

    union Value {
        int intValue;
        double doubleValue;
        const char* charPtrValue;
        void* voidPtrValue;
        bool boolValue;
    };

    AssetVal();
    AssetVal(int val);
    AssetVal(double val);
    AssetVal(const char* val);
    AssetVal(void* val);
    AssetVal(bool val);

    ~AssetVal();

    Type getType() const;

    template <typename T>
    T getVal() const;

    template <typename T>
    void setVal(T val);

private:
    Type type;
    Value value;
    int size;
};

class AssetVar {
public:
    AssetVar();
    AssetVar(const std::string& u);
    AssetVar(const std::string& u, const std::string& i);
    ~AssetVar();

    template <typename T>
    void setVal(T val);

    template <typename T>
    T getVal() const;

    template <typename T>
    void setParam(const std::string& paramName, T val);

    template <typename T>
    T getParam(const std::string& paramName) const;

private:
    std::string uri;
    std::string id;
    std::vector<std::map<std::string, AssetVal>> options;
    std::map<std::string, std::map<std::string, AssetVal>> actions;
    std::map<std::string, AssetVal> params;
    AssetVal assetVal;
};

#endif // ASSET_H