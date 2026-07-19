# Design

Landscape Audit is state-centered. It answers a narrow question: what can be certified about the exact local neighborhood of this candidate?

## Core objects

The v0.1 adapter contract separates five responsibilities without putting dynamic dispatch in the move loop:

1. The adapter's `State` type owns exact serialization and deserialization.
2. `moves`, `move_key`, and `apply` define a deterministic neighborhood.
3. `delta` is the incremental objective path.
4. `full_evaluate` is the reference path and should not read incremental caches.
5. The v0.1 improvement policy is strict minimization of a signed integer objective.

The integer policy is intentionally narrow. A future policy type may support lexicographic values without changing the meaning of existing certificates.

## Determinism

Moves are sorted by their canonical keys. Worker threads claim positions in that sorted list and write to preallocated slots. Reduction, graph construction, and hashing happen in list order. The enumeration digest is computed from sorted records, so the certificate is independent of scheduling and thread count.

## Neutral enumeration

Starting at objective value `f(seed)`, BFS only adds states with the same value. Every outgoing move from every expanded plateau state is still evaluated. A negative delta marks an improving exit. A positive delta contributes to the exit spectrum but is not added to the plateau.

The state limit counts expanded states. The checkpoint contains discovered states, the unexpanded frontier, aggregate counters, histograms, and graph edges. Resume validates the adapter identity, neighborhood, seed hash, and baseline objective before continuing.

## Radius two

The adapter owns radius-two enumeration. The core never assumes that two atomic moves commute or remain valid after one another.

## Trust boundary

The core can detect inconsistencies between `delta` and `full_evaluate`, corrupted replay artifacts, incomplete checkpoints, and nondeterministic outputs covered by tests. It cannot establish that an adapter's independently written full evaluator correctly expresses an external mathematical problem. That link remains the adapter author's responsibility.
