# Replayable computational certificates

Schema: `landscape-audit-certificate-v1`.

Certificates summarize deterministic computation. They are designed for replay and audit, but they are not proof-assistant terms or general formal proofs.

## Exactness

An exact plateau certificate requires all discovered neutral states to be expanded and every adapter-defined move to be evaluated. It has:

```json
{
  "termination_reason": "exhausted",
  "exact": true
}
```

If the expanded-state budget is reached with a nonempty frontier, the result has `exact: false`, `termination_reason: "state_limit"`, and `plateau_local_optimum: false` regardless of the exits observed so far.

## Digest

For every enumerated move, the core forms a record containing:

```text
source_state_sha256<TAB>canonical_move_key<TAB>target_state_sha256<TAB>source_value<TAB>target_value<LF>
```

Records are sorted bytewise, concatenated, and hashed with SHA-256. Replay recomputes the full enumeration and compares the canonical certificate bytes.

## Reference evaluation

With the default verification setting, the target value predicted by `delta` must equal `full_evaluate(apply(state, move))` for every enumerated move. A mismatch aborts the run; no certificate is emitted.

## Exit spectrum

`delta_histogram` is a local exit-delta spectrum. It is not a global barrier-height calculation. A true barrier requires a minimax path search over a larger state graph and is outside v0.1.
