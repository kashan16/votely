cmake_minimum_required(VERSION 3.20)
project(BlockchainVoting)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

add_executable(voting_system src/voting_system.cpp)

target_link_libraries(voting_system
  OpenSSL::SSL
  OpenSSL::Crypto
)

target_compile_options(voting_system PRIVATE -O2 -Wall)
