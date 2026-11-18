#pragma once
// Host-only Preferences/Serial adapters for the production persistence scheduler.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct SerialStub { void println(const char*) {} };
inline SerialStub Serial;
class Preferences {
public:
    inline static std::map<std::string,std::vector<uint8_t>> data;
    inline static unsigned writes=0;
    inline static bool fail=false;
    bool begin(const char*,bool) { return true; }
    bool isKey(const char* k) const { return data.count(k)!=0; }
    size_t getBytesLength(const char* k) const { return data.at(k).size(); }
    size_t getBytes(const char* k,void* p,size_t n) const {
        if (!isKey(k) || data.at(k).size()!=n) return 0;
        std::memcpy(p,data.at(k).data(),n); return n;
    }
    size_t putBytes(const char* k,const void* p,size_t n) {
        ++writes;
        if (fail) return 0;
        auto* first=static_cast<const uint8_t*>(p);
        data[k]={first,first+n}; return n;
    }
};
