# Insertion
Insertion into the proper postgresql table for the leaderboard.

## Overview

This project provides a utility for inserting data into a PostgreSQL leaderboard table, likely for tracking "kill" events in a game. It utilizes C++ with external libraries for PostgreSQL communication, JSON handling, and HTTP requests.

## Dependencies

Before building, ensure you have the following installed:

- C++17 compatible compiler (e.g., g++, clang++)
- [libpqxx](https://github.com/jtv/libpqxx) (PostgreSQL C++ connector)
- [nlohmann/json](https://github.com/nlohmann/json) (JSON for Modern C++)
- [libcurl](https://curl.se/libcurl/) (for HTTP requests)
- PostgreSQL server (for the actual leaderboard database)

On Ubuntu/Debian, you can install dependencies with:
```sh
sudo apt-get update
sudo apt-get install g++ libpqxx-dev nlohmann-json3-dev libcurl4-openssl-dev
```

## Building

To compile the main program (assuming kill_update.cpp):

```sh
g++ -std=c++17 kill_update.cpp -o kill_update -lpqxx -lpq -lcurl
```

If the nlohmann/json header is not system-installed, download it and ensure the include path is correct:
```sh
wget https://github.com/nlohmann/json/releases/latest/download/json.hpp
# Place json.hpp in the project directory or set the include path appropriately
```

## Running

1. Ensure your PostgreSQL server is running and accessible.
2. Set any required environment variables for database connection (see source for details).
3. Run the compiled binary:
   ```sh
   ./kill_update
   ```

## License

This project is licensed under the GNU General Public License v2.0. See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please open issues or submit pull requests for bug fixes or new features.
