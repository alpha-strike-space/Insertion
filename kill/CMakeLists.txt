cmake_minimum_required(VERSION 3.14)
project(Insertion CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# nlohmann/json (header-only library)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Find libpqxx (PostgreSQL C++ connector)
find_package(PQXX REQUIRED)
# Find libcurl
find_package(CURL REQUIRED)

add_executable(kill_update kill_update.cpp)

target_link_libraries(kill_update
  PRIVATE
    nlohmann_json::nlohmann_json
    PQXX::pqxx
    CURL::libcurl
)

target_include_directories(kill_update
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
