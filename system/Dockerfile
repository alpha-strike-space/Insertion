# Use the latest Ubuntu as base image
FROM ubuntu:latest

# Install necessary packages
RUN apt-get update && apt-get install -y \
    g++ \
    libpqxx-dev \
    libcurl4-openssl-dev \
    wget \
    nlohmann-json3-dev

# Set working directory
WORKDIR /app

# Copy your C++ file into the container
COPY system_update.cpp .

# Compile the C++ file
RUN g++ -o update_systems system_update.cpp -lpqxx -lpq -lcurl

# Run the program once when the container starts
CMD ["./update_systems"]

# Remember to remove container once running this file.
# Once complete, with the systems insertion. You must remove the container. 
# Otherwise, you eat up unnecessary resources.
