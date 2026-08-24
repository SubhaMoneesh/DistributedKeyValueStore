cmake_minimum_required(VERSION 3.16)
project(KeyValueStore CXX)

# Force the C++17 standard required by gRPC 1.70+
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# 1. Locate the gRPC and Protobuf packages installed in your local directory
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads REQUIRED) # Required for safe multi-threaded networking

# Paths where the compiler will generate the stub files on the fly
set(proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/kv.pb.cc")
set(proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/kv.pb.h")
set(grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/kv.grpc.pb.cc")
set(grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/kv.grpc.pb.h")


# 2. Automation: Tell CMake how to compile proto/kv.proto into C++ classes
add_custom_command(
    OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
    COMMAND protobuf::protoc                                      # Clean modern target
    ARGS --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin> # Safe Generator Expression
         -I "${CMAKE_CURRENT_SOURCE_DIR}/proto"
         "${CMAKE_CURRENT_SOURCE_DIR}/proto/kv.proto"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/proto/kv.proto"
)

# Allow our files to find the auto-generated headers
include_directories("${CMAKE_CURRENT_BINARY_DIR}" src)


# 3. Compile the auto-generated Proto files into a single reusable library
add_library(kv_grpc_proto ${grpc_srcs} ${proto_srcs})
target_link_libraries(kv_grpc_proto gRPC::grpc++ protobuf::libprotobuf)


# 4. Define the core Storage Engine and Hash Ring libraries
add_library(storage_engine src/storage_engine.cpp src/storage_engine.h)
# --- Hash Ring Library ---
add_library(hash_ring src/hash_ring.cpp src/hash_ring.h)



# Define Sanitizer Flags
set(SANITIZER_FLAGS "-fsanitize=thread" "-g" "-O1")


# 5. Build the Server executable
add_executable(kv_server src/server.cpp)
target_link_libraries(kv_server storage_engine hash_ring kv_grpc_proto Threads::Threads)


# 6. Build the Client executable
add_executable(kv_client src/client.cpp)
target_link_libraries(kv_client kv_grpc_proto Threads::Threads)


# 7. Build the Storage Engine test executable
add_executable(storage_engine_test
    tests/storage_engine_test.cpp
)

target_link_libraries(storage_engine_test
    PRIVATE
        storage_engine
        Threads::Threads
)

target_compile_options(storage_engine_test PRIVATE ${SANITIZER_FLAGS})
target_link_options(storage_engine_test PRIVATE ${SANITIZER_FLAGS})


# 8. Build the Client-Server test executable
add_executable(client_server_test
    tests/client_server_test.cpp
)

target_link_libraries(client_server_test
    PRIVATE
        kv_grpc_proto
        Threads::Threads
)


# 9. Hash Ring Test
add_executable(hash_ring_test tests/hash_ring_test.cpp)
target_link_libraries(hash_ring_test PRIVATE hash_ring Threads::Threads)
target_compile_options(hash_ring_test PRIVATE ${SANITIZER_FLAGS})
target_link_options(hash_ring_test PRIVATE ${SANITIZER_FLAGS})


# 10. Cluster Client Test
add_executable(cluster_client_test tests/cluster_client_test.cpp)
target_link_libraries(cluster_client_test PRIVATE kv_grpc_proto Threads::Threads)


# 11. Replication and quorum tests
add_executable(replication_test tests/replication_test.cpp)
target_link_libraries(replication_test PRIVATE kv_grpc_proto Threads::Threads)

add_executable(fault_tolerance_test tests/fault_tolerance_test.cpp)
target_link_libraries(fault_tolerance_test PRIVATE kv_grpc_proto Threads::Threads)



# 10. Membership Service Test

# --- Membership Service Library ---
add_library(membership_service src/membership_service.cpp src/membership_service.h)

add_executable(membership_service_test tests/membership_service_test.cpp)
target_link_libraries(membership_service_test PRIVATE membership_service Threads::Threads)
target_compile_options(membership_service_test PRIVATE ${SANITIZER_FLAGS})
target_link_options(membership_service_test PRIVATE ${SANITIZER_FLAGS})