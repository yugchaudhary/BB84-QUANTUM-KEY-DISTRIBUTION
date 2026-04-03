/* =========================================================================
 * app.js — BB84 QKD Simulation Engine + OTP Encryption + Smart Explainer
 * =========================================================================
 * v2 — adds text encryption/decryption, dual Eve comparison, dynamic
 *       physics explainer with interactive Q&A.
 * =========================================================================
 */

// ── Constants ──────────────────────────────────────────────────────────────
const BASIS_SYM = ['+', '×'];
const POLARISATION = [
    ['↔ H (0°)', '↕ V (90°)'],
    ['⤢ +45°', '⤡ −45°']
];

// ── Quantum Measurement (Born Rule) ────────────────────────────────────────
function measurePhoton(bit, photonBasis, measBasis) {
    if (photonBasis === measBasis) return bit;
    return Math.random() < 0.5 ? 0 : 1;
}

// ── Binary Entropy ─────────────────────────────────────────────────────────
function binaryEntropy(p) {
    if (p <= 0 || p >= 1) return 0;
    return -p * Math.log2(p) - (1 - p) * Math.log2(1 - p);
}

// ═══════════════════════════════════════════════════════════════════════════
// OTP PAYLOAD LAYER — Text ↔ Binary ↔ Encrypt/Decrypt
// ═══════════════════════════════════════════════════════════════════════════

function textToBinary(str) {
    const bits = [];
    for (let i = 0; i < str.length; i++) {
        const code = str.charCodeAt(i) & 0xFF;
        for (let b = 7; b >= 0; b--) bits.push((code >> b) & 1);
    }
    return bits;
}

function binaryToText(bits) {
    let str = '';
    for (let i = 0; i + 7 < bits.length; i += 8) {
        let byte = 0;
        for (let b = 0; b < 8; b++) byte = (byte << 1) | bits[i + b];
        if (byte >= 32 && byte <= 126) str += String.fromCharCode(byte);
        else str += String.fromCharCode(0x2592);  // ▒ for unprintable
    }
    return str;
}

function otpEncrypt(plainBits, keyBits) {
    const len = Math.min(plainBits.length, keyBits.length);
    const cipher = [];
    for (let i = 0; i < len; i++) cipher.push(plainBits[i] ^ keyBits[i]);
    return cipher;
}

function otpDecrypt(cipherBits, keyBits) {
    return otpEncrypt(cipherBits, keyBits);  // XOR is its own inverse
}

// ═══════════════════════════════════════════════════════════════════════════
// BB84 CORE ENGINE
// ═══════════════════════════════════════════════════════════════════════════

function runBB84(nQubits, eveActive) {
    const rb = () => Math.random() < 0.5 ? 0 : 1;

    const alice = { bits: [], bases: [], photons: [] };
    for (let i = 0; i < nQubits; i++) {
        alice.bits.push(rb());
        alice.bases.push(rb());
        alice.photons.push({ bit: alice.bits[i], basis: alice.bases[i] });
    }

    const eve = { active: eveActive, bases: [], bits: [] };
    if (eveActive) {
        for (let i = 0; i < nQubits; i++) {
            eve.bases.push(rb());
            eve.bits.push(measurePhoton(alice.photons[i].bit, alice.photons[i].basis, eve.bases[i]));
            alice.photons[i] = { bit: eve.bits[i], basis: eve.bases[i] };
        }
    }

    const bob = { bases: [], bits: [] };
    for (let i = 0; i < nQubits; i++) {
        bob.bases.push(rb());
        bob.bits.push(measurePhoton(alice.photons[i].bit, alice.photons[i].basis, bob.bases[i]));
    }

    const sifted = { aliceBits: [], bobBits: [], indices: [], errors: 0 };
    for (let i = 0; i < nQubits; i++) {
        if (alice.bases[i] === bob.bases[i]) {
            sifted.indices.push(i);
            sifted.aliceBits.push(alice.bits[i]);
            sifted.bobBits.push(bob.bits[i]);
            if (alice.bits[i] !== bob.bits[i]) sifted.errors++;
        }
    }
    const qber = sifted.aliceBits.length > 0 ? sifted.errors / sifted.aliceBits.length : 0;

    const n = sifted.aliceBits.length;
    const h2 = binaryEntropy(qber);
    const eveBits = n * h2;
    const secParam = 40;
    const finalLen = Math.max(0, Math.floor(n - eveBits - secParam));

    return {
        alice, bob, eve, sifted,
        qber, siftedLen: n, finalKeyLen: finalLen,
        eveBits, compressionRatio: n > 0 ? finalLen / n : 0
    };
}

// ── Monte Carlo ────────────────────────────────────────────────────────────
function runMonteCarlo(nTrials, nQubits, eveActive) {
    const qbers = [];
    let qberSum = 0, siftedSum = 0, finalSum = 0;
    for (let t = 0; t < nTrials; t++) {
        const r = runBB84(nQubits, eveActive);
        qbers.push(r.qber);
        qberSum += r.qber;
        siftedSum += r.siftedLen;
        finalSum += r.finalKeyLen;
    }
    const mean = qberSum / nTrials;
    let varSum = 0, min = Infinity, max = -Infinity;
    for (const q of qbers) {
        varSum += (q - mean) ** 2;
        if (q < min) min = q;
        if (q > max) max = q;
    }
    return {
        qbers, mean, stddev: Math.sqrt(varSum / nTrials), min, max,
        avgSifted: siftedSum / nTrials, avgFinal: finalSum / nTrials
    };
}

// ═══════════════════════════════════════════════════════════════════════════
// SMART EXPLAINER — Dynamic Physics Engine
// ═══════════════════════════════════════════════════════════════════════════

function generateExplainer(result, plaintext, bobDecrypted, eveActive) {
    const q = result.qber;
    const qPct = (q * 100).toFixed(2);
    const n = result.siftedLen;
    const errs = result.sifted.errors;
    let html = '';

    if (!eveActive) {
        html += `<p>✅ <strong>Clean Channel — No eavesdropper.</strong> Alice and Bob's bases matched
        on <strong>${n}</strong> of ${result.alice.bits.length} photons (~50%, as expected).
        QBER = <span class="formula">${qPct}%</span> — consistent with zero interception.</p>`;
        if (plaintext && bobDecrypted) {
            html += `<p>🔑 The One-Time Pad decryption succeeded perfectly because Alice and Bob
            share <em>identical</em> sifted keys. Each bit of ciphertext XOR'd with the matching
            key bit recovers the original plaintext.</p>`;
        }
    } else {
        html += `<p>🚨 <strong>Eve Intercepted!</strong> Eve measured each photon with a random basis,
        collapsing the quantum state. She guessed correctly only ~50% of the time.</p>`;
        html += `<p>For the ~50% she guessed wrong, Eve re-emitted a photon in the <em>wrong</em>
        eigenstate. When Bob later measures these (even in Alice's original basis), he gets a random
        result — introducing errors with probability <span class="formula">P(error) = ½ × ½ = ¼</span>.</p>`;
        html += `<p>Measured QBER = <span class="formula">${qPct}%</span>
        (theoretical: 25.0%). Errors found: <strong>${errs}</strong> out of <strong>${n}</strong> sifted bits.</p>`;
        if (plaintext && bobDecrypted) {
            html += `<p>📛 Bob's decrypted text is <strong>garbled</strong> because his key differs from
            Alice's at ~${qPct}% of positions. Each mismatched key-bit flips the corresponding plaintext
            bit, corrupting entire ASCII characters.</p>`;
        }
    }
    return html;
}

function getQAItems(result, eveActive) {
    const q = result.qber;
    const qPct = (q * 100).toFixed(2);
    const n = result.siftedLen;
    const errs = result.sifted.errors;

    return [
        {
            q: "Why did the text get garbled?",
            a: eveActive
                ? `Eve's intercept-resend attack caused <strong>${errs} bit errors</strong> in the
                   ${n}-bit sifted key. When Bob XOR-decrypts with his corrupted key, each wrong bit
                   flips the corresponding plaintext bit. Since ASCII uses 8 bits per character, even
                   a single bit-flip transforms the character entirely — producing unreadable symbols.
                   With QBER ≈ <span class="formula">${qPct}%</span>, roughly 1 in 4 characters
                   will be corrupted.`
                : `The text was <strong>NOT garbled</strong> — it decrypted perfectly. Without Eve,
                   Alice and Bob's sifted keys are identical (QBER = 0%), so every XOR operation
                   produces the correct plaintext bit.`
        },
        {
            q: "What is the No-Cloning Theorem?",
            a: `The <strong>No-Cloning Theorem</strong> (Wootters & Zurek, 1982) proves that it is
                impossible to create an identical copy of an arbitrary unknown quantum state. This is
                why Eve <em>cannot</em> simply copy Alice's photons — she must <strong>measure</strong>
                them, which collapses the superposition. Her measurement is irreversible and detectable.
                ${eveActive ? `<br><br>In this run, Eve's forced measurement introduced
                <span class="formula">${errs} detectable errors</span> — proof the theorem works.` : ''}`
        },
        {
            q: "Why is QBER ≈ 25% with Eve?",
            a: `<strong>Probability chain:</strong><br>
                ① Eve picks the <em>wrong</em> basis: <span class="formula">P = ½</span><br>
                ② Her measurement collapses the photon to a random state<br>
                ③ She re-sends a photon in her (wrong) basis<br>
                ④ Bob measures in Alice's original basis — but the photon is now in Eve's basis,
                so Bob gets a random result: <span class="formula">P(error) = ½</span><br><br>
                Combined: <span class="formula">P(error) = ½ × ½ = ¼ = 25%</span><br><br>
                This run measured <span class="formula">QBER = ${qPct}%</span> over ${n} sifted bits
                — ${Math.abs(q - 0.25) < 0.05 ? 'closely matching' : 'deviating from'} the theoretical prediction.`
        },
        {
            q: "What happens if Bob guesses the wrong basis?",
            a: `When Bob measures in the <em>wrong</em> basis, the photon is in a superposition
                relative to his measurement axis. By the <strong>Born Rule</strong>, it collapses
                randomly: <span class="formula">P(0) = P(1) = 0.5</span>.<br><br>
                This is expected and harmless — during <strong>key sifting</strong>, Alice and Bob
                publicly compare their bases (not bits!) and <strong>discard</strong> all positions
                where they disagree. Only matching-basis positions survive into the sifted key.
                In this run, ${result.alice.bits.length - n} photons were discarded (~50%).`
        },
        {
            q: "How does One-Time Pad (OTP) encryption work?",
            a: `The OTP is the only encryption scheme with <strong>perfect secrecy</strong>
                (Shannon, 1949). Rules:<br>
                ① Key must be <em>truly random</em> (BB84 provides this via quantum randomness)<br>
                ② Key must be <em>at least as long</em> as the message<br>
                ③ Key must <em>never be reused</em><br><br>
                <strong>Encryption:</strong> <span class="formula">C = M ⊕ K</span> (XOR each bit)<br>
                <strong>Decryption:</strong> <span class="formula">M = C ⊕ K</span> (XOR again)<br><br>
                If all three rules hold, the ciphertext is statistically independent of the plaintext —
                no computational power can break it. BB84 + OTP = information-theoretically secure communication.`
        }
    ];
}

// ═══════════════════════════════════════════════════════════════════════════
// UI CONTROLLER
// ═══════════════════════════════════════════════════════════════════════════

let mcChart = null;
let lastResult = null;

function $(id) { return document.getElementById(id); }

function initControls() {
    const slider = $('nQubits');
    const display = $('nQubitsVal');
    slider.addEventListener('input', () => { display.textContent = slider.value; });

    const toggle = $('eveToggle');
    const label = $('eveLabel');
    toggle.addEventListener('click', () => {
        toggle.classList.toggle('active');
        const on = toggle.classList.contains('active');
        label.textContent = on ? 'EVE ON ⚠' : 'Eve Off';
        label.className = 'toggle-label' + (on ? ' eve-on' : '');
    });

    $('runBtn').addEventListener('click', runSingle);
    $('encryptBtn').addEventListener('click', runEncrypt);
    $('compareBtn').addEventListener('click', runDualScenario);
    $('mcBtn').addEventListener('click', runMC);
}

// ── Animate Flow ──────────────────────────────────────────────────────────
function animateFlow(eveActive) {
    const nodes = ['flowAlice', 'flowEve', 'flowBob', 'flowSift'];
    nodes.forEach(id => $(id).classList.remove('active'));
    $('flowEve').className = 'flow-node eve' + (eveActive ? ' active' : ' inactive');
    let delay = 0;
    for (const id of nodes) {
        setTimeout(() => $(id).classList.add('active'), delay);
        delay += 300;
    }
}

// ── QBER Gauge ────────────────────────────────────────────────────────────
function updateGauge(qber) {
    const pct = Math.min(qber * 100, 50);
    const bar = $('gaugeBar');
    const val = $('gaugeValue');
    const assess = $('assessment');

    bar.style.width = (pct / 50 * 100) + '%';
    val.textContent = (qber * 100).toFixed(2) + '%';

    if (qber < 0.01) {
        bar.style.background = 'linear-gradient(90deg, #10b981, #34d399)';
        bar.style.boxShadow = '0 0 12px rgba(16,185,129,0.5)';
        val.style.color = '#10b981';
        assess.className = 'assessment assess-secure';
        assess.textContent = '🔒 SECURE — No eavesdropper detected';
    } else if (qber < 0.11) {
        bar.style.background = 'linear-gradient(90deg, #f59e0b, #fbbf24)';
        bar.style.boxShadow = '0 0 12px rgba(245,158,11,0.5)';
        val.style.color = '#f59e0b';
        assess.className = 'assessment assess-caution';
        assess.textContent = '⚠ CAUTION — Elevated error rate, possible noise';
    } else {
        bar.style.background = 'linear-gradient(90deg, #ef4444, #f87171)';
        bar.style.boxShadow = '0 0 12px rgba(239,68,68,0.5)';
        val.style.color = '#ef4444';
        assess.className = 'assessment assess-warning';
        assess.textContent = '🚨 WARNING — Eavesdropper likely! QBER ≈ 25%';
    }
}

// ── Stats / Photon Table / Key Display ────────────────────────────────────
function updateStats(result) {
    $('statPhotons').textContent = result.alice.bits.length;
    $('statSifted').textContent = result.siftedLen;
    $('statErrors').textContent = result.sifted.errors;
    $('statFinalKey').textContent = result.finalKeyLen;
}

function renderPhotonTable(result) {
    const tbody = $('photonBody');
    const eveActive = result.eve.active;
    tbody.innerHTML = '';

    const n = result.alice.bits.length;
    const maxRows = Math.min(n, 200);

    for (let i = 0; i < maxRows; i++) {
        const tr = document.createElement('tr');
        const basisMatch = result.alice.bases[i] === result.bob.bases[i];
        const bitMatch = result.alice.bits[i] === result.bob.bits[i];

        let eveCells = eveActive
            ? `<td class="eve-col">${BASIS_SYM[result.eve.bases[i]]}</td>
               <td class="eve-col">${result.eve.bits[i]}</td>`
            : `<td style="opacity:0.2">—</td><td style="opacity:0.2">—</td>`;

        const matchClass = basisMatch ? 'basis-match' : 'basis-miss';
        const matchIcon = basisMatch ? '✓' : '✗';
        const bitClass = basisMatch ? (bitMatch ? 'bit-ok' : 'bit-error') : 'basis-miss';
        const bitIcon = basisMatch ? (bitMatch ? '✓' : '✗ ERR') : '—';

        tr.innerHTML = `
            <td style="color:var(--text-dim)">${i + 1}</td>
            <td>${result.alice.bits[i]}</td>
            <td>${BASIS_SYM[result.alice.bases[i]]}</td>
            <td>${POLARISATION[result.alice.bases[i]][result.alice.bits[i]]}</td>
            ${eveCells}
            <td>${BASIS_SYM[result.bob.bases[i]]}</td>
            <td>${result.bob.bits[i]}</td>
            <td class="${matchClass}">${matchIcon}</td>
            <td class="${bitClass}">${bitIcon}</td>`;
        tbody.appendChild(tr);
    }
    if (n > maxRows) {
        const tr = document.createElement('tr');
        tr.innerHTML = `<td colspan="11" style="color:var(--text-dim);text-align:center">
            … and ${n - maxRows} more photons</td>`;
        tbody.appendChild(tr);
    }
}

function renderKey(bits, containerId) {
    const el = $(containerId);
    let html = '';
    for (let i = 0; i < bits.length; i++) {
        html += `<span class="key-bit ${bits[i] ? 'one' : 'zero'}">${bits[i]}</span>`;
        if ((i + 1) % 8 === 0) html += '<span class="key-separator"></span>';
    }
    el.innerHTML = html || '<span style="color:var(--text-dim)">No key generated</span>';
}

// ── Run Single Simulation ──────────────────────────────────────────────────
function runSingle() {
    const n = parseInt($('nQubits').value);
    const eveActive = $('eveToggle').classList.contains('active');

    animateFlow(eveActive);
    const result = runBB84(n, eveActive);
    lastResult = result;

    updateStats(result);
    updateGauge(result.qber);
    renderPhotonTable(result);
    renderKey(result.sifted.aliceBits, 'siftedKeyDisplay');

    $('paInfo').innerHTML = `
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px;font-size:0.85rem">
            <div>Sifted length: <strong>${result.siftedLen}</strong> bits</div>
            <div>Eve's info bound: <strong>${result.eveBits.toFixed(1)}</strong> bits</div>
            <div>Final key length: <strong>${result.finalKeyLen}</strong> bits</div>
            <div>Compression: <strong>${(result.compressionRatio * 100).toFixed(1)}%</strong></div>
        </div>`;

    // Update explainer
    $('dynamicExplainer').innerHTML = generateExplainer(result, null, null, eveActive);
    renderQA(result, eveActive);

    $('resultsSection').style.display = 'block';
    $('resultsSection').scrollIntoView({ behavior: 'smooth', block: 'start' });
}

// ═══════════════════════════════════════════════════════════════════════════
// ENCRYPT MODE — Single scenario with OTP
// ═══════════════════════════════════════════════════════════════════════════

function runEncrypt() {
    const n = parseInt($('nQubits').value);
    const eveActive = $('eveToggle').classList.contains('active');
    const plaintext = $('msgInput').value || 'Hello Bob';

    animateFlow(eveActive);
    const result = runBB84(n, eveActive);
    lastResult = result;

    updateStats(result);
    updateGauge(result.qber);
    renderPhotonTable(result);
    renderKey(result.sifted.aliceBits, 'siftedKeyDisplay');

    // OTP Encryption
    const plainBits = textToBinary(plaintext);
    const aliceKey = result.sifted.aliceBits;
    const bobKey = result.sifted.bobBits;
    const usableLen = Math.min(plainBits.length, aliceKey.length);

    let bobDecrypted = '';
    let cipherBits = [];

    if (usableLen < 8) {
        $('otpResult').innerHTML = `<div style="color:var(--accent-amber);text-align:center;padding:20px">
            ⚠ Sifted key too short (${aliceKey.length} bits) to encrypt "${plaintext}" (needs ${plainBits.length} bits).
            Increase photon count.</div>`;
    } else {
        const truncPlain = plainBits.slice(0, usableLen);
        cipherBits = otpEncrypt(truncPlain, aliceKey.slice(0, usableLen));
        const decBits = otpDecrypt(cipherBits, bobKey.slice(0, usableLen));
        bobDecrypted = binaryToText(decBits);

        const charsEncrypted = Math.floor(usableLen / 8);
        const truncMsg = plaintext.substring(0, charsEncrypted);

        const isClean = !eveActive;
        const outClass = isClean ? 'clean-text' : 'garbled-text';
        const outLabel = isClean ? '✅ Perfect Decryption' : '❌ Garbled Output';

        $('otpResult').innerHTML = `
            <div class="otp-pipeline">
                <div class="otp-stage">
                    <div class="otp-stage-label">Alice's Plaintext</div>
                    <div class="otp-stage-value" style="color:var(--accent-blue)">"${truncMsg}"</div>
                </div>
                <div class="otp-arrow">⊕ →</div>
                <div class="otp-stage">
                    <div class="otp-stage-label">Ciphertext (${cipherBits.length} bits)</div>
                    <div class="otp-stage-value" style="font-size:0.7rem;color:var(--accent-amber)">${cipherBits.slice(0, 64).join('')}${cipherBits.length > 64 ? '…' : ''}</div>
                </div>
                <div class="otp-arrow">⊕ →</div>
                <div class="otp-stage">
                    <div class="otp-stage-label">${outLabel}</div>
                    <div class="${outClass}" style="font-size:1.1rem">"${bobDecrypted}"</div>
                </div>
            </div>
            <div style="text-align:center;font-size:0.78rem;color:var(--text-dim);margin-top:8px">
                Key used: ${usableLen} bits of ${aliceKey.length}-bit sifted key · ${charsEncrypted} characters encrypted
            </div>`;
    }

    $('otpSection').style.display = 'block';

    // Update explainer with encryption context
    $('dynamicExplainer').innerHTML = generateExplainer(result, plaintext, bobDecrypted, eveActive);
    renderQA(result, eveActive);

    $('paInfo').innerHTML = `
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px;font-size:0.85rem">
            <div>Sifted length: <strong>${result.siftedLen}</strong> bits</div>
            <div>Eve's info bound: <strong>${result.eveBits.toFixed(1)}</strong> bits</div>
            <div>Final key length: <strong>${result.finalKeyLen}</strong> bits</div>
            <div>Compression: <strong>${(result.compressionRatio * 100).toFixed(1)}%</strong></div>
        </div>`;

    $('resultsSection').style.display = 'block';
    $('otpSection').scrollIntoView({ behavior: 'smooth', block: 'start' });
}

// ═══════════════════════════════════════════════════════════════════════════
// DUAL SCENARIO — Clean vs Eve side-by-side
// ═══════════════════════════════════════════════════════════════════════════

function runDualScenario() {
    const n = parseInt($('nQubits').value);
    const plaintext = $('msgInput').value || 'Hello Bob';

    const cleanR = runBB84(n, false);
    const eveR = runBB84(n, true);

    const plainBits = textToBinary(plaintext);

    function decrypt(result) {
        const len = Math.min(plainBits.length, result.sifted.aliceBits.length);
        if (len < 8) return { text: '(key too short)', chars: 0 };
        const cipher = otpEncrypt(plainBits.slice(0, len), result.sifted.aliceBits.slice(0, len));
        const dec = otpDecrypt(cipher, result.sifted.bobBits.slice(0, len));
        return { text: binaryToText(dec), chars: Math.floor(len / 8) };
    }

    const cleanDec = decrypt(cleanR);
    const eveDec = decrypt(eveR);

    $('dualPanel').innerHTML = `
        <div class="dual-card clean">
            <div class="dual-card-header">✅ Scenario A — No Eavesdropper</div>
            <div class="dual-row"><span class="dual-row-label">Alice sends:</span>
                <span class="dual-row-value">"${plaintext}"</span></div>
            <div class="dual-row"><span class="dual-row-label">Photons:</span>
                <span class="dual-row-value">${n}</span></div>
            <div class="dual-row"><span class="dual-row-label">Sifted key:</span>
                <span class="dual-row-value">${cleanR.siftedLen} bits</span></div>
            <div class="dual-row"><span class="dual-row-label">QBER:</span>
                <span class="dual-row-value stat-green">${(cleanR.qber * 100).toFixed(2)}%</span></div>
            <div class="dual-row"><span class="dual-row-label">Key errors:</span>
                <span class="dual-row-value">${cleanR.sifted.errors}</span></div>
            <div style="margin-top:16px">
                <div style="font-size:0.72rem;text-transform:uppercase;letter-spacing:1px;color:var(--text-dim);margin-bottom:8px">Bob decrypts:</div>
                <div class="clean-text">"${cleanDec.text}"</div>
            </div>
        </div>
        <div class="dual-card eve">
            <div class="dual-card-header">🚨 Scenario B — Eve Intercepts</div>
            <div class="dual-row"><span class="dual-row-label">Alice sends:</span>
                <span class="dual-row-value">"${plaintext}"</span></div>
            <div class="dual-row"><span class="dual-row-label">Photons:</span>
                <span class="dual-row-value">${n}</span></div>
            <div class="dual-row"><span class="dual-row-label">Sifted key:</span>
                <span class="dual-row-value">${eveR.siftedLen} bits</span></div>
            <div class="dual-row"><span class="dual-row-label">QBER:</span>
                <span class="dual-row-value stat-red">${(eveR.qber * 100).toFixed(2)}%</span></div>
            <div class="dual-row"><span class="dual-row-label">Key errors:</span>
                <span class="dual-row-value">${eveR.sifted.errors}</span></div>
            <div style="margin-top:16px">
                <div style="font-size:0.72rem;text-transform:uppercase;letter-spacing:1px;color:var(--text-dim);margin-bottom:8px">Bob decrypts:</div>
                <div class="garbled-text">"${eveDec.text}"</div>
            </div>
        </div>`;

    $('dualSection').style.display = 'block';

    // Update explainer for Eve scenario
    $('dynamicExplainer').innerHTML = generateExplainer(eveR, plaintext, eveDec.text, true);
    renderQA(eveR, true);

    $('dualSection').scrollIntoView({ behavior: 'smooth', block: 'start' });
}

// ═══════════════════════════════════════════════════════════════════════════
// Q&A RENDERER
// ═══════════════════════════════════════════════════════════════════════════

function renderQA(result, eveActive) {
    const items = getQAItems(result, eveActive);
    const container = $('qaList');
    container.innerHTML = '';

    for (const item of items) {
        const div = document.createElement('div');
        div.className = 'qa-item';
        div.innerHTML = `
            <button class="qa-question">
                <span class="qa-icon">▶</span>
                ${item.q}
            </button>
            <div class="qa-answer">${item.a}</div>`;
        div.querySelector('.qa-question').addEventListener('click', () => {
            div.classList.toggle('active');
        });
        container.appendChild(div);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// MONTE CARLO
// ═══════════════════════════════════════════════════════════════════════════

function runMC() {
    const n = parseInt($('nQubits').value);
    const trials = parseInt($('mcTrials').value) || 500;

    $('mcStatus').textContent = `Running ${trials} trials...`;

    setTimeout(() => {
        const clean = runMonteCarlo(trials, n, false);
        const withEve = runMonteCarlo(trials, n, true);

        $('mcStatus').innerHTML = `
           <div class="grid-3" style="margin-top:12px">
               <div class="card stat-card">
                   <div class="stat-value stat-green">${(clean.mean * 100).toFixed(2)}%</div>
                   <div class="stat-label">Mean QBER (No Eve)</div>
               </div>
               <div class="card stat-card">
                   <div class="stat-value stat-red">${(withEve.mean * 100).toFixed(2)}%</div>
                   <div class="stat-label">Mean QBER (Eve)</div>
               </div>
               <div class="card stat-card">
                   <div class="stat-value stat-amber">${(withEve.stddev * 100).toFixed(2)}%</div>
                   <div class="stat-label">Std Dev (Eve)</div>
               </div>
           </div>`;

        renderHistogram(clean.qbers, withEve.qbers);
        $('mcSection').style.display = 'block';
    }, 50);
}

function renderHistogram(cleanQbers, eveQbers) {
    const bins = 20, range = 0.5, binW = range / bins;
    function toBins(arr) {
        const counts = new Array(bins).fill(0);
        for (const q of arr) { let idx = Math.floor(q / binW); if (idx >= bins) idx = bins - 1; counts[idx]++; }
        return counts;
    }
    const labels = [];
    for (let i = 0; i < bins; i++) labels.push(`${(i * binW * 100).toFixed(0)}-${((i + 1) * binW * 100).toFixed(0)}%`);

    const ctx = $('mcChart').getContext('2d');
    if (mcChart) mcChart.destroy();

    mcChart = new Chart(ctx, {
        type: 'bar',
        data: {
            labels, datasets: [
                {
                    label: 'No Eve (clean)', data: toBins(cleanQbers), backgroundColor: 'rgba(16,185,129,0.6)',
                    borderColor: '#10b981', borderWidth: 1, borderRadius: 4
                },
                {
                    label: 'Eve Active', data: toBins(eveQbers), backgroundColor: 'rgba(239,68,68,0.6)',
                    borderColor: '#ef4444', borderWidth: 1, borderRadius: 4
                }
            ]
        },
        options: {
            responsive: true, maintainAspectRatio: false,
            plugins: {
                legend: { labels: { color: '#94a3b8', font: { size: 12 } } },
                title: { display: true, text: 'QBER Frequency Distribution', color: '#f0f4ff', font: { size: 14, weight: 'bold' } }
            },
            scales: {
                x: { ticks: { color: '#64748b', font: { size: 10 } }, grid: { color: 'rgba(255,255,255,0.04)' } },
                y: {
                    ticks: { color: '#64748b' }, grid: { color: 'rgba(255,255,255,0.04)' },
                    title: { display: true, text: 'Frequency', color: '#94a3b8' }
                }
            }
        }
    });
}

// ── Init ───────────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
    initControls();
    initChat();
});

// ═══════════════════════════════════════════════════════════════════════════
// AI CHAT CONTROLLER — Gemini API Integration
// ═══════════════════════════════════════════════════════════════════════════

function initChat() {
    const fab = $('chatFab');
    const widget = $('chatWidget');
    const toggle = $('chatToggle');
    const input = $('chatInput');
    const sendBtn = $('chatSend');
    const setKeyBtn = $('setApiKeyBtn');
    const apiModal = $('apiModal');
    const closeModal = $('closeModal');
    const saveKeyBtn = $('saveApiKey');
    const keyInput = $('apiKeyInput');

    // Toggle Chat
    fab.addEventListener('click', () => {
        widget.classList.add('active');
        fab.style.display = 'none';
        input.focus();
    });

    toggle.addEventListener('click', () => {
        widget.classList.remove('active');
        fab.style.display = 'flex';
    });

    // Modal
    setKeyBtn.addEventListener('click', () => {
        keyInput.value = localStorage.getItem('gemini_api_key') || '';
        apiModal.classList.add('active');
    });

    closeModal.addEventListener('click', () => apiModal.classList.remove('active'));

    saveKeyBtn.addEventListener('click', () => {
        const key = keyInput.value.trim();
        if (key) {
            localStorage.setItem('gemini_api_key', key);
            apiModal.classList.remove('active');
            addChatMessage('ai', '✅ API Key saved! I\'m ready to help.');
        }
    });

    // Send Message
    const handleSend = () => {
        const text = input.value.trim();
        if (!text) return;
        input.value = '';
        input.style.height = 'auto';
        processUserMessage(text);
    };

    sendBtn.addEventListener('click', handleSend);
    input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            handleSend();
        }
    });

    // Auto-resize textarea
    input.addEventListener('input', () => {
        input.style.height = 'auto';
        input.style.height = input.scrollHeight + 'px';
    });
}

function addChatMessage(role, text) {
    const container = $('chatMessages');
    const div = document.createElement('div');
    div.className = `message ${role}`;

    // Simple markdown-ish rendering for bold and code
    let html = text
        .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
        .replace(/`(.*?)`/g, '<code>$1</code>')
        .replace(/\n/g, '<br>');

    div.innerHTML = html;
    container.appendChild(div);
    container.scrollTop = container.scrollHeight;
    return div;
}

function addLoadingIndicator() {
    const container = $('chatMessages');
    const div = document.createElement('div');
    div.className = 'message ai loading';
    div.innerHTML = '<div class="dot"></div><div class="dot"></div><div class="dot"></div>';
    container.appendChild(div);
    container.scrollTop = container.scrollHeight;
    return div;
}

async function processUserMessage(text) {
    addChatMessage('user', text);

    const apiKey = localStorage.getItem('gemini_api_key');
    if (!apiKey) {
        setTimeout(() => {
            addChatMessage('ai', '⚠️ No API Key found. Please click "Set Gemini API Key" at the bottom to enable the real AI.');
        }, 500);
        return;
    }

    const loading = addLoadingIndicator();

    try {
        const response = await callGemini(text, apiKey);
        loading.remove();
        addChatMessage('ai', response);
    } catch (err) {
        loading.remove();
        addChatMessage('ai', '❌ Error: ' + err.message);
    }
}

async function callGemini(userPrompt, apiKey) {
    // Build context string from current simulation state
    let context = "Context: You are a physics expert assistant integrated into a BB84 QKD interactive simulator.\n";
    if (lastResult) {
        context += `Current Simulation State:\n`;
        context += `- Photons: ${lastResult.alice.bits.length}\n`;
        context += `- Eve Active: ${lastResult.eve.active}\n`;
        context += `- Sifted Key Length: ${lastResult.siftedLen}\n`;
        context += `- QBER: ${(lastResult.qber * 100).toFixed(2)}%\n`;
        context += `- Errors Found: ${lastResult.sifted.errors}\n`;
        context += `- Final Key Length: ${lastResult.finalKeyLen}\n`;
    } else {
        context += "No simulation has been run yet.\n";
    }

    const systemPrompt = `You are a helpful, brilliant Quantum Cryptography expert. 
    Explain concepts simply but accurately. Use the context provided about the current simulation state 
    to answer the user's specific doubts. Keep responses concise and formatted with markdown (bold, code blocks).
    If a user asks about the results on their screen, use the Context provided.`;

    const url = `https://generativelanguage.googleapis.com/v1/models/gemini-1.5-flash:generateContent?key=${apiKey}`;

    const payload = {
        contents: [{
            parts: [{
                text: `${systemPrompt}\n\n${context}\n\nUser Question: ${userPrompt}`
            }]
        }],
        generationConfig: {
            temperature: 0.7,
            topK: 40,
            topP: 0.95,
            maxOutputTokens: 1024,
        }
    };

    const response = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    });

    const data = await response.json();
    if (data.error) throw new Error(data.error.message);
    return data.candidates[0].content.parts[0].text;
}
