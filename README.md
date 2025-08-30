# Insertion

A C++ utility for inserting game event data (such as leaderboard "kills") into a PostgreSQL database. Designed for high-performance, reliable data handling in the competitive environment of [Eve Frontier](https://evefrontier.com/en).

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Getting Started](#getting-started)
  - [Dependencies](#dependencies)
  - [Building](#building)
  - [Configuration](#configuration)
- [Usage](#usage)
- [Code Structure](#code-structure)
- [Troubleshooting & FAQ](#troubleshooting--faq)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

Insertion is a tool for reliably inserting game events into a PostgreSQL database, with special attention to efficiency and extensibility. It is used for environments such as [Eve Frontier](https://evefrontier.com/en) where statistical updates and accuracy are critical.

## Features

- Fast and safe insertion of leaderboard events.
- Uses industry-standard libraries: libpqxx (PostgreSQL), nlohmann/json (JSON), libcurl (HTTP).
- Configurable PostgreSQL connection via environment variables.
- Modular code for easy extension (e.g., support other event types).
- Simple CLI interface for manual or automated use.

## Getting Started

### Dependencies

Ensure you have these installed:

- **C++17** compatible compiler (g++, clang++)
- [`libpqxx`](https://github.com/jtv/libpqxx) — PostgreSQL C++ library
- [`nlohmann/json`](https://github.com/nlohmann/json) — Modern C++ JSON library
- [`libcurl`](https://curl.se/libcurl/) — For HTTP requests
- **PostgreSQL server** — For the actual leaderboard database

**Ubuntu/Debian:**
```sh
sudo apt-get update
sudo apt-get install g++ libpqxx-dev nlohmann-json3-dev libcurl4-openssl-dev
```

### Building

To compile the main program (e.g., `kill_update.cpp`):

```sh
g++ -std=c++17 kill_update.cpp -o kill_update -lpqxx -lpq -lcurl
```

If `nlohmann/json` is not system-installed:
```sh
wget https://github.com/nlohmann/json/releases/latest/download/json.hpp
# Place json.hpp in the project directory or set the include path appropriately
```

### Configuration

Set these environment variables before running:

- `PGHOST` — PostgreSQL server host (e.g., `localhost`)
- `PGPORT` — PostgreSQL port (default: `5432`)
- `PGUSER` — Database username
- `PGPASSWORD` — Database password
- `PGDATABASE` — Database name

Example:
```sh
export PGHOST=localhost
export PGPORT=5432
export PGUSER=myuser
export PGPASSWORD=mypassword
export PGDATABASE=leaderboard
```

## Usage

1. Ensure your PostgreSQL server is running and accessible.
2. Set required environment variables.
3. Run the compiled binary:
   ```sh
   ./kill_update
   ```
   - The program may accept command-line arguments or read JSON input—see source code for details.
4. For automated usage, integrate this binary with your event system of choice.

## Code Structure

- **kill_update.cpp** — Main program logic, handles input, processes events, inserts into database.

All other file follow a similar logic. You may either do this with CMakeLists.txt to create executables and services or run everything on containers with the Docker Engine.

## Troubleshooting & FAQ

- **Connection errors:** Double-check environment variables and database credentials.
- **Missing dependencies:** Install all required libraries and headers, especially PostgreSQL and cURL.
- **Segmentation faults:** Ensure correct input format and valid pointers.
- **How do I add a new event type?**  
  Extend the parsing and insertion logic in the main source files respective of their proper functions.

## Contributing

Contributions are welcome!

1. Fork the repository.
2. Create a feature branch.
3. Submit pull requests with clear descriptions.
4. Open issues for bugs or feature requests.

## License

This project is licensed under the [GNU General Public License v2.0](LICENSE).

---

*For questions or support, open an issue on GitHub.*
