#include "hash_ring.h"
#include <set>

uint64_t ConsistentHashRing::Hash(const std::string& input) {
    uint64_t hash = 14695981039346656037ULL;     // fnv offset basis
    for (unsigned char c: input) {
        hash ^= c;
        hash *= 1099511628211ULL;    // fnv prime
    }
    
    // MurmurHash3 finalizer (avalanche mix)
    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= hash >> 33;

    return hash;
}


ConsistentHashRing::ConsistentHashRing(int virtual_nodes_per_physical) : virtual_nodes_per_physical_(virtual_nodes_per_physical) {}


std::string ConsistentHashRing::GetNode(const std::string& key) const{
    std::shared_lock lock(mutex_);
    if (ring_.empty())
        return "";
    uint64_t h = Hash(key);
    auto it = ring_.lower_bound(h);
    if (it == ring_.end())
        it = ring_.begin();
    return it->second;
}


void ConsistentHashRing::AddNode(const std::string& node_address) {
    std::unique_lock lock(mutex_);
    for (int i = 0; i < virtual_nodes_per_physical_; i++) {
        std::string v_address = node_address + "#" + std::to_string(i);
        ring_[Hash(v_address)] = node_address;
    }   
}


void ConsistentHashRing::RemoveNode(const std::string& node_address) {
    std::unique_lock lock(mutex_);
    for (int i = 0; i < virtual_nodes_per_physical_; i++) {
        std::string v_address = node_address + "#" + std::to_string(i);
        ring_.erase(Hash(v_address));
    }
}


std::vector<std::string> ConsistentHashRing::GetAllPhysicalNodes() const {
    std::shared_lock lock(mutex_);
    std::set<std::string> unique_nodes;
    for (const auto& e: ring_) {
        unique_nodes.insert(e.second);
    }
    return std::vector<std::string>(unique_nodes.begin(), unique_nodes.end());
}


std::vector<std::string> ConsistentHashRing::GetPreferenceList(const std::string& key, int n) const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    if(ring_.empty()) return result;

    auto it = ring_.lower_bound(Hash(key));
    size_t visited = 0;
    std::set<std::string> next_nds;
    
    while (result.size() < static_cast<size_t>(n) && visited < ring_.size()) {
        if (it == ring_.end()) it = ring_.begin();
        if(next_nds.insert(it->second).second) {
            result.push_back(it->second);
        }
        it++;
        visited++;
    }
    return result;
}
    