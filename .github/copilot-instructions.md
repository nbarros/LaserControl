# LaserControl Copilot Instructions

## Project Overview

`LaserControl` is a C++11 static library and app set for controlling three serial devices used in DUNE IO laser operations:

- `device::Laser`
- `device::Attenuator`
- `device::PowerMeter`

The current serial transport is implemented in `include/asio_serial/serial.hpp` + `src/asio_serial.cpp` using `boost::asio::serial_port`.

## Architecture

### 1. Device layer (`Device`)

Core abstraction: `device::Device` in `include/Device.hh` / `src/Device.cpp`

Responsibilities:
- Own serial port object (`serial::Serial`)
- Hold connection parameters (`m_comport`, `m_baud`)
- Manage framing (`m_com_pre`, `m_request_suffix`, `m_response_suffix`)
- Enforce thread safety with `std::recursive_mutex m_io_mutex`
- Provide:
  - `write_cmd(...)`
  - `read_cmd(...)`
  - `exchange_cmd(...)` (atomic command + response)
  - `reset_connection()`

### 2. Device protocol layer

Derived protocol implementations:
- `src/Laser.cpp` / `include/Laser.hh`
- `src/Attenuator.cpp` / `include/Attenuator.hh`
- `src/PowerMeter.cpp` / `include/PowerMeter.hh`

These classes implement device-specific command formats, parsing, and state caching.

### 3. Serial transport layer

Transport API and implementation:
- Header: `include/asio_serial/serial.hpp`
- Implementation: `src/asio_serial.cpp`

Key capabilities:
- Blocking read/write primitives
- Read-line helpers (`readline`, `readlines`)
- Callback-based bidirectional transactions:
  - `async_transaction(request, callback, response_eol, max_size)`
  - `transaction(request, response, response_eol, max_size)`

Important behavior:
- Request bytes are exactly what caller passes (`request` already includes request suffix if needed)
- `response_eol` is only used as read delimiter (`async_read_until`), not appended to write data

### 4. Utilities and apps

- `src/utilities.cpp` / `include/utilities.hh`: string helpers + serial port discovery wrappers
- `apps/serial_manager.cpp`: operational CLI for mapping devices and running commands
- `src/probe_ports.cc`: serial port listing utility

## Build and Run

Root build:

```bash
cd sw/LaserControl
mkdir -p build
cd build
cmake ..
cmake --build . -j4
```

Primary generated targets:
- `libLaserControl.a`
- `probe_serial_ports`
- `apps/serial_manager`

Dependencies in current CMake:
- Boost.System
- spdlog
- readline
- nlohmann_json

## Coding Style Requirements (Project-Specific)

Follow these rules strictly when editing this project:

1. Braces style:
- Open braces on a new line for functions, classes, `if/else`, loops, and `switch` blocks
- Exception: one-liners may keep inline braces

2. Method naming:
- Prefer lowercase snake_case names (`set_timeout_ms`, `exchange_cmd`, etc.)
- For serial transport APIs, prefer snake_case aliases (`set_port`, `set_baudrate`, `is_open`, `wait_readable`, etc.)

3. Keep compatibility unless explicitly asked to break API:
- If renaming an externally used method, provide compatibility wrappers where practical

4. Thread safety:
- Preserve `Device` lock discipline (`m_io_mutex`) around command/response operations
- Do not introduce write/read interleaving between command and matching response paths

5. Exception behavior:
- Existing code uses `serial::IOException`, `serial::SerialException`, and `serial::PortNotOpenedException`
- Preserve this behavior and avoid silent failures

## Editing Guidance

When implementing serial command paths:

- Use `exchange_cmd(...)` instead of split `write_cmd(...)` + `read_cmd(...)` where protocol allows
- Keep request suffix and response delimiter distinct:
  - request suffix (`m_request_suffix`) is part of outbound payload
  - response delimiter (`m_response_suffix`) drives read termination

When touching serial transport internals:
- Preserve transaction queue semantics
- Preserve timeout cancellation behavior in async operations
- Keep read/write mutex behavior intact

## Scope Boundaries

- Do not modify mirrored copies under:
  - `sw/slow_control/quasar/opcua-server/LaserControl/`
  unless explicitly requested.
- Primary source of truth for this project is:
  - `sw/LaserControl/`

## Validation Checklist for Changes

After modifications:
1. Build with `cmake --build . -j4`
2. Ensure `probe_serial_ports` links
3. Ensure `apps/serial_manager` links
4. If serial behavior changed, verify `exchange_cmd` call paths still compile and preserve atomic request/response flow
