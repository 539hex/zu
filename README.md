# ⚡ Zu - The Lightweight Key-Value Store

[![License](https://img.shields.io/badge/license-BSD--2--Clause-blue.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Version](https://img.shields.io/badge/version-0.3.0--alpha-orange.svg)]()

> 🚧
> **Alpha Release - Active Development**
>
> This project is currently in alpha stage with ongoing development. It's suitable for experimentation, learning, and development environments. Contributions and feedback are welcome as we work toward a stable release.

## Overview

Zu is a lightweight, fast command-line key-value store implemented in C that combines persistent disk storage with an in-memory hash table for caching. It automatically caches items on their first access, using the hash table to provide near-instantaneous O(1) lookups for cached data. All data is persistently stored in a local binary file, while frequently accessed items are kept in memory for faster retrieval.

## Key Features

- **Persistent Storage**: Data is automatically saved to and loaded from a binary file (`dump.zdb`)
- **Fast In-Memory Lookups**: A hash table is used for the in-memory cache, providing O(1) average time complexity for lookups.
- **Command-Line Interface**: Simple, intuitive commands for all operations
- **Performance Monitoring**: Built-in execution time measurement for each operation
- **Lightweight Design**: Minimal resource footprint with efficient C implementation



| Command              | Description                                                 |
| -------------------- | ----------------------------------------------------------- |
| `zset <key> <value>` | Store or update a key-value pair                            |
| `zget <key>`         | Retrieve the value for a given key (caches on first access) |
| `zrm <key>`          | Remove a key-value pair (from both cache and disk)          |
| `zall`               | List all stored key-value pairs                             |
| `init_db`            | Initialize the database with random key-value pairs         |
| `cache_status`       | Show current cache contents and usage statistics            |
| `benchmark`          | Run performance benchmark                                   |
| `clean`              | Clear the terminal screen                                   |
| `help`               | Display available commands                                  |
| `exit` / `quit`      | Exit the program                                            |

## REST API Endpoints

Zu also exposes a simple REST API for `set` and `get` operations. The server runs on port `1337` by default.

### Endpoints

| Endpoint             | Method | Description                                   | Parameters                                   | Example                                     |
| -------------------- | ------ | --------------------------------------------- | -------------------------------------------- | ------------------------------------------- |
| `/set`               | `GET`  | Store or update a key-value pair              | `key=<key>`, `value=<value>`                 | `http://localhost:1337/set?key=name&value=John%20Doe` |
| `/get`               | `GET`  | Retrieve the value for a given key            | `key=<key>`                                  | `http://localhost:1337/get?key=name`        |

## Installation

### Prerequisites

- C compiler (GCC recommended)
- Make utility
- POSIX-compliant system (Linux, macOS, WSL)

### Build Instructions

1. Clone the repository:

   ```bash
   git clone https://github.com/539hex/zu.git
   cd zu
   ```

2. Compile the project:

   ```bash
   make
   ```

   This creates an optimized executable named `zu` in the root directory.

3. Run the program:
   ```bash
   ./zu
   ```

## Usage Example

```bash
$ ./zu
Zu v0.3.0-alpha
Type 'help' for available commands.

> zset name "John Doe"
OK
(0.12ms)

> zget name
John Doe
(0.08ms)

> cache_status
Cache status: 1/1000 items used
  [0] Key: name, Value: John Doe, Hits: 1, Last accessed: 1234567890

> zall
name: John Doe
Total keys: 1
(0.15ms)

> exit
Goodbye!
```

## Project Structure

```
zu/
├── Makefile          # Build configuration
├── README.md         # Project documentation
├── LICENSE           # License file
├── test.sh           # Test script
├── src/              # Source code directory
│   ├── zu.c          # Main program entry point
│   ├── *.c           # C files
│   └── *.h           # Header files
├── tests/            # Test suite directory
│   ├── test.c        # Test definition file
└── zu                # Compiled executable (after build)
```

## Configuration

The system can be configured by modifying the following parameters in `src/config.h`:

### Cache Settings

- **CACHE_SIZE**: Maximum number of items that can be stored in the memory cache (default: 1000)
- **CACHE_TTL**: Time-to-live (TTL) for cached items in seconds (default: 60)

- **Caching Behavior**:
  - Items are cached on their first access (get operation)
  - Uses LRU (Least Recently Used) eviction when cache is full
  - Cache entries track hit count and last access time
  - Cache is cleared on program exit

### Database Settings

- **FILENAME**: Name of the database file (default: "dump.zdb")
- **INIT_DB_SIZE**: Number of random key-value pairs to create when initializing the database (default: 5)
- **MIN_LENGTH**: Minimum length for generated keys and values (default: 4)
- **MAX_LENGTH**: Maximum length for generated keys and values (default: 64)

These settings can be modified before compilation to adjust the behavior of the system. For example, increasing `CACHE_SIZE` will allow more items to be cached in memory, while decreasing it will make the cache more aggressive in evicting items.

## Testing

To run the test suite, execute the following command:

```bash
./test.sh
```

This will build the test suite and run it.

## Roadmap

- [ ] Support for different data types

- [X] REST API
- [ ] Atomic operations and transactions
- [X] Comprehensive test suite
- [X] Performance benchmarking tools
- [ ] Data compression options
- [X] Hash tables for faster lookups

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

## License

This project is licensed under the BSD 2-Clause License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Inspired by the amazing [Redis](https://github.com/redis/redis)
- Built with performance and simplicity in mind

---
