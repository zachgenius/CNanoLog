/*
 * CNanoLog Date Rotation Example
 *
 * Demonstrates automatic log file rotation by date.
 * When rotation is enabled, log files are automatically created
 * with date suffixes, and new files are created when the date changes.
 */

#include "cnanolog.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main(void) {
    printf("=== CNanoLog Date Rotation Example ===\n\n");

    /* ====================================================================
     * Example 1: Daily Rotation (Most Common)
     * ==================================================================== */

    printf("Example 1: Daily Rotation\n");
    printf("--------------------------\n");

    /* Configure daily rotation */
    cnanolog_rotation_config_t config = {
        .policy = CNANOLOG_ROTATE_DAILY,
        .base_path = "logs/app.clog"
    };

    /* Initialize with rotation */
    if (cnanolog_init_ex(&config) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    /* Get current date to show expected filename */
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);

    printf("Configuration:\n");
    printf("  Policy:      CNANOLOG_ROTATE_DAILY\n");
    printf("  Base path:   %s\n", config.base_path);
    printf("  Current date: %04d-%02d-%02d\n",
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    printf("  Created file: logs/app-%04d-%02d-%02d.clog\n\n",
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    /* Log messages */
    printf("Writing sample log messages...\n");
    log_info("Application started");
    log_info1("Configuration loaded from %s", "config.json");
    log_warn1("Cache size limit: %d MB", 256);

    for (int i = 0; i < 10; i++) {
        log_info2("Processing request %d of %d", i + 1, 10);
        if (i % 3 == 0) {
            log_debug1("Debug checkpoint at iteration %d", i);
        }
    }

    log_info("All requests processed successfully");

    /* Get statistics */
    usleep(50000);  /* Wait for background thread */
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("\nStatistics:\n");
    printf("  Total logs written:  %llu\n", stats.total_logs_written);
    printf("  Bytes written:       %llu\n", stats.total_bytes_written);
    printf("  Compression ratio:   %.2fx\n", stats.compression_ratio_x100 / 100.0);
    printf("  Dropped logs:        %llu\n\n", stats.dropped_logs);

    /* Shutdown */
    cnanolog_shutdown();
    printf("Logger shut down successfully\n\n");

    /* ====================================================================
     * How Rotation Works
     * ==================================================================== */

    printf("How Date Rotation Works:\n");
    printf("------------------------\n");
    printf("1. File Naming:\n");
    printf("   Base path:  logs/app.clog\n");
    printf("   Daily file: logs/app-YYYY-MM-DD.clog\n");
    printf("   Example:    logs/app-2025-11-02.clog\n\n");

    printf("2. Automatic Rotation:\n");
    printf("   - Background thread checks date every loop iteration\n");
    printf("   - When date changes (e.g., at midnight):\n");
    printf("     a) Current file is finalized with dictionary\n");
    printf("     b) New file is created with new date\n");
    printf("     c) Logging continues seamlessly\n\n");

    printf("3. File Contents:\n");
    printf("   - Each dated file is self-contained\n");
    printf("   - Includes its own dictionary of log sites\n");
    printf("   - Can be decompressed independently\n");
    printf("   - Binary format preserves all timing information\n\n");

    /* ====================================================================
     * Example 2: No Rotation (Default)
     * ==================================================================== */

    printf("Example 2: No Rotation (Default)\n");
    printf("---------------------------------\n");
    printf("If you don't need rotation, use cnanolog_init():\n\n");
    printf("  cnanolog_init(\"app.clog\");  // Single file, no date suffix\n\n");

    /* ====================================================================
     * Decompression
     * ==================================================================== */

    printf("Decompressing Rotated Logs:\n");
    printf("---------------------------\n");
    printf("Each dated log file can be decompressed independently:\n\n");
    printf("  ./decompressor logs/app-2025-11-02.clog\n");
    printf("  ./decompressor logs/app-2025-11-03.clog\n");
    printf("  ./decompressor logs/app-2025-11-04.clog\n\n");

    printf("Or decompress multiple files:\n\n");
    printf("  ./decompressor logs/app-*.clog > all_logs.txt\n");
    printf("  ./decompressor -l ERROR logs/app-*.clog  # Only errors\n\n");

    /* ====================================================================
     * Best Practices
     * ==================================================================== */

    printf("Best Practices:\n");
    printf("---------------\n");
    printf("1. Use rotation for long-running services\n");
    printf("2. Daily rotation is sufficient for most applications\n");
    printf("3. Store logs in a dedicated directory (e.g., logs/)\n");
    printf("4. Set up log cleanup/archival for old files\n");
    printf("5. Each file is independent - safe to delete old files\n\n");

    printf("=== Example Complete ===\n");
    printf("\nNext Steps:\n");
    printf("1. Check logs/app-YYYY-MM-DD.clog in your directory\n");
    printf("2. Decompress with: ./decompressor logs/app-*.clog\n");
    printf("3. Try running across midnight to see rotation in action\n");

    return 0;
}
