# BB84 Quantum Key Distribution (QKD) — Interactive Simulation

This project implements the **BB84 protocol**, including an interactive web-based dashboard and a high-performance C simulation engine. It's designed for educational and research purposes in the field of quantum cryptography.

## 🚀 Two Ways to Explore

### 1. Interactive Dashboard (HTML/JS)
A visual, dark-themed dashboard that allows you to:
-   **Simulate the BB84 Flow:** See Alice and Bob exchanging polarized photons.
-   **Toggle Eve (Eavesdropper):** Instantly visualize how interception affects the Quantum Bit Error Rate (QBER).
-   **One-Time Pad Encryption:** Use the generated quantum key to encrypt and decrypt text.
-   **Statistical Analysis:** Run Monte-Carlo trials and view the error distribution.

**To run:** Open `dashboard/index.html` in your web browser.

### 2. Simulation Engine (C CLI)
A command-line tool for large-scale simulations and detailed statistical reports.
-   **Features:** Cascade Error Correction, Privacy Amplification, and CSV exports.
-   **Usage:**
    ```bash
    make
    ./bb84_sim --help
    ```

## 🛠️ Requirements
-   **Compiler:** `gcc` or any C11-compliant compiler.
-   **Build System:** `make`.
-   **Browser:** Any modern browser with ES6 support.

## 📂 Project Structure
-   `/dashboard`: Core web UI and JavaScript engine (`app.js`).
-   `main.c`, `bb84.c`: Core logic of the QKD protocol.
-   `cascade.c`: Error correction through the Cascade algorithm.
-   `privacy_amp.c`: Privacy amplification layer.
-   `stats.c`: Tools for Monte-Carlo simulations.
-   `Makefile`: Build automation.

---
*Created as part of an Engineering Physics Project.*
