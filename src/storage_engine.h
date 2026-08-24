#pragma once
#include<string>
#include<shared_mutex>
#include<optional>
#include<unordered_map>
#include<cstdint>


struct VersionedValue {
    std::string value;
    uint64_t timestamp;
    bool is_tombstone;
};


class StorageEngine {
    public:
        bool Put(const std::string& key, const std::string& value, uint64_t timestamp);
        std::optional<VersionedValue> GetVersioned(const std::string& key) const;
        bool Delete(const std::string& key, uint64_t timestamp);
    
    private:
        mutable std::shared_mutex mutex_;
        std::unordered_map<std::string, VersionedValue> data_;
};