#include <iostream>
#include "../src/hash_ring.h"
#include <cassert>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>


void test_determinism();
void test_distribution();
void test_disruption();
void test_concurrency();
void test_preference_list();


int main() {
    test_determinism();
    test_distribution();
    test_disruption();
    test_concurrency();
    test_preference_list();
    std::cout << "All tests have passed." << std::endl;
}


void test_determinism() {
    ConsistentHashRing ring;
    for (int i = 1; i <= 6; i++) 
        ring.AddNode("node" + std::to_string(i));
    
    std::string output1 = ring.GetNode("some_input");
    std::string output2 = ring.GetNode("some_input");
    assert(output1 == output2);

    ring.RemoveNode(output1);
    output1 = ring.GetNode("some_input");
    output2 = ring.GetNode("some_input");
    assert(output1 == output2);

    std::cout << "[OK] The Ring is Determistic." << std::endl;
}


void test_distribution() {
    ConsistentHashRing ring;
    for (int i = 1; i <= 6; i++) 
        ring.AddNode("node" + std::to_string(i));
    
    int num_keys = 100000;
    std::map<std::string, int> count;
    for (int j = 1; j <= num_keys; j++) {
        std::string key = "key_" + std::to_string(j);
        count[ring.GetNode(key)]++;
    }

    double expected = num_keys / 6.0;
    for (auto& e: count) {
        double ratio = e.second / expected;
        std::cout << e.first << ": " << e.second << "/" << expected << "(" << ratio << " times the expected)" << std::endl;
        assert(ratio < 1.3 && ratio > 0.7);
    }

    std::cout << "[OK] Distribution is almost uniform." << std::endl;
}


void test_disruption() {
    ConsistentHashRing ring;
    for (int i = 1; i <= 6; i++) 
        ring.AddNode("node" + std::to_string(i));
    
    int num_keys = 10000;
    std::unordered_map<std::string, std::string> before;

    for (int i = 1; i <= num_keys; i++) {
        std::string key = "key_" + std::to_string(i);
        before[key] = ring.GetNode(key);
    }

    ring.RemoveNode("node3");

    int changed = 0;

    for (int i = 1; i <= num_keys; i++) {
        std::string key = "key_" + std::to_string(i);
        if (ring.GetNode(key) != before[key]) changed++;
    }

    double expected = 1.0 / 6.0;
    double reality = static_cast<double>(changed) / num_keys;

    assert(reality < 0.30);

    std::cout << "[OK] The disruption when a node is deleted is " << changed << "/" << num_keys << "(" << reality*100 << " percent), which is minimal." << std::endl;
}

void test_concurrency() {
    ConsistentHashRing ring;
    for (int i = 1; i <= 6; i++) {
        ring.AddNode("node" + std::to_string(i));
    }

    std::atomic<bool> stop(false);
    std::atomic<uint64_t> total_reads(0);
    std::atomic<uint64_t> total_writes(0);

    std::vector<std::thread> threads;

    // 8 Reader threads continuously querying GetNode and GetAllPhysicalNodes
    int num_readers = 8;
    for (int r = 0; r < num_readers; r++) {
        threads.emplace_back([&ring, &stop, &total_reads]() {
            uint64_t local_reads = 0;
            while (!stop.load(std::memory_order_relaxed)) {
                std::string key = "key_" + std::to_string(local_reads % 1000);
                std::string owner = ring.GetNode(key);
                assert(!owner.empty());
                
                if (local_reads % 100 == 0) {
                    auto nodes = ring.GetAllPhysicalNodes();
                    assert(!nodes.empty());
                }
                local_reads++;
            }
            total_reads += local_reads;
        });
    }

    // 4 Writer threads continuously mutating the ring via AddNode and RemoveNode
    int num_writers = 4;
    for (int w = 0; w < num_writers; w++) {
        threads.emplace_back([&ring, &stop, &total_writes, w]() {
            uint64_t local_writes = 0;
            while (!stop.load(std::memory_order_relaxed)) {
                std::string temp_node = "temp_node_" + std::to_string(local_writes % 50);
                ring.AddNode(temp_node);
                ring.RemoveNode(temp_node);
                local_writes += 2;
            }
            total_writes += local_writes;
        });
    }

    // Run the concurrent stress test for 500 milliseconds
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop.store(true, std::memory_order_relaxed);

    for (auto& t : threads) {
        t.join();
    }

    auto nodes = ring.GetAllPhysicalNodes();
    assert(nodes.size() == 6);

    std::cout << "[OK] Concurrency: " << total_reads.load() << " reads and "
              << total_writes.load() << " writes completed safely without data races or deadlocks.\n";
}


void test_preference_list() {
    ConsistentHashRing ring;
    for (int i = 0; i < 6; i++) ring.AddNode("key_" + std::to_string(i));

    // check size and distinctness
    auto pre_list = ring.GetPreferenceList("some_key", 3);
    assert(pre_list.size() == 3);
    assert(pre_list[0] != pre_list[1] && pre_list[0] != pre_list[2] && pre_list[2] != pre_list[1]);

    // check same key gives same list (determinism)
    assert(pre_list == ring.GetPreferenceList("some_key", 3));

    // check GetNode is equal to the first element of list
    assert(pre_list[0] == ring.GetNode("some_key"));

    // check size and distinctness for 5000 keys
    for (int i = 0; i < 5000; i++) {
        auto pref = ring.GetPreferenceList("bulk_key_" + std::to_string(i), 3);
        assert(pref.size() == 3);
        assert(pref[0] != pref[1] && pref[0] != pref[2] && pref[2] != pref[1]);
    }

    std::cout << "[OK] Preference lists are correctly sized, distinct, deterministic.\n";
}