
#ifndef ASSET_H
#define ASSET_H

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstring> // Include cstring for std::strlen

// class AssetVal {
// public:
//     enum class Type {
//         Int,
//         Double,
//         CharPtr,
//         VoidPtr,
//         Bool
//     };

//     union Value {
//         int intValue;
//         double doubleValue;
//         const char* charPtrValue;
//         void* voidPtrValue;
//         bool boolValue;
//     };

//     AssetVal() : type(Type::Int), value{0} {}
//     AssetVal(int val) : type(Type::Int), value{val} {}
//     AssetVal(double val) : type(Type::Double), value{0.0} {
//         if (std::is_same_v<decltype(value.doubleValue), int>) {
//             value.intValue = static_cast<int>(val);
//         } else {
//             value.doubleValue = val;
//         }
//     }
//     AssetVal(const char* val) : type(Type::CharPtr), value{val} {}
//     AssetVal(void* val) : type(Type::VoidPtr), value{val} {}
//     AssetVal(bool val) : type(Type::Bool), value{val} {}

//     ~AssetVal() {}

//     Type getType() const { return type; }

//     template <typename T>
//     T getVal() const {
//         if constexpr (std::is_same_v<T, int>) {
//             return static_cast<T>(value.intValue);
//         } else if constexpr (std::is_same_v<T, double>) {
//             return static_cast<T>(value.doubleValue);
//         } else if constexpr (std::is_same_v<T, const char*>) {
//             return static_cast<T>(value.charPtrValue);
//         } else if constexpr (std::is_same_v<T, void*>) {
//             return static_cast<T>(value.voidPtrValue);
//         } else if constexpr (std::is_same_v<T, bool>) {
//             return static_cast<T>(value.boolValue);
//         } else {
//             throw std::invalid_argument("Unsupported type");
//         }
//     }

//     template <typename T>
//     void setVal(T val) {
//         if constexpr (std::is_same_v<T, int>) {
//             type = Type::Int;
//             value.intValue = static_cast<int>(val);
//         } else if constexpr (std::is_same_v<T, double>) {
//             type = Type::Double;
//             value.doubleValue = static_cast<double>(val);
//         } else if constexpr (std::is_same_v<T, const char*>) {
//             type = Type::CharPtr;
//             value.charPtrValue = static_cast<const char*>(val);
//         } else if constexpr (std::is_same_v<T, void*>) {
//             type = Type::VoidPtr;
//             value.voidPtrValue = static_cast<void*>(val);
//         } else if constexpr (std::is_same_v<T, bool>) {
//             type = Type::Bool;
//             value.boolValue = static_cast<bool>(val);
//         } else {
//             throw std::invalid_argument("Unsupported type");
//         }
//     }

// private:
//     Type type;
//     Value value;
// };

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

    AssetVal() : type(Type::Int) {
        value.intValue = 0;
        value.charPtrValue = nullptr;
        size = 0;

    }
    AssetVal(int val) : type(Type::Int) {
        value.intValue = val;

    }
    AssetVal(double val) : type(Type::Double) {
        value.doubleValue = val;
    }
    AssetVal(const char* val) : type(Type::CharPtr){
        value.charPtrValue = strdup(val);
        size = strlen(val)+1;

    }
    AssetVal(void* val) : type(Type::VoidPtr){
        value.voidPtrValue = val;

    }
    AssetVal(bool val) : type(Type::Bool) {
                value.boolValue = val;
    }



    ~AssetVal() {
        if(type == Type::CharPtr)
        {
            if(value.charPtrValue)
                free ((void*)value.charPtrValue);
        }

    }

    Type getType() const { return type; }

    template <typename T>
    T getVal() const {
        if constexpr (std::is_same_v<T, int>) {
            return static_cast<T>(value.intValue);
        } else if constexpr (std::is_same_v<T, double>) {
            return static_cast<T>(value.doubleValue);
        } else if constexpr (std::is_same_v<T, const char*>) {
            return static_cast<T>(value.charPtrValue);
        } else if constexpr (std::is_same_v<T, void*>) {
            return static_cast<T>(value.voidPtrValue);
        } else if constexpr (std::is_same_v<T, bool>) {
            return static_cast<T>(value.boolValue);
        } else {
            throw std::invalid_argument("Unsupported type");
        }
    }

    template <typename T>
    void setVal(T val) {
        if constexpr (std::is_same_v<T, int>) {
            type = Type::Int;
            value.intValue = static_cast<int>(val);
        } else if constexpr (std::is_same_v<T, double>) {
            type = Type::Double;
            value.doubleValue = static_cast<double>(val);
        } else if constexpr (std::is_same_v<T, const char*>) {
            type = Type::CharPtr;
            if(value.charPtrValue)
                free((void*) value.charPtrValue);
            value.charPtrValue = strdup(static_cast<const char*>(val));
            size = strlen(val);
        } else if constexpr (std::is_same_v<T, void*>) {
            type = Type::VoidPtr;
            value.voidPtrValue = static_cast<void*>(val);
        } else if constexpr (std::is_same_v<T, bool>) {
            type = Type::Bool;
            value.boolValue = static_cast<bool>(val);
        } else {
            throw std::invalid_argument("Unsupported type");
        }
    }

private:
    Type type;
    Value value;
    int size;
};

class AssetVar {
public:
    AssetVar() : uri(""), id(""), options({}), actions({}), params({}) {}
    AssetVar(const std::string& u) : uri(u), id(""), options({}), actions({}), params({}) {}
    AssetVar(const std::string& u, const std::string& i) : uri(u), id(i), options({}), actions({}), params({}) {}
    ~AssetVar() {}

    template <typename T>
    void setVal(T val) {
        assetVal.setVal(val);
    }

    template <typename T>
    T getVal() const {
        return assetVal.getVal<T>();
    }

    template <typename T>
    void setParam(const std::string& paramName, T val) {
        params[paramName].setVal(val);
    }

    template <typename T>
    T getParam(const std::string& paramName) const {
        auto it = params.find(paramName);
        if (it != params.end()) {
            return it->second.getVal<T>();
        } else {
            throw std::invalid_argument("Parameter not found");
        }
    }

private:
    std::string uri;
    std::string id;
    std::vector<std::map<std::string, AssetVal>> options;
    std::map<std::string, std::map<std::string, AssetVal>> actions;
    std::map<std::string, AssetVal> params;
    AssetVal assetVal;
};

#endif // ASSET_H
