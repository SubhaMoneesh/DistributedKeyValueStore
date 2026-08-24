#include "kv.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include<thread>
#include<vector>
#include<cassert>

using kvstore::PutRequest;
using kvstore::PutResponse;
using kvstore::GetRequest;
using kvstore::GetResponse;
using kvstore::DeleteRequest;
using kvstore::DeleteResponse;

void edge_cases_test(kvstore::KeyValueStore::Stub* stub) {
    // empty key and value (put and get)
    {
        PutRequest req;
        PutResponse resp;
        grpc::ClientContext ctx;
        req.set_key("");
        req.set_value("");
        auto status = stub->Put(&ctx, req, &resp);
        assert(status.ok());
        assert(resp.success());

        GetRequest g_req;
        GetResponse g_resp;
        grpc::ClientContext g_ctx;
        g_req.set_key("");
        status = stub->Get(&g_ctx, g_req, &g_resp);
        assert(status.ok() && g_resp.found() && g_resp.value().empty());
    }    

    // value in string in hexadecimal (put and get)
    {
        PutRequest req;
        PutResponse resp;
        grpc::ClientContext ctx;
        req.set_key("binary_key");
        std::string binary_payload("\0\x01\x02\x00\x03\xFF", 6);
        req.set_value(binary_payload);
        auto status = stub->Put(&ctx, req, &resp);
        assert(status.ok() && resp.success());
                
        GetRequest g_req;
        GetResponse g_resp;
        grpc::ClientContext g_ctx;
        g_req.set_key("binary_key");
        status = stub->Get(&g_ctx, g_req, &g_resp);
        assert(status.ok() && g_resp.found() && g_resp.value() == binary_payload);
    }

    std::cout << " [✓] Edge cases handles safely without server crashes." << std::endl;
}

void concurrency_test(kvstore::KeyValueStore::Stub* stub) {
    std::vector<std::thread> threads;
    int num_t = 10;
    int num_iter = 100;
    std::atomic<int> completed_ops(0);

    for (int t = 0; t < num_t; t++) {
        threads.emplace_back([stub, t, num_iter, &completed_ops](){
            for (int i = 0; i < num_iter; i++) {
                std::string key = "shared_key_" + std::to_string(i%3);
                std::string value = "thread_" + std::to_string(t) + "_iter_" + std::to_string(i);
                // put rpc
                {
                    PutRequest req;
                    PutResponse resp;
                    grpc::ClientContext ctx;
                    req.set_key(key);
                    req.set_value(value);
                    auto status = stub->Put(&ctx, req, &resp);
                    assert(status.ok() && resp.success());
                }

                // get rpc
                {
                    GetRequest req;
                    GetResponse resp;
                    grpc::ClientContext ctx;
                    std::string key = "shared_key_" + std::to_string(i%3);
                    req.set_key(key);
                    auto status = stub->Get(&ctx, req, &resp);
                    assert(status.ok());
                }

                // delete rpc few times
                {
                    DeleteRequest req;
                    DeleteResponse resp;
                    grpc::ClientContext ctx;
                    std::string key = "shared_key_" + std::to_string(i%7);
                    req.set_key(key);
                    auto status = stub->Delete(&ctx, req, &resp);
                    assert(status.ok());
                }
                completed_ops += 3;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << " [✓] Concurrency test passed! Total RPCs executed: " 
              << completed_ops.load() << std::endl;
}

int main(int argc, char** argv) {
    std::string target = "localhost:50051";
    if (argc > 1) target = argv[1];
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = kvstore::KeyValueStore::NewStub(channel);

    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "            STARTING gRPC NETWORK TEST SUITE          " << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;

    edge_cases_test(stub.get());
    concurrency_test(stub.get());

    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "     NETWORK AND CONCURRENCY VERIFICATION PASSED.     " << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;

    return 0;
}