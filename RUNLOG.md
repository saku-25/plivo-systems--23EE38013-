# RUNLOG.md - Flaky Network Experiments

| # | Profile | delay_ms | Miss % | Overhead | Changes Made & Why |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | A.json | 40 | 8.13% | 1.02x | **Baseline C run.** Naive send/recv. Dropped packets caused silent failures because there was no protection. |
| 2 | A.json | 40 | 9.80% | 2.15x | **Added Basic ARQ (ACKs + Retransmissions).** ACKs were too large causing overhead to exceed 2.0x cap. Timeout was too slow. |
| 3 | A.json | 20 | 100.0% | 1.62x | **Pure FEC + Non-blocking Receiver.** Switched receiver to non-blocking I/O. However, 20ms was too tight for the relay's jitter spikes. |
| 4 | A.json | 43 | **0.60%** | **1.62x** | **Final Valid Run.** Increased delay to 43ms and implemented an **overlapping 3-frame FEC scheme** (sending parity for [0,1] and [1,2]). The overlapping parity survives 2 consecutive packet drops, keeping misses safely under the 1% cap. Bandwidth remains perfect at 1.62x. |
