#include "storage_engine.h"

bool StorageEngine::Put(const std::string& key, const std::string& value, uint64_t timestamp) {
    std::unique_lock lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end() || it->second.timestamp < timestamp) {
        data_[key] = VersionedValue{value, timestamp, false};
        return true;
    }
    return false;
}

std::optional<VersionedValue> StorageEngine::GetVersioned(const std::string& key) const{
    std::shared_lock lock(mutex_);
    auto it = data_.find(key);
    if(it == data_.end())
        return std::nullopt;
    else
        return it->second;
}

bool StorageEngine::Delete(const std::string& key, uint64_t timestamp) {
    std::unique_lock lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end() && it->second.timestamp > timestamp) return false;
    
    data_[key] = VersionedValue{"", timestamp, true};
    return true;
}