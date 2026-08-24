#pragma once
#include <map>
#include <shared_mutex>


class ConsistentHashRing {
public:
    // get, add, getall, delete, static hash
    std::string GetNode(const std::string& key) const;
    std::vector<std::string> GetPreferenceList(const std::string& key, int n) const;
    void AddNode(const std::string& node_address);
    void RemoveNode(const std::string& node_address);
    std::vector<std::string> GetAllPhysicalNodes() const;

    static uint64_t Hash(const std::string& input);

    // constructor
    explicit ConsistentHashRing(int virtual_nodes_per_physical = 150);

private:
    int virtual_nodes_per_physical_;
    std::map<std::uint64_t, std::string> ring_;
    mutable std::shared_mutex mutex_;
};