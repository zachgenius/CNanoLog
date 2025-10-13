#include <cnanolog.h>

int main() {
    log_info("Hello, %s!", "world");
    log_warn("This is a warning.");
    log_error("This is an error.");
    return 0;
}