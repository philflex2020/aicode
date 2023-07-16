#pragma once

// CommandArgs
// p wilshire
// 07_16_2023

#include <iostream>
#include <string>
#include <vector>
#include <map>

class CommandLineArgs {
public:
    void parse(int argc, char* argv[]) {
        extra = "";
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg.substr(0, 2) == "--") {
                std::string key = arg.substr(2);
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    std::string value = argv[i + 1];
                    args[key] = value;
                    i++;
                } else {
                    args[key] = "";
                }
            }
            else if (arg.substr(0, 1) == "-") {
                std::string key = arg.substr(1);
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    std::string value = argv[i + 1];
                    args[key] = value;
                    i++;
                } else {
                    args[key] = "";
                }
            }
            else {
                extra = arg;
            }
        }
    }

    std::string getExtra() const {
        return extra;
    }
    
    std::string getValue(const std::string& key) const {
        auto it = args.find(key);
        if (it != args.end()) {
            return it->second;
        }
        return "";
    }

    std::string getValue(const std::string& key,const std::string& opt) const {
        auto it = args.find(key);
        if (it != args.end()) {
            return it->second;
        }
        it = args.find(opt);
        if (it != args.end()) {
            return it->second;
        }
        return "";
    }

    std::string getValue(const std::string& key,const std::string& opt, const std::string& def) const {
        auto res = getValue(key,opt);
        if (res.empty()) 
            return def;
        return res;
    }

    bool hasValue(const std::string& key) const {
        return args.find(key) != args.end();
    }

    bool hasValue(const std::string& key, const std::string& opt) const {
        auto ikey = args.find(key);// != args.end();
        auto iopt = args.find(opt);// != args.end();
        if (ikey != args.end())
        {
            return true;
        }
        if (iopt != args.end())
        {
            return true;
        }
        return false;
    }

private:
    std::map<std::string, std::string> args;
    std::string extra;
};
