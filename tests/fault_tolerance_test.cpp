#include "kv.grpc.pb.h"
#include "grpcpp/grpcpp.h"
#include <thread>
#include <fstream>

// method that takes address and returns port number
std::string PortOf(const std::string& address) {
    return address.substr(address.find(":") + 1);
}

// method that takes the address and deletes server on that port
void Kill(const std::string& address) {
    std::string cmd = "./scripts/kill_node.sh " + PortOf(address);
    std::system(cmd.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void Restart() {
    std::string cmd = "./scripts/stop_cluster.sh";
    std::system(cmd.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cmd = "./scripts/start_cluster.sh";
    std::system(cmd.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

// method that takes addresses vector and dead addresses vector 
// and returns the first alive address
std::string PickAlive(std::vector<std::string>& addresses, std::vector<std::string>& dead) {
    for (auto& a: addresses) 
        if (std::find(dead.begin(), dead.end(), a) == dead.end())
            return a;
    return addresses.front();
}

// method that takes config file path and returns vector of addresses
std::vector<std::string> ReadAddresses(const std::string& path) {
    std::vector<std::string> addrs;
    std::ifstream file(path);
    std::string line;
    while(std::getline(file, line)) 
        if(!line.empty())
            addrs.push_back(line);
    return addrs;
}

// main method
int main() {

    // create addresses vector
    auto addresses = ReadAddresses("cluster.conf");

    // create stubs for those addresses
    std::vector<std::unique_ptr<kvstore::KeyValueStore::Stub>> stubs;
    for (auto& addr: addresses) {
        stubs.push_back(kvstore::KeyValueStore::NewStub(
            grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())
        ));
    }

    // create a lambda function that takes the address and returns the corresponding stub pointer
    auto StubFor = [&] (const std::string& addr) -> kvstore::KeyValueStore::Stub* {
        for (size_t i = 0; i < addresses.size(); i++)
            if (addresses[i] == addr)
                return stubs[i].get();
        return nullptr;
    };
    
    int num_iter = 2;

    for (int z = 0; z < addresses.size(); z++) {
        for (int y = 0; y < num_iter; y++) {

            std::cout << "\nNode_" + std::to_string(z) + "_iter_" + std::to_string(y)
            << std::endl;
            Restart();

            // create a key and value for read, put and delete rpcs
            std::string key = "fault_tolerant_key_" + std::to_string(y);
            std::string value = "fault_tolerant_value_" + std::to_string(y);

            // get the preference list of nodes for that key
            kvstore::DebugPreferenceListResponse presp;
            {
                kvstore::DebugPreferenceListRequest req;
                grpc::ClientContext ctx;
                req.set_key(key);
                auto status = stubs[z]->DebugPreferenceList(&ctx, req, &presp);
                assert(status.ok());
            }
            std::cout << "preference list: ";
            for(auto& name: presp.nodes())
                std::cout << name << " ";
            std::cout << std::endl;
            // put the key and value into the cluster
            {
                kvstore::PutRequest req;
                kvstore::PutResponse resp;
                grpc::ClientContext ctx;
                req.set_key(key);
                req.set_value(value);
                auto status = stubs[z]->Put(&ctx, req, &resp);
                assert(status.ok() && resp.success());
            }
            std::cout << "[OK] Write succeeded with 3/3 replicas alive." << std::endl;

            // delete one node from the pref list and create vector of dead nodes
            Kill(presp.nodes(0));
            std::vector<std::string> dead = {presp.nodes(0)};

            // check if the get using alive node works on that key
            {
                kvstore::GetRequest req;
                kvstore::GetResponse resp;
                grpc::ClientContext ctx;
                req.set_key(key);
                auto coordinator = PickAlive(addresses, dead);
                auto status = StubFor(coordinator)->Get(&ctx, req, &resp);
                assert(status.ok() && resp.found() && resp.value() == value);
            }
            std::cout << "[OK] Read succeeded with 2/3 replicas alive (quorum of 2 met)." << std::endl;

            // check if the delete using alive node works on that key
            {
                kvstore::DeleteRequest req;
                kvstore::DeleteResponse resp;
                grpc::ClientContext ctx;
                req.set_key(key);
                auto coordinator = PickAlive(addresses, dead);
                auto status = StubFor(coordinator)->Delete(&ctx, req, &resp);
                assert(status.ok() && resp.success());
            }
            std::cout << "[OK] Delete succeeded with 2/3 replicas alive (quorum of 2 met)." << std::endl;

            // check if the put using alive node works on that key
            {
                kvstore::PutRequest req;
                kvstore::PutResponse resp;
                grpc::ClientContext ctx;
                req.set_key(key);
                req.set_value(value);
                auto coordinator = PickAlive(addresses, dead);
                auto status = StubFor(coordinator)->Put(&ctx, req, &resp);
                assert(status.ok() && resp.success());
            }
            std::cout << "[OK] Put succeeded with 2/3 replicas alive (quorum of 2 met)." << std::endl;

            // delete the next node from the pref list
            Kill(presp.nodes(1));
            dead.push_back(presp.nodes(1));

            // check if the get using alive node works on that key
            {
                kvstore::GetRequest req;
                kvstore::GetResponse resp;
                grpc::ClientContext ctx;
                req.set_key(key);
                auto coordinator = PickAlive(addresses, dead);
                auto status = StubFor(coordinator)->Get(&ctx, req, &resp);
                assert(!status.ok());
            }
            std::cout << "[OK] Read correctly failed with only 1/3 replicas alive "
                        "(quorum of 2 NOT met — this is expected, not a bug)." << std::endl;

            // check if the delete using alive node works on that key
            {
                kvstore::DeleteRequest req;
                kvstore::DeleteResponse resp;
                grpc::ClientContext ctx;
                req.set_key(key);
                auto coordinator = PickAlive(addresses, dead);
                auto status = StubFor(coordinator)->Delete(&ctx, req, &resp);
                assert(!status.ok());
            }
            std::cout << "[OK] Delete correctly failed with only 1/3 replicas alive "
                        "(quorum of 2 NOT met — this is expected, not a bug)." << std::endl;
            
            // check if the put using alive node works on that key
            {
                kvstore::PutRequest req;
                kvstore::PutResponse resp;
                grpc::ClientContext ctx;
                req.set_key(key);
                auto coordinator = PickAlive(addresses, dead);
                auto status = StubFor(coordinator)->Put(&ctx, req, &resp);
                assert(!status.ok());
            }
            std::cout << "[OK] Put correctly failed with only 1/3 replicas alive "
                        "(quorum of 2 NOT met — this is expected, not a bug)." << std::endl;

        }
    }
    // output successful test
    std::cout << "\nAll iterations passed successfully, verifying code for nicely following quorum consensus\n";
    Restart();
    return 0;
}