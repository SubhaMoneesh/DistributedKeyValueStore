#include "kv.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <fstream>
#include <random>
#include <cassert>
#include <vector>


std::vector<std::string> node_adds(std::string path) {
    std::vector<std::string> addresses;
    std::ifstream file(path);
    std::string line;
    while(std::getline(file, line)) {
        if (!line.empty()) addresses.push_back(line);
    }
    return addresses;
}


// test a key to be having the same owner from all nodes
void test_same_owner(std::vector<std::unique_ptr<kvstore::KeyValueStore::Stub>>& stubs);
// write in one random node, and read in one random node
void test_functionality(std::vector<std::unique_ptr<kvstore::KeyValueStore::Stub>>& stubs);


int main() {
    std::vector<std::string> addresses = node_adds("cluster.conf");
    std::vector<std::unique_ptr<kvstore::KeyValueStore::Stub>> stubs;
    for(auto& address: addresses) {
        auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
        stubs.push_back(kvstore::KeyValueStore::NewStub(channel));
    }

    test_same_owner(stubs);

    test_functionality(stubs);

    return 0;
}


void test_functionality(
    std::vector<std::unique_ptr<kvstore::KeyValueStore::Stub>>& stubs
) {
    std::mt19937 gen(42);
    std::uniform_int_distribution<int> node_pick(0, stubs.size() - 1);

    int num_keys = 200;

    // choose random stub and put
    // then choose random stub and check getnode
    for (int i = 0; i < num_keys; i++) {
            std::string key = "key_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i);
        {
            auto& stub = stubs[node_pick(gen)];
            grpc::ClientContext ctx;
            kvstore::PutRequest req;
            req.set_key(key);
            req.set_value(value);
            kvstore::PutResponse resp;
            grpc::Status status = stub->Put(&ctx, req, &resp);
            assert(status.ok() && resp.success());
        }
        {
            auto& stub = stubs[node_pick(gen)];
            grpc::ClientContext ctx;
            kvstore::GetRequest req;
            req.set_key(key);
            kvstore::GetResponse resp;
            grpc::Status status = stub->Get(&ctx, req, &resp);
            assert(status.ok() && resp.found() && resp.value() == value);
        }
    }

    std::cout << " [✓] All write/reads (" << num_keys << ") are done finely even from random nodes." 
    << std::endl;
}

void test_same_owner(
    std::vector<std::unique_ptr<kvstore::KeyValueStore::Stub>>& stubs
) {
    std::string key = "Common_key";
    std::string owner;
    for (auto& stub: stubs) {
        kvstore::DebugOwnerRequest req;
        req.set_key(key);
        kvstore::DebugOwnerResponse resp;
        grpc::ClientContext ctx;
        grpc::Status status = stub->DebugOwner(&ctx, req, &resp);
        if(owner.empty()) owner = resp.owner_address();
        assert(status.ok());
        assert(owner == resp.owner_address());
    }
    std::cout << " [✓] All nodes have agreed that same key has same owner." << std::endl;
}