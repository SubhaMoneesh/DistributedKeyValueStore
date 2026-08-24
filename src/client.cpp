#include <grpcpp/grpcpp.h>
#include "kv.grpc.pb.h"
#include <iostream>

int main(int argc, char** argv) {
    // create channel and stub
    std::string target = "localhost:50051";
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = kvstore::KeyValueStore::NewStub(channel);

    // Put
    {
        kvstore::PutRequest req;
        req.set_key("foo");
        req.set_value("bar");
        kvstore::PutResponse resp;
        grpc::ClientContext ctx;

        auto status = stub->Put(&ctx, req, &resp);
        std::cout << "Put success = " << resp.success() << std::endl;
    }

    // Get
    {
        kvstore::GetRequest req;
        kvstore::GetResponse resp;
        grpc::ClientContext ctx;
        req.set_key("foo");

        auto status = stub->Get(&ctx, req, &resp);
        if (resp.found())
            std::cout << "Got " << resp.value() << std::endl;
        else
            std::cout << "Key not found :(" << std::endl;
    }

    return 0;
}