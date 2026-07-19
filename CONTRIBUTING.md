# Contributing

Contributions should preserve the project's narrow audit semantics.

1. Open an issue describing the state, move, incremental objective, and independent reference objective involved.
2. Keep move enumeration deterministic and give every move a stable canonical key.
3. Add a mismatch test for incremental versus full evaluation.
4. Add an exactness test whenever state, depth, time, or storage budgets are changed.
5. Run the full CMake/CTest workflow before opening a pull request.

New adapters should be small enough to audit and should not add solver logic to the core library.
