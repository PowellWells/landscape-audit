#include "adapters.hpp"
#include "landscape_audit/audit.hpp"

#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void neutral_trap_distinguishes_point_and_plateau() {
    const examples::NeutralTrap adapter;
    const auto result = landscape_audit::audit_plateau(adapter, adapter.seed(), 1);
    require(result.exact, "neutral trap should be exact");
    require(result.point_local_optimum, "seed should be point-local-optimal");
    require(!result.plateau_local_optimum, "neutral plateau has a hidden improving exit");
    require(result.neutral_state_count == 2, "neutral plateau should have two states");
    require(result.minimum_delta == -1, "hidden exit delta should be -1");
}

void state_budget_never_certifies_truncated_plateau() {
    const examples::NeutralTrap adapter;
    landscape_audit::AuditOptions options;
    options.max_states = 1;
    const auto result = landscape_audit::audit_plateau(adapter, adapter.seed(), 1, options);
    require(!result.exact, "truncated enumeration must not be exact");
    require(result.termination_reason == "state_limit", "termination reason should be explicit");
    require(!result.plateau_local_optimum, "truncated enumeration must not certify the plateau");
}

void threads_are_deterministic() {
    const examples::GraphColoring adapter;
    landscape_audit::AuditOptions serial;
    serial.threads = 1;
    serial.max_states = 200;
    landscape_audit::AuditOptions parallel = serial;
    parallel.threads = 4;
    const auto first = landscape_audit::audit_plateau(adapter, adapter.seed(), 1, serial);
    const auto second = landscape_audit::audit_plateau(adapter, adapter.seed(), 1, parallel);
    require(landscape_audit::certificate_json(first) == landscape_audit::certificate_json(second), "thread count changed certificate bytes");
}

void radius_two_is_adapter_defined_not_cartesian_runtime_composition() {
    const examples::MaxSat adapter;
    const auto moves = adapter.moves(adapter.seed(), 2);
    require(moves.size() == 6, "four variables should expose six canonical pair flips");
    for (const auto& move : moves) require(move.first < move.second, "pair flips should be canonical");
}

void strict_descent_reaches_a_closed_state() {
    const examples::Scheduling adapter;
    const auto closure = landscape_audit::strict_descent_closure(adapter, adapter.seed(), 1);
    const auto audit = landscape_audit::audit_plateau(adapter, closure.state, 1);
    require(audit.point_local_optimum, "strict closure endpoint should have no strict one-move exit");
    require(closure.value <= adapter.full_evaluate(adapter.seed()), "strict closure worsened objective");
}

void all_adapters_have_zero_incremental_mismatches() {
    landscape_audit::AuditOptions options;
    options.max_states = 200;
    options.threads = 3;
    const examples::MaxSat maxsat;
    const examples::GraphColoring coloring;
    const examples::Scheduling scheduling;
    (void)landscape_audit::audit_plateau(maxsat, maxsat.seed(), 1, options);
    (void)landscape_audit::audit_plateau(maxsat, maxsat.seed(), 2, options);
    (void)landscape_audit::audit_plateau(coloring, coloring.seed(), 1, options);
    (void)landscape_audit::audit_plateau(coloring, coloring.seed(), 2, options);
    (void)landscape_audit::audit_plateau(scheduling, scheduling.seed(), 1, options);
    (void)landscape_audit::audit_plateau(scheduling, scheduling.seed(), 2, options);
}

template <class Adapter>
void fuzz_adapter(const Adapter& adapter, std::uint64_t seed) {
    std::mt19937_64 random(seed);
    auto state = adapter.seed();
    for (std::size_t iteration = 0; iteration < 2000; ++iteration) {
        const auto radius = 1U + static_cast<unsigned>(random() % 2U);
        const auto moves = adapter.moves(state, radius);
        const auto& move = moves[static_cast<std::size_t>(random() % moves.size())];
        const auto current = adapter.full_evaluate(state);
        const auto next = adapter.apply(state, move);
        require(current + adapter.delta(state, move) == adapter.full_evaluate(next), "fuzz mismatch for " + adapter.instance_id());
        state = next;
    }
}

void randomized_incremental_reference_fuzzing_has_zero_mismatches() {
    fuzz_adapter(examples::MaxSat{}, 0x617U);
    fuzz_adapter(examples::GraphColoring{}, 0xC010U);
    fuzz_adapter(examples::Scheduling{}, 0x5CEDU);
}

void replay_rejects_corruption() {
    const examples::NeutralTrap adapter;
    const auto result = landscape_audit::audit_plateau(adapter, adapter.seed(), 1);
    const auto root = std::filesystem::temp_directory_path() / "landscape-audit-test";
    std::filesystem::create_directories(root);
    const auto good = root / "good.json";
    const auto bad = root / "bad.json";
    landscape_audit::atomic_write(good, landscape_audit::certificate_json(result));
    landscape_audit::atomic_write(bad, landscape_audit::certificate_json(result) + "corruption");
    require(landscape_audit::verify_certificate_bytes(result, good), "valid replay was rejected");
    require(!landscape_audit::verify_certificate_bytes(result, bad), "corrupted certificate was accepted");
    std::filesystem::remove_all(root);
}

void checkpoint_resume_matches_uninterrupted_enumeration() {
    const examples::NeutralTrap adapter;
    const auto root = std::filesystem::temp_directory_path() / "landscape-audit-checkpoint-test";
    std::filesystem::create_directories(root);
    const auto checkpoint = root / "plateau.checkpoint";
    landscape_audit::AuditOptions bounded;
    bounded.max_states = 1;
    bounded.checkpoint = checkpoint;
    const auto partial = landscape_audit::audit_plateau(adapter, adapter.seed(), 1, bounded);
    require(!partial.exact, "one expanded state should truncate the two-state plateau");
    require(std::filesystem::exists(checkpoint), "bounded run did not write checkpoint");
    landscape_audit::AuditOptions resumed;
    resumed.max_states = 10;
    resumed.checkpoint = checkpoint;
    resumed.resume = true;
    const auto continued = landscape_audit::audit_plateau(adapter, adapter.seed(), 1, resumed);
    const auto direct = landscape_audit::audit_plateau(adapter, adapter.seed(), 1);
    require(landscape_audit::certificate_json(continued) == landscape_audit::certificate_json(direct), "resumed certificate differs from uninterrupted run");
    std::filesystem::remove_all(root);
}

}  // namespace

int main() {
    try {
        neutral_trap_distinguishes_point_and_plateau();
        state_budget_never_certifies_truncated_plateau();
        threads_are_deterministic();
        radius_two_is_adapter_defined_not_cartesian_runtime_composition();
        strict_descent_reaches_a_closed_state();
        all_adapters_have_zero_incremental_mismatches();
        randomized_incremental_reference_fuzzing_has_zero_mismatches();
        replay_rejects_corruption();
        checkpoint_resume_matches_uninterrupted_enumeration();
        require(landscape_audit::sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 empty vector failed");
        std::cout << "all landscape-audit tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return 1;
    }
}
