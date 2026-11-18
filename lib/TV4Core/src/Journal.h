#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace tv4 {
// Two independent NVS keys per record. A new generation becomes active only
// after the complete, checksummed blob has been written and read back.
class BlobStore {
public:
    virtual ~BlobStore() = default;
    virtual size_t length(const char* key)=0;
    virtual bool read(const char* key,void* data,size_t size)=0;
    virtual bool write(const char* key,const void* data,size_t size)=0;
};
inline uint32_t crc32(const uint8_t* data,size_t size) {
    uint32_t value=0xffffffff;
    for (size_t i=0;i<size;++i) {
        value^=data[i];
        for (int bit=0;bit<8;++bit)
            value=(value>>1) ^ (0xedb88320u & (0u-(value&1u)));
    }
    return ~value;
}
enum class Load { Empty, Valid, Recovered, Damaged };
template<class T> class Journal {
    static_assert(std::is_trivially_copyable<T>::value);
    using Bytes=std::array<uint8_t,12+sizeof(T)>;
public:
    Journal(BlobStore& store,const char* a,const char* b,uint32_t schema) :
        store_(store),keys_{a,b},schema_(schema) {}
    Load load(T& value) {
        bool any=false, invalid=false;
        active_=-1; generation_=0;
        for (int slot=0;slot<2;++slot) {
            const auto size=store_.length(keys_[slot]);
            if (!size) continue;
            any=true;
            T payload{}; uint32_t seq=0;
            if (size!=Bytes{}.size() || !decode(slot,payload,seq)) { invalid=true; continue; }
            const uint32_t difference=seq-generation_;
            if (active_<0 || (difference && difference<0x80000000u)) {
                active_=slot; generation_=seq; value=payload;
            }
        }
        if (active_>=0) return invalid ? Load::Recovered : Load::Valid;
        return any ? Load::Damaged : Load::Empty;
    }
    bool save(const T& value) {
        if (!value.valid()) return false;
        Bytes bytes{};
        const uint32_t next=generation_+1;
        put(bytes,0,schema_); put(bytes,4,next);
        std::memcpy(bytes.data()+8,&value,sizeof(value));
        put(bytes,bytes.size()-4,crc32(bytes.data(),bytes.size()-4));
        const int slot=active_==0 ? 1 : 0;
        if (!store_.write(keys_[slot],bytes.data(),bytes.size())) return false;
        Bytes check{};
        if (!store_.read(keys_[slot],check.data(),check.size()) || bytes!=check) return false;
        active_=slot; generation_=next;
        return true;
    }
private:
    static void put(Bytes& b,size_t pos,uint32_t n) { std::memcpy(b.data()+pos,&n,4); }
    static uint32_t get(const Bytes& b,size_t pos) { uint32_t n; std::memcpy(&n,b.data()+pos,4); return n; }
    bool decode(int slot,T& value,uint32_t& seq) {
        Bytes b{};
        if (!store_.read(keys_[slot],b.data(),b.size()) || get(b,0)!=schema_ ||
            get(b,b.size()-4)!=crc32(b.data(),b.size()-4)) return false;
        seq=get(b,4); std::memcpy(&value,b.data()+8,sizeof(value));
        return value.valid();
    }
    BlobStore& store_;
    const char* keys_[2];
    uint32_t schema_, generation_=0;
    int active_=-1;
};
}
