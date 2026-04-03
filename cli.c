/**
 * cli.c — CLI Argument Parsing Implementation
 * =============================================
 * Engineering Physics Project — BB84 QKD Simulator
 */

#include "cli.h"
#include "bb84.h"    /* for MAX_QUBITS */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * cli_print_help()
 * ------------------------------------------------------------------------- */
void cli_print_help(const char *prog)
{
    printf(
        "\nUSAGE:\n"
        "  %s [OPTIONS]\n\n"
        "OPTIONS:\n"
        "  -n <photons>   Photons to simulate (1-%d, default: 64)\n"
        "  -e             Activate Eve (intercept-resend eavesdropper)\n"
        "  -b             Run BOTH scenarios (clean channel + Eve)\n"
        "  -v             Verbose — print per-photon tables\n"
        "                 (recommended only when -n <= 64)\n"
        "  -s <seed>      Fixed PRNG seed for reproducible runs\n"
        "  -h             Print this help and exit\n\n"
        "EXAMPLES:\n"
        "  %s -n 32 -b -v          # both scenarios, verbose\n"
        "  %s -n 1000 -e           # 1000 photons, Eve only\n"
        "  %s -n 256 -s 42 -b     # reproducible run, seed=42\n\n",
        prog, MAX_QUBITS, prog, prog, prog
    );
}

/* -------------------------------------------------------------------------
 * cli_parse_args()
 * ------------------------------------------------------------------------- */
int cli_parse_args(Config *cfg, int argc, char *argv[])
{
    /* Defaults */
    cfg->n_qubits   = 64;
    cfg->eve_active = 0;
    cfg->verbose    = 0;
    cfg->both       = 0;
    cfg->seed       = 0;   /* 0 means "use time()" */

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            cli_print_help(argv[0]);
            return 1;   /* signal: caller should exit(0) */
        }

        else if (strcmp(argv[i], "-e") == 0) {
            cfg->eve_active = 1;
        }

        else if (strcmp(argv[i], "-v") == 0) {
            cfg->verbose = 1;
        }

        else if (strcmp(argv[i], "-b") == 0) {
            cfg->both = 1;
        }

        else if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "[ERROR] -n requires a numeric argument.\n");
                return -1;
            }
            cfg->n_qubits = atoi(argv[++i]);
            if (cfg->n_qubits <= 0 || cfg->n_qubits > MAX_QUBITS) {
                fprintf(stderr,
                    "[ERROR] -n must be between 1 and %d (got %d).\n",
                    MAX_QUBITS, cfg->n_qubits);
                return -1;
            }
        }

        else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "[ERROR] -s requires a numeric argument.\n");
                return -1;
            }
            cfg->seed = (uint32_t)atoi(argv[++i]);
        }

        else {
            fprintf(stderr, "[ERROR] Unknown option: %s\n", argv[i]);
            cli_print_help(argv[0]);
            return -1;
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * cli_print_banner()
 * ------------------------------------------------------------------------- */
void cli_print_banner(const Config *cfg)
{
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║   BB84 Quantum Key Distribution  —  Simulator   ║\n");
    printf("║   Engineering Physics Project                    ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    printf("  Configuration\n");
    printf("  ─────────────────────────────────────────\n");
    printf("  Photons (n)    : %d\n",   cfg->n_qubits);
    printf("  Eve active     : %s\n",   cfg->eve_active ? "YES ⚠" : "no");
    printf("  Both scenarios : %s\n",   cfg->both       ? "yes"   : "no");
    printf("  Verbose        : %s\n",   cfg->verbose    ? "yes"   : "no");

    if (cfg->seed != 0) {
        printf("  PRNG seed      : %u (fixed)\n", cfg->seed);
    } else {
        printf("  PRNG seed      : time-based (non-deterministic)\n");
    }

    printf("  ─────────────────────────────────────────\n\n");
}
