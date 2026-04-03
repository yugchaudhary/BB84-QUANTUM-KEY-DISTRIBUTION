/**
 * main.c — BB84 QKD Simulator Entry Point (v4 — Final Engine)
 * =============================================================
 * Engineering Physics Project
 *
 * Modules integrated:
 *   1. bb84.c        — core quantum logic (Alice / Eve / Bob / sifting)
 *   2. cli.c         — base argument parsing (reused for banner)
 *   3. privacy_amp.c — Toeplitz universal hashing (privacy amplification)
 *   4. stats.c       — Monte-Carlo QBER statistics + ASCII histogram
 *   5. cascade.c     — Cascade error correction (Brassard & Salvail 1993)
 *   6. export.c      — CSV export for external plotting
 *
 * ALL FLAGS:
 *   -n <N>           Photons per run (default: 64)
 *   -e               Eve active (intercept-resend)
 *   -b               Both scenarios (clean + Eve)
 *   -v               Verbose per-photon tables (use with small -n)
 *   -p               Privacy amplification after sifting
 *   -c               Cascade error correction before privacy amplification
 *   -s <seed>        Fixed PRNG seed
 *   --stats <N>      Monte-Carlo: N trials, print QBER report + histogram
 *   --export <file>  Export per-trial CSV after --stats
 *   --sweep <file>   Sweep n_qubits [32..512], export summary CSV
 *   -h               Help
 */

#include "bb84.h"
#include "cli.h"
#include "privacy_amp.h"
#include "stats.h"
#include "cascade.h"
#include "export.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define SEC_PARAM      40     /* bits of security slack for privacy amp  */
#define SWEEP_N_MIN    32     /* photon-count sweep range min            */
#define SWEEP_N_MAX    512    /* photon-count sweep range max            */
#define SWEEP_STEPS    16     /* data points across sweep range          */
#define SWEEP_TRIALS   200    /* trials per sweep data point             */

/* =========================================================================
 * Extended config
 * =========================================================================
 */
typedef struct {
    Config base;           /* -n -e -b -v -s                             */
    int    do_pa;          /* -p  : run privacy amplification            */
    int    do_cascade;     /* -c  : run Cascade error correction         */
    int    stats_runs;     /* --stats <N>                                */
    char   export_file[256]; /* --export <file>                          */
    char   sweep_file[256];  /* --sweep  <file>                          */
} ExtConfig;

/* -------------------------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------------------------- */
static void print_full_help(const char *prog);
static int  parse_all_args(ExtConfig *cfg, int argc, char *argv[]);
static void run_scenario(const ExtConfig *cfg, int eve_active,
                         const char *label);
static void run_stats_mode(const ExtConfig *cfg);
static void run_sweep_mode(const ExtConfig *cfg);

/* =========================================================================
 * main()
 * =========================================================================
 */
int main(int argc, char *argv[])
{
    ExtConfig cfg;
    memset(&cfg, 0, sizeof(ExtConfig));
    cfg.base.n_qubits = 64;

    int ret = parse_all_args(&cfg, argc, argv);
    if (ret != 0) return (ret > 0) ? 0 : 1;

    /* Seed PRNG once. */
    uint32_t seed = cfg.base.seed ? cfg.base.seed : (uint32_t)time(NULL);
    srand(seed);

    cli_print_banner(&cfg.base);

    /* ------------------------------------------------------------------ *
     * Dispatch on mode.                                                   *
     * ------------------------------------------------------------------ */

    /* Mode 1: photon-count sweep → CSV. */
    if (cfg.sweep_file[0] != '\0') {
        run_sweep_mode(&cfg);
        return 0;
    }

    /* Mode 2: Monte-Carlo statistics (+ optional CSV export). */
    if (cfg.stats_runs > 0) {
        run_stats_mode(&cfg);
        return 0;
    }

    /* Mode 3: Single or dual scenario. */
    if (cfg.base.verbose && cfg.base.n_qubits > 64) {
        printf("[WARN] -v with -n > 64 produces a very long table.\n\n");
    }

    if (cfg.base.both) {
        run_scenario(&cfg, 0, "No Eavesdropping  (clean channel)");
        run_scenario(&cfg, 1, "Eve Active        (intercept-resend)");
    } else {
        run_scenario(&cfg,
                     cfg.base.eve_active,
                     cfg.base.eve_active ? "Eve Active (intercept-resend)"
                                         : "No Eavesdropping (clean channel)");
    }
    return 0;
}

/* =========================================================================
 * run_scenario() — one complete BB84 exchange with all optional stages.
 * =========================================================================
 *
 * Full pipeline:
 *   [1] Alice prepares photons
 *   [2] Eve intercepts (optional)
 *   [3] Bob measures
 *   [4] Key sifting
 *   [5] Cascade error correction  (-c)
 *   [6] Privacy amplification     (-p)
 */
static void run_scenario(const ExtConfig *cfg, int eve_active,
                         const char *label)
{
    int n = cfg->base.n_qubits;

    printf("┌──────────────────────────────────────────────────────┐\n");
    printf("│  SCENARIO: %-42s│\n", label);
    printf("└──────────────────────────────────────────────────────┘\n\n");

    AliceData alice;
    BobData   bob;
    EveData   eve;
    eve.active = eve_active;

    /* Stage 1 */
    printf("[1] Alice preparing %d photons...\n", n);
    alice_generate(&alice, n);
    if (cfg->base.verbose) print_alice(&alice);

    /* Stage 2 */
    printf("[2] Quantum channel  (Eve %s)...\n",
           eve.active ? "INTERCEPTING ⚠" : "inactive ✓");
    eve_intercept(&eve, alice.photons, n);

    /* Stage 3 */
    printf("[3] Bob measuring photons...\n");
    bob_measure(&bob, alice.photons, n);
    if (cfg->base.verbose) print_bob(&bob);

    /* Stage 4 */
    printf("[4] Sifting key (public basis comparison)...\n");
    SiftedKey sifted = sift_key(&alice, &bob);
    print_sifted_key(&sifted);

    /* The key we'll feed into PA.  May be updated by Cascade. */
    const uint8_t *key_for_pa = sifted.key;
    int            len_for_pa = sifted.length;
    double         qber_for_pa = sifted.error_rate;
    int            extra_leaked = 0;    /* parity bits revealed by Cascade */

    /* Stage 5 — Cascade (optional) */
    CascadeResult cascade_res;
    if (cfg->do_cascade && sifted.length > 0) {
        printf("[5] Running Cascade error correction (%d passes)...\n",
               CASCADE_PASSES);
        cascade_res = cascade_reconcile(alice.bits, sifted.key,
                                        sifted.length,
                                        (sifted.error_rate > 0)
                                            ? sifted.error_rate : 0.11,
                                        CASCADE_PASSES);
        print_cascade_result(&cascade_res);

        /* After Cascade the effective QBER should be ~0, but PA must also
         * account for the parity bits Cascade sent over the public channel. */
        key_for_pa    = cascade_res.corrected_key;
        qber_for_pa   = cascade_res.residual_error_rate;
        extra_leaked  = cascade_res.parities_revealed;
        len_for_pa    = cascade_res.key_length;
    }

    /* Stage 6 — Privacy Amplification (optional) */
    if (cfg->do_pa && len_for_pa > 0) {
        /* Build a temporary SiftedKey from the (possibly Cascade-corrected)
         * key so that privacy_amplify() can consume it uniformly. */
        SiftedKey pa_input;
        memcpy(pa_input.key, key_for_pa, (size_t)len_for_pa);
        pa_input.length     = len_for_pa;
        pa_input.n_errors   = (int)(qber_for_pa * len_for_pa);
        pa_input.error_rate = qber_for_pa;

        int stage = cfg->do_cascade ? 6 : 5;
        printf("[%d] Privacy amplification (sec_param=%d, +%d parity bits)...\n",
               stage, SEC_PARAM, extra_leaked);

        /* Inflate the security parameter by extra_leaked to account for
         * parity bits Cascade revealed: final_len = n - t - s - extra. */
        AmplifiedKey amp = privacy_amplify(&pa_input, qber_for_pa,
                                           SEC_PARAM + extra_leaked);
        print_amplified_key(&amp);
    }

    printf("\n");
}

/* =========================================================================
 * run_stats_mode()
 * =========================================================================
 */
static void run_stats_mode(const ExtConfig *cfg)
{
    printf("┌──────────────────────────────────────────────────────┐\n");
    printf("│  MONTE-CARLO  %d trials × %d photons  Eve: %s        │\n",
           cfg->stats_runs, cfg->base.n_qubits,
           cfg->base.eve_active ? "YES" : "no ");
    printf("└──────────────────────────────────────────────────────┘\n\n");

    printf("Running %d trials... ", cfg->stats_runs);
    fflush(stdout);

    StatsReport report = stats_run(cfg->stats_runs,
                                   cfg->base.n_qubits,
                                   cfg->base.eve_active,
                                   SEC_PARAM);
    printf("done.\n");

    stats_print_report(&report);
    stats_print_histogram(&report, 15, 40);

    /* Optional CSV export. */
    if (cfg->export_file[0] != '\0') {
        export_trials_csv(&report, cfg->export_file);
    }
}

/* =========================================================================
 * run_sweep_mode()
 * =========================================================================
 */
static void run_sweep_mode(const ExtConfig *cfg)
{
    printf("┌──────────────────────────────────────────────────────┐\n");
    printf("│  PHOTON-COUNT SWEEP → %s\n", cfg->sweep_file);
    printf("│  n = %d…%d (%d steps), %d trials/point              │\n",
           SWEEP_N_MIN, SWEEP_N_MAX, SWEEP_STEPS, SWEEP_TRIALS);
    printf("└──────────────────────────────────────────────────────┘\n\n");

    export_sweep_csv(SWEEP_N_MIN, SWEEP_N_MAX, SWEEP_STEPS,
                     SWEEP_TRIALS, SEC_PARAM, cfg->sweep_file);
}

/* =========================================================================
 * parse_all_args()
 * =========================================================================
 */
static int parse_all_args(ExtConfig *cfg, int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_full_help(argv[0]); return 1;
        }
        else if (!strcmp(argv[i], "-e")) { cfg->base.eve_active = 1; }
        else if (!strcmp(argv[i], "-v")) { cfg->base.verbose    = 1; }
        else if (!strcmp(argv[i], "-b")) { cfg->base.both       = 1; }
        else if (!strcmp(argv[i], "-p")) { cfg->do_pa           = 1; }
        else if (!strcmp(argv[i], "-c")) { cfg->do_cascade      = 1; }

        else if (!strcmp(argv[i], "-n")) {
            if (++i >= argc) { fputs("-n needs a value.\n", stderr); return -1; }
            cfg->base.n_qubits = atoi(argv[i]);
            if (cfg->base.n_qubits <= 0 || cfg->base.n_qubits > MAX_QUBITS) {
                fprintf(stderr, "-n must be 1–%d.\n", MAX_QUBITS);
                return -1;
            }
        }
        else if (!strcmp(argv[i], "-s")) {
            if (++i >= argc) { fputs("-s needs a value.\n", stderr); return -1; }
            cfg->base.seed = (uint32_t)atoi(argv[i]);
        }
        else if (!strcmp(argv[i], "--stats")) {
            if (++i >= argc) { fputs("--stats needs a value.\n", stderr); return -1; }
            cfg->stats_runs = atoi(argv[i]);
            if (cfg->stats_runs <= 0 || cfg->stats_runs > MAX_TRIALS) {
                fprintf(stderr, "--stats must be 1–%d.\n", MAX_TRIALS);
                return -1;
            }
        }
        else if (!strcmp(argv[i], "--export")) {
            if (++i >= argc) { fputs("--export needs a filename.\n", stderr); return -1; }
            strncpy(cfg->export_file, argv[i], sizeof(cfg->export_file) - 1);
        }
        else if (!strcmp(argv[i], "--sweep")) {
            if (++i >= argc) { fputs("--sweep needs a filename.\n", stderr); return -1; }
            strncpy(cfg->sweep_file, argv[i], sizeof(cfg->sweep_file) - 1);
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_full_help(argv[0]);
            return -1;
        }
    }
    return 0;
}

/* =========================================================================
 * print_full_help()
 * =========================================================================
 */
static void print_full_help(const char *prog)
{
    printf(
        "\nUSAGE:  %s [OPTIONS]\n\n"
        "CORE:\n"
        "  -n <N>           Photons per run (1-%d, default: 64)\n"
        "  -e               Eve active (intercept-resend attack)\n"
        "  -b               Run both: clean channel AND Eve\n"
        "  -v               Verbose per-photon tables (use with -n ≤ 64)\n"
        "  -s <seed>        Fixed PRNG seed for reproducible runs\n\n"
        "POST-PROCESSING:\n"
        "  -c               Cascade error correction (before PA)\n"
        "  -p               Privacy amplification    (Toeplitz hashing)\n\n"
        "ANALYSIS:\n"
        "  --stats <N>      Monte-Carlo: N trials, print QBER report\n"
        "  --export <file>  Export per-trial CSV  (use after --stats)\n"
        "  --sweep <file>   Sweep n_qubits %d→%d, export summary CSV\n\n"
        "EXAMPLES:\n"
        "  %s -n 32 -b -v                    # both scenarios, verbose\n"
        "  %s -n 64 -b -c -p                 # + Cascade + PA\n"
        "  %s -n 256 --stats 1000 -e         # 1000-trial Eve analysis\n"
        "  %s -n 256 --stats 1000 --export out.csv\n"
        "  %s --sweep sweep.csv              # full photon-count sweep\n\n",
        prog, MAX_QUBITS, SWEEP_N_MIN, SWEEP_N_MAX,
        prog, prog, prog, prog, prog
    );
}
