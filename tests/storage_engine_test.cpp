#include "../src/storage_engine.h"
#include<cassert>
#include<iostream>
#include<thread>


void test_functionality() {
    StorageEngine engine;

    // Get missing key gives nullopt
    assert(engine.GetVersioned("missing_string") == std::nullopt);

    // put
    assert(engine.Put("num_nodes", "6 nodes", 100) == true);
    auto val = engine.GetVersioned("num_nodes");
    assert(val.has_value());
    assert(val->value == "6 nodes" && val->timestamp == 100 && !val->is_tombstone);

    //old write vs new write
    assert(!engine.Put("num_nodes", "7 nodes", 99));
    val = engine.GetVersioned("num_nodes");
    assert(val->value == "6 nodes" && val->timestamp == 100);
    assert(engine.Put("num_nodes", "7 nodes", 101));
    val = engine.GetVersioned("num_nodes");
    assert(val->value == "7 nodes" && val->timestamp == 101);

    
    //new delete vs old delete
    assert(engine.Delete("num_nodes", 102));
    val = engine.GetVersioned("num_nodes");
    assert(val->is_tombstone);
    assert(!engine.Delete("num_nodes", 99));

    // old write vs new write
    assert(!engine.Put("num_nodes", "3 nodes", 98));
    val = engine.GetVersioned("num_nodes");
    assert(val->is_tombstone);
    assert(engine.Put("num_nodes", "10 nodes", 150));
    val = engine.GetVersioned("num_nodes");
    assert(val->value == "10 nodes" && !val->is_tombstone);

    std::cout << "[✓] Functional validation passed successfully." << std::endl;
}


void test_concurrency() {
    StorageEngine engine;

    // fix number of threads and iterations in each thread
    int num_t = 5;
    int num_iter = 200;

    // create a vector of threads
    std::vector<std::thread> threads;

    std::cout << "Launching " << num_t << " threads to hammer overlapping keys" << std::endl;

    // create each thread and pass it on to the threads vector using loop
    for (int i = 0; i < num_t; i++) {
        threads.emplace_back([&engine, num_iter, i] () {
            for(int j = 0; j < num_iter; j++) {
                // create a key and value
                std::string key = "shared_key_" + std::to_string(j % 5);
                std::string value = "thread_" + std::to_string(i) + "_iter_" + std::to_string(j);
                uint64_t ts = 100000 + j;                
                // put a key value
                engine.Put(key, value, ts);

                // get it
                auto res = engine.GetVersioned(key);

                // delete it every 7 th iteration
                if(j % 7 == 0)
                    engine.Delete(key, ts);
            }
        });
    }

    // join all threads one by one using loop
    for (auto& t: threads)
        t.join();
    
    std::cout << "[✓] Concurrency validation passed successfully." << std::endl;
}


int main() {
    std::cout << "----------------------------------------------------------" << std::endl;
    std::cout << "         RUNNING STORAGE ENGINE ISOLATION TESTS           " << std::endl;
    std::cout << "----------------------------------------------------------" << std::endl;

    test_functionality();
    test_concurrency();

    std::cout << "----------------------------------------------------------" << std::endl;
    std::cout << "          ALL VERIFICATIONS PASSED SUCCESSFULLY!          " << std::endl;
    std::cout << "----------------------------------------------------------" << std::endl;

    return 0;
}