/**
 * cli.h — Command-Line Interface Argument Parsing
 * ================================================
 * Engineering Physics Project — BB84 QKD Simulator
 *
 * Parses argv[] into a Config struct consumed by main.c.
 *
 * USAGE:
 *   ./bb84_sim [OPTIONS]
 *
 * OPTIONS:
 *   -n <photons>   Number of qubits / photons to simulate (default: 64)
 *   -e             Enable Eve (eavesdropper) — intercept-resend attack
 *   -v             Verbose mode — print per-photon tables (good for -n <= 64)
 *   -s <seed>      Set PRNG seed manually (default: time-based)
 *   -b             Run both scenarios (no-Eve + Eve) in sequence
 *   -h             Show this help and exit
 *
 * EXAMPLES:
 *   ./bb84_sim -n 32 -b -v           # both scenarios, verbose, 32 photons
 *   ./bb84_sim -n 1000 -e            # 1000 photons, Eve active only
 *   ./bb84_sim -n 256 -s 42 -b      # reproducible run with seed 42
 */

#ifndef CLI_H
#define CLI_H

#include <stdint.h>

/**
 * Config — parsed command-line options.
 * All fields have sensible defaults set by cli_parse_args().
 */
typedef struct {
    int      n_qubits;    /**< Number of photons to simulate            */
    int      eve_active;  /**< 1 = run with Eve eavesdropping           */
    int      verbose;     /**< 1 = print per-photon detail tables       */
    int      both;        /**< 1 = run both (clean + Eve) back-to-back  */
    uint32_t seed;        /**< PRNG seed; 0 = use time()                */
} Config;

/**
 * cli_parse_args() — populate a Config from argc/argv.
 *
 * @param cfg   Output Config struct to populate.
 * @param argc  Argument count from main().
 * @param argv  Argument vector from main().
 * @return      0 on success, non-zero if the caller should exit(0)
 *              (e.g., after printing help).
 */
int  cli_parse_args(Config *cfg, int argc, char *argv[]);

/**
 * cli_print_banner() — print an ASCII header with the active configuration.
 *
 * @param cfg  Parsed configuration to display.
 */
void cli_print_banner(const Config *cfg);

/**
 * cli_print_help() — print usage/help text and return.
 *
 * @param prog  argv[0], used in the usage line.
 */
void cli_print_help(const char *prog);

#endif /* CLI_H */
