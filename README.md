# ⚡ Zu - The Adaptive Cache Store

[![License](https://img.shields.io/badge/license-BSD--2--Clause-blue.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Version](https://img.shields.io/badge/version-0.1.0--alpha-orange.svg)]()

> 🚧
> **Alpha Release - Active Development**
>
> This project is currently in alpha stage with ongoing development. It's suitable for experimentation, learning, and development environments. Contributions and feedback are welcome as we work toward a stable release.

## Overview

Zu is a lightweight, command-line key-value store implemented in C that combines persistent disk storage with intelligent in-memory caching. It automatically identifies frequently accessed "hot" keys and caches them for improved performance, while maintaining data persistence through a local binary file.

## Key Features

- **Persistent Storage**: Data is automatically saved to and loaded from a binary file (`z.u`)
- **Intelligent Caching**: Frequently accessed keys are cached in memory for faster retrieval
- **Command-Line Interface**: Simple, intuitive commands for all operations
- **Performance Monitoring**: Built-in execution time measurement for each operation
- **Lightweight Design**: Minimal resource footprint with efficient C implementation

## Commands

| Command              | Description                                            |
| -------------------- | ------------------------------------------------------ |
| `zset <key> <value>` | Store or update a key-value pair                       |
| `zget <key>`         | Retrieve the value for a given key                     |
| `zrm <key>`          | Remove a key-value pair                                |
| `zall`               | List all stored pairs with hit counts and cache status |
| `init_db`            | Initialize the database with random key-value pairs    |
| `help`               | Display available commands                             |
| `exit` / `quit`      | Save data and exit the program                         |

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
Zu v0.1.0-alpha
Type 'help' for available commands.

> zset name "John Doe"
OK
(0.12ms)

> zget name
"John Doe"
[COLD, hits: 1]
(0.08ms)


> zall
name: "John Doe" [COLD, hits: 1]
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
├── src/              # Source code directory
│   ├── zu.c          # Main program entry point
│   ├── *.c           # C files
│   └── *.h           # Header files
└── zu                # Compiled executable (after build)
```

## Configuration

The caching system uses configurable thresholds for determining "hot" keys. These can be modified in the source code before compilation:

- **HIT_THRESHOLD_FOR_CACHING**: Number of accesses required for a key to be cached

## Roadmap

- [ ] Support for different data types
- [ ] Network interface (TCP/HTTP)
- [ ] Atomic operations and transactions
- [ ] Comprehensive test suite
- [ ] Performance benchmarking tools
- [ ] Data compression options

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

## License

This project is licensed under the BSD 2-Clause License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Inspired by the amazing [Redis](https://github.com/redis/redis)
- Built with performance and simplicity in mind

---
