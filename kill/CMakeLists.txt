# Minimum Version for CMAKE.
cmake_minimum_required(VERSION 3.14)
project(KillUpdate) # Create the Project
# Crow makes usage of C++ 17 Standards.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Find required packages before using their variables
find_package(PostgreSQL REQUIRED) #  PostgreSQL : Must Install
find_package(CURL REQUIRED)  # cURL : Must Install
# We will use the FetchContent to download our necessary repositories for the libraries we shall use.
include(FetchContent)
# FetchContent to download the libpqxx library for us to use.
FetchContent_Declare(
  libpqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG 7.10.1 # Specification for release or commit tag from the repository.
  CMAKE_ARGS
    "-DPostgreSQL_INCLUDE_DIR=${PostgreSQL_INCLUDE_DIR}"
    "-DPostgreSQL_LIBRARY=${PostgreSQL_LIBRARY}"
)
FetchContent_MakeAvailable(libpqxx)
# If we received pqxx_static we create an alias for when pqxx is linked later for server.cpp file.
if(TARGET pqxx_static AND NOT TARGET pqxx::pqxx)
    add_library(pqxx::pqxx ALIAS pqxx_static)
endif()
# Otherwise, if we have pqxx we add the library with the proper alias.
if(TARGET pqxx AND NOT TARGET pqxx::pqxx)
    add_library(pqxx::pqxx ALIAS pqxx)
endif()
# FetchContent to download the nlohmann json library for us to use.
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0  # Specification for release or commit tag from the repository.
)
FetchContent_MakeAvailable(nlohmann_json)
# Add your source files here
set(SOURCES
    kill_update.cpp
)
# Put together
add_executable(update_kills ${SOURCES})
# Includes from dependencies
target_include_directories(update_kills PRIVATE
    ${nlohmann_json_SOURCE_DIR}/single_include
    ${libpqxx_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PostgreSQL_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)
# Target libraries. Use pqxx::pqxx if available, otherwise fallback.
if(TARGET pqxx::pqxx)
    set(PQXX_TARGET pqxx::pqxx)
else()
    set(PQXX_TARGET pqxx)
endif()
# Link all libraries
target_link_libraries(kills PRIVATE
    nlohmann_json::nlohmann_json
    ${PQXX_TARGET}
    ${PostgreSQL_LIBRARIES}
    CURL::libcurl 
)
# NOTE:
# You must run the target executable on the system as a service for continuity.
# mkdir build
# cd build
# cmake ..
# make
