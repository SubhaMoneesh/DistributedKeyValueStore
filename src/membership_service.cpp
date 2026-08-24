#include "membership_service.h"
#include<chrono>

namespace{
uint64_t NowEpochMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
}

std::string ToString(NodeStatus s) {
    switch(s) {
        case NodeStatus::ALIVE: return "Alive";
        case NodeStatus::DEAD: return "Dead";
        case NodeStatus::SUSPECT: return "Suspect";
    }
}

MembershipService::MembershipService(
    std::string self_address,
    const std::vector<std::string>& all_nodes,
    std::chrono::milliseconds suspect_timeout = std::chrono::milliseconds(1500),
    std::chrono::milliseconds dead_timeout = std::chrono::milliseconds(4000)
) 
 :  self_address_(std::move(self_address)), 
    suspect_timeout_(suspect_timeout),
    dead_timeout_(dead_timeout) {
    auto now = std::chrono::steady_clock::now();
    uint64_t my_epoch = NowEpochMs();

    for(auto& node: all_nodes) {
        table_[node] = MemberInfo{
            NodeStatus::ALIVE,
            0,
            0,
            now
        };
    }
}

void MembershipService::IncrementSelfHeartbeat() {
    std::unique_lock lock(mutex_);
    auto& me = table_[self_address_];
    me.heartbeat++;
    me.status = NodeStatus::ALIVE;
    me.last_update = std::chrono::steady_clock::now();
}

void MembershipService::CheckTimeouts() {
    std::unique_lock lock(mutex_);
    for (auto& [addr, info]: table_) {
        if(addr == self_address_) continue;
        auto elapsed = std::chrono::steady_clock::now() - info.last_update;
        if(elapsed > dead_timeout_) info.status = NodeStatus::DEAD;
        else if(elapsed > suspect_timeout_) info.status = NodeStatus::SUSPECT;
        else info.status = NodeStatus::ALIVE;
    }
}

void MembershipService::MergeRemote(const std::vector<GossipEntry>& remote) {
    std::unique_lock lock(mutex_);
    for (auto& memb: remote) {
        if(memb.address == self_address_) continue;
        auto now = std::chrono::steady_clock::now();
        auto it = table_.find(memb.address);
        if (it == table_.end()) {
            table_[memb.address] = MemberInfo{
                NodeStatus::ALIVE,
                memb.start_epoch_ms,
                memb.heartbeat,
                now
            };
            continue;
        }
        auto& info = it->second;
        bool fresher = info.start_epoch_ms < memb.start_epoch_ms
         || (info.start_epoch_ms == memb.start_epoch_ms && 
            info.heartbeat < memb.heartbeat);
        if (fresher) {
            info.heartbeat = memb.heartbeat;
            info.start_epoch_ms = memb.start_epoch_ms;
            info.status = NodeStatus::ALIVE;
            info.last_update = now;
        } 
    }
}

std::vector<GossipEntry> MembershipService::GetSnapshotForGossip() const {
    std::shared_lock lock(mutex_);
    std::vector<GossipEntry> result;
    for (auto& [addr, info]: table_) {
        result.push_back(
            GossipEntry{
                addr,
                info.start_epoch_ms,
                info.heartbeat
            }
        );
    }
    return result;
}

std::vector<MembershipSnapshotEntry> MembershipService::DebugSnapshot() const {
    std::shared_lock lock(mutex_);
    std::vector<MembershipSnapshotEntry> result;
    for (auto& [addr, info]: table_) {
        result.push_back(
            MembershipSnapshotEntry{
                addr,
                info.status,
                info.start_epoch_ms,
                info.heartbeat
            }
        );
    }
    return result;
}

bool MembershipService::isAlive(const std::string& node) const {
    std::shared_lock lock(mutex_);
    auto it = table_.find(node);
    return it != table_.end() && it->second.status == NodeStatus::ALIVE;
}

bool MembershipService::isDead(const std::string& node) const {
    std::shared_lock lock(mutex_);
    auto it = table_.find(node);
    return it != table_.end() && it->second.status == NodeStatus::DEAD;
}

std::vector<std::string> MembershipService::GetAliveNodes() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    for (auto& [addr, info]: table_) {
        if(info.status == NodeStatus::ALIVE)
            result.push_back(addr);
    }
    return result;
}