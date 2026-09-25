# Distributed, Fault-Tolerant Key-Value Store

A high-availability, distributed key-value store built in C++ from scratch. Inspired by Amazon's Dynamo architecture, this project implements a decentralized cluster capable of sharding data, replicating it for fault tolerance, and resolving concurrent operations using tunable quorum consensus.

## 🚀 Features

*   **High-Performance Networking:** Utilizes **gRPC** and **Protocol Buffers** for fast, reliable inter-node communication and client-server interactions.
*   **Sharding via Consistent Hashing:** Distributes the keyspace evenly across a 6-node cluster using a continuous hash ring. Employs virtual nodes (v-nodes) to prevent data hotspots and ensure uniform load balancing.
*   **Replication & Quorum Consensus:** Implements tunable replication to handle node failures gracefully without data loss. 
    *   **Replication Factor (N = 3):** Data is replicated to 3 distinct physical nodes.
    *   **Quorum (W = 2, R = 2):** Ensures strict consistency by requiring overlapping read and write quorums ($R + W > N$).
*   **Versioned Storage Engine:** Thread-safe, in-memory storage engine utilizing `std::shared_mutex`, complete with logical timestamping and tombstone support for robust delete operations.
*   **Fault Tolerance:** Designed to survive up to 2 simultaneous node failures while continuing to serve read and write traffic safely.

## 🛠️ Tech Stack

*   **Language:** C++17
*   **RPC Framework:** gRPC
*   **Serialization:** Protocol Buffers (Protobuf)
*   **Build System:** CMake
*   **Concurrency:** C++ `<thread>`, `<future>`, and `<shared_mutex>`

## 📁 Project Structure

```text
Kv-store/
├── proto/              # gRPC service definitions (kv.proto)
├── src/                # Core implementation
│   ├── server.cpp      # Node server & replication logic
│   ├── client.cpp      # Basic client implementation
│   ├── storage_engine  # Versioned in-memory storage 
│   └── hash_ring       # Consistent hashing and preference lists
├── tests/              # Comprehensive test suite (unit, integration, fault tolerance)
├── scripts/            # Bash scripts to start/stop the cluster and simulate node death
├── cluster.conf        # Cluster topology configuration
└── CMakeLists.txt      # CMake build configuration
```

## ⚙️ Getting Started

### Prerequisites

Ensure you have the following installed on your system:
*   **C++17** compatible compiler (GCC/Clang)
*   **CMake** (v3.16 or higher)
*   **gRPC** and **Protobuf** installed locally

### Building the Project

1. Clone the repository:
   ```bash
   git clone <your-repository-url>
   cd Kv-store
   ```
2. Create a build directory and compile:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```
   *(Note: The executables will be built into the `build` directory, but the provided scripts expect to be run from the root `Kv-store` directory).*

## 🏃‍♂️ Running the Cluster

You can easily spin up a local 6-node cluster using the provided bash scripts.

1. **Start the cluster:**
   ```bash
   ./scripts/start_cluster.sh
   ```
   *This will start 6 nodes locally on ports 50051 through 50056. Logs will be saved in the `logs/` directory.*

2. **Stop the cluster:**
   ```bash
   ./scripts/stop_cluster.sh
   ```

## 🧪 Testing

The project includes a robust suite of tests validating everything from the storage engine to network fault tolerance. Run the compiled test binaries from the `build` directory:

*   **Storage Engine:** `./build/storage_engine_test`
*   **Hash Ring:** `./build/hash_ring_test`
*   **Client-Server Concurrency:** `./build/client_server_test`
*   **Cluster Routing:** `./build/cluster_client_test`
*   **Replication Verification:** `./build/replication_test`
*   **Fault Tolerance (Kill Nodes):** `./build/fault_tolerance_test`

## 🗺️ Roadmap / Future Work

Currently, the project completes **Phase 1 through Phase 3** of the original design. The following phases are planned:

*   [ ] **Phase 4: Decentralized Failure Detection (Gossip Protocol)**
    *   Transition from static configurations to a dynamic peer-to-peer heartbeat system.
    *   Implement random peer selection and membership list merging to autonomously detect and route around dead nodes.
*   [ ] **Phase 5: Vector Clocks & Conflict Resolution**
    *   Implement Vector Clocks `[Node_ID -> Counter]` to track causal histories of keys.
    *   Handle network partitions by returning sibling versions on diverged reads, passing conflict resolution to the client application.
