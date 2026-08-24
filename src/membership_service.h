#pragma once
#include<string>
#include<vector>
#include<chrono>
#include<shared_mutex>

enum class NodeStatus { ALIVE, SUSPECT, DEAD };

std::string ToString(NodeStatus s);

struct GossipEntry {
    std::string address;
    uint64_t start_epoch_ms;
    uint64_t heartbeat;
};

struct MembershipSnapshotEntry {
    std::string address;
    NodeStatus status;
    uint64_t start_epoch_ms;
    uint64_t heartbeat;
};

class MembershipService {
    public:
        MembershipService(std::string self_address, 
            const std::vector<std::string>& all_nodes,
            std::chrono::milliseconds suspect_timeout = std::chrono::milliseconds(1500),
            std::chrono::milliseconds dead_timeout = std::chrono::milliseconds(4000)
        );

        void IncrementSelfHeartbeat();
        void CheckTimeouts();
        void MergeRemote(const std::vector<GossipEntry>& remote);

        std::vector<GossipEntry> GetSnapshotForGossip() const;
        std::vector<MembershipSnapshotEntry> DebugSnapshot() const;

        bool isAlive(const std::string& node) const;
        bool isDead(const std::string& node) const;
        bool isSuspect(const std::string& node) const;
        std::vector<std::string> GetAliveNodes() const;

    private:
        struct MemberInfo {
            NodeStatus status;
            uint64_t start_epoch_ms;
            uint64_t heartbeat;
            std::chrono::steady_clock::time_point last_update;
        };
        std::string self_address_;
        std::chrono::milliseconds suspect_timeout_;
        std::chrono::milliseconds dead_timeout_;
        mutable std::shared_mutex mutex_;
        std::unordered_map<std::string, MemberInfo> table_;
};