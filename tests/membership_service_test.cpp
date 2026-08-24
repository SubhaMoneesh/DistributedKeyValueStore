#include "membership_service.h"
#include <cassert>
#include <iostream>
#include <thread>

// test initial status as alive
void test_initial_status() {
    MembershipService m("nodeA", {"nodeA", "nodeB", "nodeC"});
    assert(m.isAlive("nodeB"));
    assert(m.isAlive("nodeC"));
    std::cout << "[OK] All peers start as ALIVE during the initial grace period.\n";
}

// test self heartbeat increments
void test_self_heartbeat_increments() {
    MembershipService m("nodeA", {"nodeA", "nodeB", "nodeC"});
    m.IncrementSelfHeartbeat();
    m.IncrementSelfHeartbeat();
    for(auto& i: m.DebugSnapshot()) {
        if(i.address == "nodeA") assert(i.heartbeat == 2);
    }
    std::cout << "[OK] Self heartbeat counter increments correctly.\n";
}

// test merge prefers higher heartbeat and even more  higher start epoch
void test_merge_preference() {
    MembershipService m("nodeA", {"nodeA", "nodeB", "nodeC"});
    m.MergeRemote({{"nodeB", 3, 3}});
    m.MergeRemote({{"nodeB", 3, 2}});
    for(auto& i: m.DebugSnapshot()) {
        if(i.address == "nodeB") assert(i.heartbeat == 3 && i.start_epoch_ms == 3);
    }
    m.MergeRemote({{"nodeB", 4, 0}});
    for(auto& i: m.DebugSnapshot()) {
        if(i.address == "nodeB") assert(i.heartbeat == 0 && i.start_epoch_ms == 4);
    }
    m.MergeRemote({{"nodeB", 2, 10}});
    for(auto& i: m.DebugSnapshot()) {
        if(i.address == "nodeB") assert(i.heartbeat == 0 && i.start_epoch_ms == 4);
    }
    std::cout << "[OK] Merge correctly ignores stale heartbeats and accepts fresher ones.\n";
}

// test timeout function
void test_timeout() {
    using namespace std::chrono_literals;
    MembershipService m("nodeA", {"nodeA", "nodeB", "nodeC"}, 150ms, 300ms);
    std::this_thread::sleep_for(150ms);
    m.CheckTimeouts();
    assert(m.isSuspect("nodeB") && m.isSuspect("nodeC"));
    std::this_thread::sleep_for(150ms);
    m.CheckTimeouts();
    assert(m.isDead("nodeB") && m.isDead("nodeC"));
    m.MergeRemote({{"nodeB", 2, 2}});
    assert(m.isDead("nodeC"));
    assert(m.isAlive("nodeB"));
    std::cout << "[OK] ALIVE -> SUSPECT -> DEAD -> ALIVE transitions work correctly.\n";
}

// test get all alive after check timeouts
void test_all_alive() {
    using namespace std::chrono_literals;
    MembershipService m("nodeA", {"nodeA", "nodeB", "nodeC"}, 150ms, 300ms);
    auto a = m.GetAliveNodes();
    assert(std::find(a.begin(), a.end(), "nodeB") != a.end());
    std::this_thread::sleep_for(300ms);
    m.CheckTimeouts();
    a = m.GetAliveNodes();
    assert(std::find(a.begin(), a.end(), "nodeB") == a.end());
    assert(std::find(a.begin(), a.end(), "nodeA") != a.end());
    assert(std::find(a.begin(), a.end(), "nodeC") == a.end());
    std::cout << "[OK] GetAliveNodes correctly filters out dead peers.\n";
}

int main() {
    std::cout << "----------------------------------------------------------\n";
    std::cout << "        RUNNING MEMBERSHIP SERVICE ISOLATION TESTS        \n";
    std::cout << "----------------------------------------------------------\n";
    test_initial_status();
    test_self_heartbeat_increments();
    test_merge_preference();
    test_timeout();
    test_all_alive();
    std::cout << "----------------------------------------------------------\n";
    std::cout << "          ALL VERIFICATIONS PASSED SUCCESSFULLY!          \n";
    std::cout << "----------------------------------------------------------\n";
    return 0;
}