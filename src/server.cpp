#include <grpcpp/grpcpp.h>
#include "storage_engine.h"
#include "hash_ring.h"
#include "kv.grpc.pb.h"
#include<iostream>
#include<csignal>
#include<atomic>
#include<thread>
#include<chrono>
#include<fstream>
#include<future>


std::unique_ptr<grpc::Server> server;
std::atomic<bool> shutdown_requested(false);

struct GetReply {
    bool ok;
    bool found;
    std::string value;
    uint64_t timestamp;
    bool is_tombstone;
};

void SignalHandler(int signal) {
    shutdown_requested.store(true);
}

uint64_t NowMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

class KVServiceImpl : public kvstore::KeyValueStore::Service {
    public:
        static constexpr int kN = 3;  // Replication factor
        static constexpr int kR = 2;  // read quorum
        static constexpr int kW = 2;  // write quorum

        // store self address, peer stubs, and ring
        KVServiceImpl(std::string self_address, const std::vector<std::string>& all_nodes)
         : self_address_(std::move(self_address)) {
            for (const auto& addr: all_nodes) {
                ring_.AddNode(addr);
                if (addr != self_address_) {
                    auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
                    peer_stubs_[addr] = kvstore::KeyValueStore::NewStub(channel);
                }
            }
            std::cout << "Node " << self_address_ << " ready. Cluster size: "
                  << all_nodes.size() << ". N: " << kN << ", W: " << kW << " and R: "
                  << kR << std::endl;
        }

        grpc::Status Put(
            grpc::ServerContext*,
            const kvstore::PutRequest* request,
            kvstore::PutResponse* response
        ) override {
            uint64_t timestamp = NowMicros();
            std::vector<std::string> pref = ring_.GetPreferenceList(request->key(), kN);
            int ack = 0;
            std::vector<std::future<bool>> futures;
            for (auto& node: pref) {
                if (node == self_address_) {    
                    storage_.Put(request->key(), request->value(), timestamp);
                    ack++;
                    continue;
                }
                futures.push_back(std::async(std::launch::async, [
                    this, node, key = request->key(), value = request->value(), timestamp
                ] () {
                    return SendReplicatePut(node, key, value, timestamp);
                }));
            }
            for (auto& f: futures) if(f.get()) ack++;
            response->set_success(ack >= kW);
            if (ack >= kW) return grpc::Status::OK;
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Failed to reach write quorum.");
        }
        
        grpc::Status ReplicatePut(
            grpc::ServerContext*,
            const kvstore::ReplicatePutRequest* request,
            kvstore::ReplicatePutResponse* response
        ) override {
            storage_.Put(request->key(), request->value(), request->timestamp());
            response->set_success(true);  // ack means put is processed, whether it is done or not
            return grpc::Status::OK;
        }

        grpc::Status Get(
            grpc::ServerContext*,
            const kvstore::GetRequest* request,
            kvstore::GetResponse* response
        ) override {
            auto pref = ring_.GetPreferenceList(request->key(), kN);
            std::vector<GetReply> replies;
            std::vector<std::future<GetReply>> futures;
            for (auto& node: pref) {
                if (node == self_address_) {
                    auto val = storage_.GetVersioned(request->key());
                    if (val.has_value()) replies.push_back({true, true, val->value, val->timestamp, val->is_tombstone});
                    else replies.push_back({true, false, "", 0, false});
                    continue;
                }
                futures.push_back(std::async(std::launch::async, [this, node, key = request->key()] () {
                    return SendReplicateGet(node, key);
                }));
            }
            for (auto& f: futures) replies.push_back(f.get());
            int responded = 0;
            bool found = false;
            GetReply winner{};
            for (auto& r: replies) {
                if (!r.ok) continue;
                responded++;
                if (r.found && (!found || winner.timestamp < r.timestamp)) {
                    found = true;
                    winner = r;
                }
            }
            if (responded < kR) {
                return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Failed to reach read quorum");
            }
            response->set_found(found && !winner.is_tombstone);
            if (response->found()) response->set_value(winner.value);
            return grpc::Status::OK;
        }

        grpc::Status ReplicateGet(
            grpc::ServerContext*,
            const kvstore::ReplicateGetRequest* request,
            kvstore::ReplicateGetResponse* response
        ) override {
            auto val = storage_.GetVersioned(request->key());
            response->set_found(val.has_value()); 
            if (!val.has_value()) return grpc::Status::OK;
            response->set_value(val->value);
            response->set_timestamp(val->timestamp);
            response->set_is_tombstone(val->is_tombstone);
            return grpc::Status::OK;
        }

        grpc::Status Delete(
            grpc::ServerContext*,
            const kvstore::DeleteRequest* request,
            kvstore::DeleteResponse* response
        ) override {
            auto timestamp = NowMicros();
            auto pref = ring_.GetPreferenceList(request->key(), kN);
            int ack = 0;
            std::vector<std::future<bool>> futures;
            for (auto& node: pref) {
                if (node == self_address_) {
                    storage_.Delete(request->key(), timestamp);
                    ack++;
                    continue;
                }
                futures.push_back(std::async(std::launch::async, [
                    this, node, key = request->key(), timestamp
                ] () {
                    return SendReplicateDelete(node, key, timestamp);
                }));
            }
            for (auto& f: futures) if(f.get()) ack++;
            response->set_success(ack >= kW);
            if (ack >= kW) return grpc::Status::OK;
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Failed to reach write quorum");
        }

        grpc::Status ReplicateDelete(
            grpc::ServerContext*,
            const kvstore::ReplicateDeleteRequest* request,
            kvstore::ReplicateDeleteResponse* response
        ) override {
            storage_.Delete(request->key(), request->timestamp());
            response->set_success(true);
            return grpc::Status::OK;
        }

        grpc::Status DebugOwner(
            grpc::ServerContext*,
            const kvstore::DebugOwnerRequest* request,
            kvstore::DebugOwnerResponse* response
        ) override {
            response->set_owner_address(ring_.GetNode(request->key()));
            return grpc::Status::OK;
        }

        grpc::Status DebugPreferenceList(
            grpc::ServerContext*,
            const kvstore::DebugPreferenceListRequest* request,
            kvstore::DebugPreferenceListResponse* response
        ) override {
            for (auto& node: ring_.GetPreferenceList(request->key(), kN)) response->add_nodes(node);
            return grpc::Status::OK;
        }

        grpc::Status DebugLocalGet(
            grpc::ServerContext*,
            const kvstore::DebugLocalGetRequest* request,
            kvstore::DebugLocalGetResponse* response
        ) override {
            auto val = storage_.GetVersioned(request->key());
            response->set_exists(val.has_value());
            if (!val.has_value()) return grpc::Status::OK;
            response->set_value(val->value);
            response->set_timestamp(val->timestamp);
            response->set_is_tombstone(val->is_tombstone);
            return grpc::Status::OK;
        }
        

    private:
        StorageEngine storage_;
        std::unordered_map<std::string, std::unique_ptr<kvstore::KeyValueStore::Stub>> peer_stubs_;
        ConsistentHashRing ring_;
        std::string self_address_;

        bool SendReplicatePut(std::string node, std::string key, std::string value, uint64_t timestamp) {
            grpc::ClientContext ctx;
            ctx.set_deadline(
                std::chrono::system_clock::now() + std::chrono::milliseconds(2000)
            );
            kvstore::ReplicatePutRequest req;
            kvstore::ReplicatePutResponse resp;
            req.set_key(key);
            req.set_value(value);
            req.set_timestamp(timestamp);
            auto status = peer_stubs_.at(node)->ReplicatePut(&ctx, req, &resp);
            return (status.ok() && resp.success());
        }

        GetReply SendReplicateGet(std::string node, std::string key) {
            grpc::ClientContext ctx;
            ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(2000));
            kvstore::ReplicateGetRequest req;
            req.set_key(key);
            kvstore::ReplicateGetResponse resp;
            auto status = peer_stubs_.at(node)->ReplicateGet(&ctx, req, &resp);
            if(!status.ok()) return {false, false, "", 0, false};
            return {true, resp.found(), resp.value(), resp.timestamp(), resp.is_tombstone()};
        }

        bool SendReplicateDelete(std::string node, std::string key, uint64_t timestamp) {
            grpc::ClientContext ctx;
            ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(2000));
            kvstore::ReplicateDeleteRequest req;
            req.set_key(key);
            req.set_timestamp(timestamp);
            kvstore::ReplicateDeleteResponse resp;
            auto status = peer_stubs_.at(node)->ReplicateDelete(&ctx, req, &resp);
            return status.ok() && resp.success();
        }
};


std::vector<std::string> ExtractNodes(const std::string& path) {
    std::vector<std::string> nodes;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty())
            nodes.push_back(line);
    }
    return nodes;
}


void RunServer(const std::string& self_address, const std::string& cluster_config_file) {
    auto all_nodes = ExtractNodes(cluster_config_file);
    KVServiceImpl service(self_address, all_nodes);
    grpc::ServerBuilder builder;

    builder.AddListeningPort(self_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    std::cout << "Server listening on port " << self_address << std::endl;

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    while(!shutdown_requested.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Shutdown requested. Shutting down gracefully." << std::endl;    
    server->Shutdown();
    server->Wait();

    std::cout << "Server shutdown complete." << std::endl;
}


int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <self_address> <cluster_config_file>" << std::endl;
        return 1;
    }

    RunServer(argv[1], argv[2]);
    return 0;
}