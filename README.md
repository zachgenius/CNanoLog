# CNanoLog

A simple, lightweight, C99-compatible, zero-dependency logging library.

## Features

*   Zero dependencies
*   C99 compatible
*   Header-only or compiled library
*   Different log levels
*   Easy to use

## Usage

```c
#include <cnanolog.h>

int main() {
    log_info("Hello, %s!", "world");
    log_warn("This is a warning.");
    log_error("This is an error.");
    return 0;
}
```