#include "kv.grpc.pb.h"
#include "grpcpp/grpcpp.h"
#include <fstream>


// create function to read addresses from config file path and return vector of addresses
std::vector<std::string> ReadAddresses(const std::string& path) {
    std::vector<std::string> addrs;
    std::ifstream file(path);
    std::string line;
    while(std::getline(file, line)) 
        if(!line.empty())
            addrs.push_back(line);
    return addrs;
}


// main function
int main() {

    // get addresses vector
    auto addresses = ReadAddresses("cluster.conf");

    // create vector of stubs to all those adresses
    std::vector<std::unique_ptr<kvstore::KeyValueStore::Stub>> stubs;
    for (auto& addr: addresses)
        stubs.push_back(kvstore::KeyValueStore::NewStub(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())));

    // do some number of iterations
    int num_iter = 100;
    for(int i = 0; i < num_iter; i++) {

        // create a key and value
        std::string key = "key_repl_" + std::to_string(i);
        std::string value = "value_repl_" + std::to_string(i);

        // put that key-value pair into the cluster through one of the servers
        {
            grpc::ClientContext ctx;
            kvstore::PutRequest req;
            kvstore::PutResponse resp;
            req.set_key(key);
            req.set_value(value);
            auto status = stubs[0]->Put(&ctx, req, &resp);
            assert(status.ok() && resp.success());
        }

        // get the pref list for that key and check that its size is 3
        kvstore::DebugPreferenceListResponse presp;
        {
            kvstore::DebugPreferenceListRequest req;
            grpc::ClientContext ctx;
            req.set_key(key);
            auto status = stubs[0]->DebugPreferenceList(&ctx, req, &presp);
            assert(status.ok());
        }
        assert(presp.nodes_size() == 3);

        // check for each server or node in the pref list 
        // that thier local storage has the data pair
        for (auto& node: presp.nodes()) {
            kvstore::DebugLocalGetRequest req;
            kvstore::DebugLocalGetResponse resp;
            grpc::ClientContext ctx;
            req.set_key(key);
            int ind = -1;
            for (size_t i = 0; i < addresses.size(); i++) {
                if(addresses[i] == node){
                    ind = i;
                    break;
                }
            }
            assert(ind!=-1);
            auto status = stubs[ind]->DebugLocalGet(&ctx, req, &resp);
            assert(status.ok() && resp.exists() && !resp.is_tombstone() && resp.value() == value);
        }

        // check if we get the pair through random node (just for checking)
        {
            auto& stub = stubs[(i + 1) % stubs.size()];
            kvstore::GetRequest req;
            kvstore::GetResponse resp;
            grpc::ClientContext ctx;
            req.set_key(key);
            auto status = stub->Get(&ctx, req, &resp);
            assert(status.ok() && resp.found() && resp.value() == value);
        }
    }
    // output all cases succeeded
    std::cout << "[OK] " << num_iter
              << " keys correctly replicated to all 3 owners and read via quorum." << std::endl;
    return 0;
}