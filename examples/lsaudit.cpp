#include "adapters.hpp"
#include "landscape_audit/audit.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Cli {
    std::string example = "neutral-trap";
    std::size_t radius = 1;
    std::size_t max_states = 100000;
    std::size_t threads = 1;
    bool close_first = false;
    bool resume = false;
    std::filesystem::path certificate = "audit.json";
    std::filesystem::path graph = "audit.graphml";
    std::filesystem::path checkpoint;
    std::filesystem::path verify;
};

void usage() {
    std::cout << "Landscape Audit v0.1.0\n\n"
              << "Usage: lsaudit [neutral-trap|maxsat|coloring|scheduling] [options]\n"
              << "  --radius N           adapter-defined neighborhood radius (default 1)\n"
              << "  --max-states N       neutral states allowed before a bounded result\n"
              << "  --threads N          deterministic parallel move evaluation\n"
              << "  --close-first        run deterministic strict descent before auditing\n"
              << "  --checkpoint PATH    atomically save resumable plateau state\n"
              << "  --resume             continue from --checkpoint\n"
              << "  --certificate PATH   JSON certificate output\n"
              << "  --graph PATH         GraphML output\n"
              << "  --verify PATH        replay and byte-compare an existing certificate\n";
}

Cli parse(int argc, char** argv) {
    Cli cli;
    int i = 1;
    if (i < argc && argv[i][0] != '-') cli.example = argv[i++];
    while (i < argc) {
        const std::string option = argv[i++];
        const auto value = [&]() -> std::string {
            if (i >= argc) throw std::runtime_error("missing value after " + option);
            return argv[i++];
        };
        if (option == "--radius") cli.radius = std::stoull(value());
        else if (option == "--max-states") cli.max_states = std::stoull(value());
        else if (option == "--threads") cli.threads = std::stoull(value());
        else if (option == "--close-first") cli.close_first = true;
        else if (option == "--checkpoint") cli.checkpoint = value();
        else if (option == "--resume") cli.resume = true;
        else if (option == "--certificate") cli.certificate = value();
        else if (option == "--graph") cli.graph = value();
        else if (option == "--verify") cli.verify = value();
        else if (option == "--help" || option == "-h") { usage(); std::exit(0); }
        else throw std::runtime_error("unknown option: " + option);
    }
    return cli;
}

template <class Adapter>
int run(const Adapter& adapter, const Cli& cli) {
    landscape_audit::AuditOptions options;
    options.max_states = cli.max_states;
    options.threads = cli.threads;
    options.checkpoint = cli.checkpoint;
    options.resume = cli.resume;
    auto seed = adapter.seed();
    if (cli.close_first) {
        auto closure = landscape_audit::strict_descent_closure(adapter, seed, cli.radius, options);
        seed = closure.state;
        std::cout << "strict closure accepted " << closure.accepted_moves.size() << " move(s); objective=" << closure.value << '\n';
    }
    const auto result = landscape_audit::audit_plateau(adapter, seed, cli.radius, options);
    if (!cli.verify.empty()) {
        const bool accepted = landscape_audit::verify_certificate_bytes(result, cli.verify);
        std::cout << (accepted ? "VERIFIED" : "REJECTED") << ": " << cli.verify.string() << '\n';
        return accepted ? 0 : 2;
    }
    landscape_audit::write_outputs(result, cli.certificate, cli.graph);
    std::cout << "objective=" << result.objective_value
              << " point_local_optimum=" << std::boolalpha << result.point_local_optimum
              << " plateau_local_optimum=" << result.plateau_local_optimum
              << " neutral_states=" << result.neutral_state_count
              << " exact=" << result.exact << '\n'
              << "certificate=" << cli.certificate.string() << '\n'
              << "graph=" << cli.graph.string() << '\n';
    return result.exact ? 0 : 3;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto cli = parse(argc, argv);
        if (cli.example == "neutral-trap") return run(examples::NeutralTrap{}, cli);
        if (cli.example == "maxsat") return run(examples::MaxSat{}, cli);
        if (cli.example == "coloring") return run(examples::GraphColoring{}, cli);
        if (cli.example == "scheduling") return run(examples::Scheduling{}, cli);
        throw std::runtime_error("unknown example: " + cli.example);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
