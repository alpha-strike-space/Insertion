# Use the latest Ubuntu as base image
FROM ubuntu:latest

# Install necessary packages
RUN apt-get update && apt-get install -y \
    g++ \
    libpqxx-dev \
    libcurl4-openssl-dev \
    wget \
    nlohmann-json3-dev \
    cron

# Set working directory
WORKDIR /app

# Get the JSON Reader
#RUN wget https://github.com/nlohmann/json/releases/latest/download/json.hpp -O json.hpp

# Copy your C++ file into the container
COPY kill_update.cpp .

# Compile the C++ file
RUN g++ -o killer kill_update.cpp -lpqxx -lpq -lcurl

# Add a cron job to run the program every hour
RUN echo "*/5 * * * * root /app/killer >> /app/output.log 2>&1" > /etc/cron.d/killer-cron

# Give execution rights to cron job
RUN chmod 0644 /etc/cron.d/killer-cron

# Start the cron service when the container runs
CMD ["cron", "-f"]
