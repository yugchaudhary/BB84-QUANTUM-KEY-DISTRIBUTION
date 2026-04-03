/**
 * stats.h — Statistical Analysis Engine
 * =======================================
 * Engineering Physics Project — BB84 QKD Simulator
 *
 * Runs multiple BB84 exchanges and computes aggregate statistics on:
 *   - QBER distribution (mean, variance, min, max)
 *   - Key length vs. photon count
 *   - Privacy amplification compression ratio
 *
 * Also renders a simple ASCII histogram of QBER frequency to the terminal.
 */

#ifndef STATS_H
#define STATS_H

/* Maximum number of Monte-Carlo trials in a single statistics run. */
#define MAX_TRIALS 10000

/**
 * TrialResult — outcome of a single BB84 trial.
 */
typedef struct {
    int    n_qubits;        /**< Photons sent this trial                    */
    int    sifted_length;   /**< Bits in the sifted key                     */
    int    n_errors;        /**< Errors detected in matched positions        */
    double qber;            /**< Quantum Bit Error Rate for this trial       */
    int    final_key_len;   /**< Bits remaining after privacy amplification  */
} TrialResult;

/**
 * StatsReport — aggregate statistics over all trials.
 */
typedef struct {
    int    n_trials;          /**< Number of trials performed                 */
    int    n_qubits;          /**< Photons per trial                          */
    int    eve_active;        /**< 1 = Eve was present, 0 = clean channel     */
    double qber_mean;         /**< Mean QBER across all trials                */
    double qber_variance;     /**< Variance of QBER                           */
    double qber_min;          /**< Minimum QBER observed                      */
    double qber_max;          /**< Maximum QBER observed                      */
    double avg_sifted_len;    /**< Average sifted key length                  */
    double avg_final_len;     /**< Average final key length after PA          */
    double avg_compression;   /**< Average privacy amplification compression  */
    TrialResult trials[MAX_TRIALS]; /**< Per-trial raw results               */
} StatsReport;

/**
 * stats_run() — execute n_trials BB84 exchanges and collect results.
 *
 * @param n_trials   Number of Monte-Carlo repetitions (1 – MAX_TRIALS).
 * @param n_qubits   Photons per trial.
 * @param eve_active 1 to have Eve intercept in every trial.
 * @param sec_param  Privacy amplification security parameter (e.g., 40).
 * @return           Fully populated StatsReport.
 */
StatsReport stats_run(int n_trials, int n_qubits, int eve_active, int sec_param);

/**
 * stats_print_report() — display the aggregate statistics table.
 */
void stats_print_report(const StatsReport *report);

/**
 * stats_print_histogram() — render an ASCII histogram of QBER values.
 *
 * @param report  The StatsReport to visualise.
 * @param bins    Number of histogram bins (recommended: 10–20).
 * @param width   Max bar width in characters (recommended: 40).
 */
void stats_print_histogram(const StatsReport *report, int bins, int width);

#endif /* STATS_H */
