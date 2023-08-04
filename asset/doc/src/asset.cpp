#include "asset.h"
#include <cstring>

AssetVal::AssetVal() : type(Type::Int) {
    value.intValue = 0;
    value.charPtrValue = nullptr;
    size = 0;
}

AssetVal::AssetVal(int val) : type(Type::Int) {
    value.intValue = val;
}

AssetVal::AssetVal(double val) : type(Type::Double) {
    value.doubleValue = val;
}

AssetVal::AssetVal(const char* val) : type(Type::CharPtr) {
    value.charPtrValue = strdup(val);
    size = strlen(val) + 1;
}

AssetVal::AssetVal(void* val) : type(Type::VoidPtr) {
    value.voidPtrValue = val;
}

AssetVal::AssetVal(bool val) : type(Type::Bool) {
    value.boolValue = val;
}

AssetVal::~AssetVal() {
    if (type == Type::CharPtr) {
        if (value.charPtrValue)
            free((void*)value.charPtrValue);
    }
}

AssetVal::Type AssetVal::getType() const {
    return type;
}

template <>
int AssetVal::getVal<int>() const {
    if (type != Type::Int)
        throw std::invalid_argument("AssetVal is not of type Int");
    return value.intValue;
}

template <>
double AssetVal::getVal<double>() const {
    if (type != Type::Double)
        throw std::invalid_argument("AssetVal is not of type Double");
    return value.doubleValue;
}

template <>
const char* AssetVal::getVal<const char*>() const {
    if (type != Type::CharPtr)
        throw std::invalid_argument("AssetVal is not of type CharPtr");
    return value.charPtrValue;
}

template <>
void* AssetVal::getVal<void*>() const {
    if (type != Type::VoidPtr)
        throw std::invalid_argument("AssetVal is not of type VoidPtr");
    return value.voidPtrValue;
}

template <>
bool AssetVal::getVal<bool>() const {
    if (type != Type::Bool)
        throw std::invalid_argument("AssetVal is not of type Bool");
    return value.boolValue;
}

template <>
void AssetVal::setVal<int>(int val) {
    type = Type::Int;
    value.intValue = val;
}

template <>
void AssetVal::setVal<double>(double val) {
    type = Type::Double;
    value.doubleValue = val;
}

template <>
void AssetVal::setVal<const char*>(const char* val) {
    type = Type::CharPtr;
    if (value.charPtrValue)
        free((void*)value.charPtrValue);
    value.charPtrValue = strdup(val);
    size = strlen(val) + 1;
}

template <>
void AssetVal::setVal<void*>(void* val) {
    type = Type::VoidPtr;
    value.voidPtrValue = val;
}

template <>
void AssetVal::setVal<bool>(bool val) {
    type = Type::Bool;
    value.boolValue = val;
}

AssetVar::AssetVar() : uri(""), id(""), options({}), actions({}), params({}) {}

AssetVar::AssetVar(const std::string& u) : uri(u), id(""), options({}), actions({}), params({}) {}

AssetVar::AssetVar(const std::string& u, const std::string& i) : uri(u), id(i), options({}), actions({}), params({}) {}

AssetVar::~AssetVar() {}

template <>
void AssetVar::setVal<int>(int val) {
    assetVal.setVal(val);
}

template <>
int AssetVar::getVal<int>() const {
    return assetVal.getVal<int>();
}

template <>
void AssetVar::setVal<double>(double val) {
    assetVal.setVal(val);
}

template <>
double AssetVar::getVal<double>() const {
    return assetVal.getVal<double>();
}

template <>
void AssetVar::setVal<const char*>(const char* val) {
    assetVal.setVal(val);
}

template <>
const char* AssetVar::getVal<const char*>() const {
    return assetVal.getVal<const char*>();
}

template <>
void AssetVar::setVal<void*>(void* val) {
    assetVal.setVal(val);
}

template <>
void* AssetVar::getVal<void*>() const {
    return assetVal.getVal<void*>();
}

template <>
void AssetVar::

setVal<bool>(bool val) {
    assetVal.setVal(val);
}

template <>
bool AssetVar::getVal<bool>() const {
    return assetVal.getVal<bool>();
}

template <>
void AssetVar::setParam<int>(const std::string& paramName, int val) {
    params[paramName].setVal(val);
}

template <>
int AssetVar::getParam<int>(const std::string& paramName) const {
    auto it = params.find(paramName);
    if (it != params.end()) {
        return it->second.getVal<int>();
    } else {
        throw std::invalid_argument("Parameter not found");
    }
}

template <>
void AssetVar::setParam<double>(const std::string& paramName, double val) {
    params[paramName].setVal(val);
}

template <>
double AssetVar::getParam<double>(const std::string& paramName) const {
    auto it = params.find(paramName);
    if (it != params.end()) {
        return it->second.getVal<double>();
    } else {
        throw std::invalid_argument("Parameter not found");
    }
}

template <>
void AssetVar::setParam<const char*>(const std::string& paramName, const char* val) {
    params[paramName].setVal(val);
}

template <>
const char* AssetVar::getParam<const char*>(const std::string& paramName) const {
    auto it = params.find(paramName);
    if (it != params.end()) {
        return it->second.getVal<const char*>();
    } else {
        throw std::invalid_argument("Parameter not found");
    }
}

template <>
void AssetVar::setParam<void*>(const std::string& paramName, void* val) {
    params[paramName].setVal(val);
}

template <>
void* AssetVar::getParam<void*>(const std::string& paramName) const {
    auto it = params.find(paramName);
    if (it != params.end()) {
        return it->second.getVal<void*>();
    } else {
        throw std::invalid_argument("Parameter not found");
    }
}

template <>
void AssetVar::setParam<bool>(const std::string& paramName, bool val) {
    params[paramName].setVal(val);
}

template <>
bool AssetVar::getParam<bool>(const std::string& paramName) const {
    auto it = params.find(paramName);
    if (it != params.end()) {
        return it->second.getVal<bool>();
    } else {
        throw std::invalid_argument("Parameter not found");
    }
}