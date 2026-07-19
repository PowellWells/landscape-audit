#pragma once

#include "landscape_audit/sha256.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace landscape_audit {

struct AuditOptions {
    std::size_t max_states = 100000;
    std::size_t threads = 1;
    bool verify_incremental = true;
    std::filesystem::path checkpoint;
    bool resume = false;
};

struct EdgeRecord {
    std::string from;
    std::string to;
    std::string move;
    std::int64_t from_value{};
    std::int64_t to_value{};
};

struct NodeRecord {
    std::string key;
    std::string serialized;
    std::int64_t value{};
};

struct AuditResult {
    std::string schema = "landscape-audit-certificate-v1";
    std::string instance_id;
    std::string neighborhood_id;
    std::size_t neighborhood_radius = 1;
    std::string generator_version;
    std::string seed_hash;
    std::int64_t objective_value{};
    std::string objective_order = "strict integer minimization";
    std::uint64_t move_count{};
    std::uint64_t expanded_state_count{};
    std::uint64_t neutral_state_count{};
    std::uint64_t outgoing_edge_count{};
    std::optional<std::int64_t> minimum_delta;
    std::map<std::int64_t, std::uint64_t> delta_histogram;
    std::string enumeration_digest;
    bool reference_verifier_result = true;
    bool point_local_optimum = true;
    bool plateau_local_optimum = true;
    bool exact = true;
    std::string termination_reason = "exhausted";
    std::vector<NodeRecord> nodes;
    std::vector<EdgeRecord> edges;
};

inline std::string json_escape(std::string_view value) {
    std::ostringstream out;
    for (const unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20U) {
                    constexpr char digits[] = "0123456789abcdef";
                    out << "\\u00" << digits[c >> 4U] << digits[c & 0x0fU];
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    return out.str();
}

inline std::string xml_escape(std::string_view value) {
    std::string out;
    for (const char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c;
        }
    }
    return out;
}

inline std::string to_hex(std::string_view input) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(input.size() * 2U);
    for (const unsigned char c : input) {
        out.push_back(digits[c >> 4U]);
        out.push_back(digits[c & 0x0fU]);
    }
    return out;
}

inline std::string from_hex(std::string_view input) {
    if (input.size() % 2U != 0U) throw std::runtime_error("invalid checkpoint hex");
    const auto nibble = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<unsigned>(c - 'A' + 10);
        throw std::runtime_error("invalid checkpoint hex digit");
    };
    std::string out(input.size() / 2U, '\0');
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = static_cast<char>((nibble(input[2U * i]) << 4U) | nibble(input[2U * i + 1U]));
    return out;
}

inline void atomic_write(const std::filesystem::path& path, std::string_view data) {
    if (path.empty()) return;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot write " + temporary.string());
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!output) throw std::runtime_error("failed writing " + temporary.string());
    }
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::rename(temporary, path);
}

inline std::string certificate_json(const AuditResult& result) {
    std::ostringstream out;
    out << "{\n"
        << "  \"schema\": \"" << json_escape(result.schema) << "\",\n"
        << "  \"instance_id\": \"" << json_escape(result.instance_id) << "\",\n"
        << "  \"state_hash\": \"" << result.seed_hash << "\",\n"
        << "  \"serialized_state_hash\": \"" << result.seed_hash << "\",\n"
        << "  \"objective_value\": " << result.objective_value << ",\n"
        << "  \"objective_order\": \"" << json_escape(result.objective_order) << "\",\n"
        << "  \"neighborhood_id\": \"" << json_escape(result.neighborhood_id) << "\",\n"
        << "  \"neighborhood_radius\": " << result.neighborhood_radius << ",\n"
        << "  \"generator_version\": \"" << json_escape(result.generator_version) << "\",\n"
        << "  \"generator_build_hash\": \"header-only-v0.1.0\",\n"
        << "  \"move_count\": " << result.move_count << ",\n"
        << "  \"expanded_state_count\": " << result.expanded_state_count << ",\n"
        << "  \"neutral_state_count\": " << result.neutral_state_count << ",\n"
        << "  \"outgoing_edge_count\": " << result.outgoing_edge_count << ",\n"
        << "  \"minimum_delta\": ";
    if (result.minimum_delta) out << *result.minimum_delta; else out << "null";
    out << ",\n  \"delta_histogram\": {";
    bool first = true;
    for (const auto& [delta, count] : result.delta_histogram) {
        if (!first) out << ',';
        out << "\n    \"" << delta << "\": " << count;
        first = false;
    }
    if (!first) out << '\n';
    out << "  },\n"
        << "  \"enumeration_digest\": \"" << result.enumeration_digest << "\",\n"
        << "  \"reference_verifier_result\": " << (result.reference_verifier_result ? "true" : "false") << ",\n"
        << "  \"point_local_optimum\": " << (result.point_local_optimum ? "true" : "false") << ",\n"
        << "  \"plateau_local_optimum\": " << (result.plateau_local_optimum ? "true" : "false") << ",\n"
        << "  \"termination_reason\": \"" << result.termination_reason << "\",\n"
        << "  \"exact\": " << (result.exact ? "true" : "false") << "\n}\n";
    return out.str();
}

inline std::string graphml(const AuditResult& result) {
    std::ostringstream out;
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n"
        << "  <key id=\"value\" for=\"node\" attr.name=\"objective\" attr.type=\"long\"/>\n"
        << "  <key id=\"move\" for=\"edge\" attr.name=\"move\" attr.type=\"string\"/>\n"
        << "  <key id=\"delta\" for=\"edge\" attr.name=\"delta\" attr.type=\"long\"/>\n"
        << "  <graph edgedefault=\"directed\">\n";
    std::map<std::string, std::int64_t> nodes;
    for (const auto& node : result.nodes) nodes[node.key] = node.value;
    for (const auto& edge : result.edges) {
        nodes.try_emplace(edge.from, edge.from_value);
        nodes.try_emplace(edge.to, edge.to_value);
    }
    for (const auto& [key, value] : nodes) out << "    <node id=\"" << key << "\"><data key=\"value\">" << value << "</data></node>\n";
    for (std::size_t i = 0; i < result.edges.size(); ++i) {
        const auto& edge = result.edges[i];
        out << "    <edge id=\"e" << i << "\" source=\"" << edge.from << "\" target=\"" << edge.to << "\">"
            << "<data key=\"move\">" << xml_escape(edge.move) << "</data>"
            << "<data key=\"delta\">" << (edge.to_value - edge.from_value) << "</data></edge>\n";
    }
    out << "  </graph>\n</graphml>\n";
    return out.str();
}

template <class Adapter>
struct EvaluatedMove {
    using State = typename Adapter::State;
    typename Adapter::Move move;
    State next;
    std::string move_key;
    std::string next_serialized;
    std::string next_key;
    std::int64_t value{};
};

template <class Adapter>
std::vector<EvaluatedMove<Adapter>> evaluate_moves(const Adapter& adapter,
                                                    const typename Adapter::State& state,
                                                    std::int64_t current,
                                                    std::size_t radius,
                                                    const AuditOptions& options) {
    auto moves = adapter.moves(state, radius);
    std::sort(moves.begin(), moves.end(), [&](const auto& a, const auto& b) { return adapter.move_key(a) < adapter.move_key(b); });
    std::vector<EvaluatedMove<Adapter>> results(moves.size());
    std::atomic<std::size_t> next_index{0};
    std::exception_ptr failure;
    std::mutex failure_mutex;
    const auto worker = [&] {
        try {
            while (true) {
                const auto i = next_index.fetch_add(1);
                if (i >= moves.size()) return;
                auto next = adapter.apply(state, moves[i]);
                const auto full = adapter.full_evaluate(next);
                if (options.verify_incremental) {
                    const auto predicted = current + adapter.delta(state, moves[i]);
                    if (predicted != full) throw std::runtime_error("incremental/reference mismatch for move " + adapter.move_key(moves[i]));
                }
                auto serialized = adapter.serialize(next);
                results[i] = {moves[i], std::move(next), adapter.move_key(moves[i]), serialized, sha256(serialized), full};
            }
        } catch (...) {
            std::lock_guard lock(failure_mutex);
            if (!failure) failure = std::current_exception();
        }
    };
    const auto count = std::max<std::size_t>(1, std::min<std::size_t>(options.threads, moves.empty() ? 1 : moves.size()));
    std::vector<std::thread> threads;
    threads.reserve(count);
    for (std::size_t i = 0; i < count; ++i) threads.emplace_back(worker);
    for (auto& thread : threads) thread.join();
    if (failure) std::rethrow_exception(failure);
    return results;
}

template <class Adapter>
struct ClosureResult {
    typename Adapter::State state;
    std::int64_t value{};
    std::vector<std::string> accepted_moves;
    std::uint64_t evaluated_moves{};
};

template <class Adapter>
ClosureResult<Adapter> strict_descent_closure(const Adapter& adapter,
                                              typename Adapter::State state,
                                              std::size_t radius,
                                              const AuditOptions& options = {}) {
    auto value = adapter.full_evaluate(state);
    ClosureResult<Adapter> result{state, value, {}, 0};
    while (true) {
        auto moves = evaluate_moves(adapter, result.state, result.value, radius, options);
        result.evaluated_moves += moves.size();
        auto best = moves.end();
        for (auto it = moves.begin(); it != moves.end(); ++it) {
            if (it->value >= result.value) continue;
            if (best == moves.end() || it->value < best->value || (it->value == best->value && it->move_key < best->move_key)) best = it;
        }
        if (best == moves.end()) return result;
        result.state = std::move(best->next);
        result.value = best->value;
        result.accepted_moves.push_back(best->move_key);
    }
}

template <class Adapter>
AuditResult audit_plateau(const Adapter& adapter,
                          const typename Adapter::State& seed,
                          std::size_t radius,
                          const AuditOptions& options = {}) {
    using State = typename Adapter::State;
    if (options.max_states == 0) throw std::invalid_argument("max_states must be positive");
    AuditResult result;
    result.instance_id = adapter.instance_id();
    result.neighborhood_id = adapter.neighborhood_id(radius);
    result.neighborhood_radius = radius;
    result.generator_version = adapter.generator_version();
    const auto seed_serialized = adapter.serialize(seed);
    result.seed_hash = sha256(seed_serialized);
    result.objective_value = adapter.full_evaluate(seed);

    std::vector<State> states;
    std::queue<std::size_t> frontier;
    std::unordered_set<std::string> seen;

    const auto save_checkpoint = [&] {
        if (options.checkpoint.empty()) return;
        std::ostringstream out;
        out << "LANDSCAPE_AUDIT_CHECKPOINT_V1\n"
            << to_hex(result.instance_id) << '\n'
            << to_hex(result.neighborhood_id) << '\n'
            << result.neighborhood_radius << '\n'
            << result.seed_hash << '\n'
            << result.objective_value << '\n'
            << result.move_count << ' ' << result.expanded_state_count << ' ' << result.outgoing_edge_count << '\n'
            << result.point_local_optimum << ' ' << result.plateau_local_optimum << '\n'
            << (result.minimum_delta ? 1 : 0) << ' ' << result.minimum_delta.value_or(0) << '\n'
            << result.delta_histogram.size() << '\n';
        for (const auto& [delta, count] : result.delta_histogram) out << delta << ' ' << count << '\n';
        out << states.size() << '\n';
        for (const auto& state : states) {
            const auto serialized = adapter.serialize(state);
            out << to_hex(serialized) << '\n';
        }
        auto frontier_copy = frontier;
        out << frontier_copy.size() << '\n';
        while (!frontier_copy.empty()) {
            out << frontier_copy.front() << '\n';
            frontier_copy.pop();
        }
        out << result.edges.size() << '\n';
        for (const auto& edge : result.edges) {
            out << to_hex(edge.from) << ' ' << to_hex(edge.to) << ' ' << to_hex(edge.move) << ' '
                << edge.from_value << ' ' << edge.to_value << '\n';
        }
        atomic_write(options.checkpoint, out.str());
    };

    if (options.resume) {
        if (options.checkpoint.empty()) throw std::invalid_argument("resume requires a checkpoint path");
        std::ifstream input(options.checkpoint, std::ios::binary);
        if (!input) throw std::runtime_error("cannot read checkpoint " + options.checkpoint.string());
        std::string line;
        std::getline(input, line);
        if (line != "LANDSCAPE_AUDIT_CHECKPOINT_V1") throw std::runtime_error("unsupported checkpoint schema");
        std::string encoded_instance, encoded_neighborhood, saved_seed;
        std::getline(input, encoded_instance);
        std::getline(input, encoded_neighborhood);
        std::size_t saved_radius{};
        input >> saved_radius >> saved_seed;
        if (from_hex(encoded_instance) != result.instance_id || from_hex(encoded_neighborhood) != result.neighborhood_id ||
            saved_radius != radius || saved_seed != result.seed_hash) {
            throw std::runtime_error("checkpoint does not match adapter, neighborhood, or seed");
        }
        std::int64_t saved_objective{};
        input >> saved_objective;
        if (saved_objective != result.objective_value) throw std::runtime_error("checkpoint objective does not match reference evaluator");
        input >> result.move_count >> result.expanded_state_count >> result.outgoing_edge_count;
        input >> result.point_local_optimum >> result.plateau_local_optimum;
        bool has_minimum{};
        std::int64_t minimum{};
        input >> has_minimum >> minimum;
        if (has_minimum) result.minimum_delta = minimum;
        std::size_t histogram_size{};
        input >> histogram_size;
        for (std::size_t i = 0; i < histogram_size; ++i) {
            std::int64_t delta{};
            std::uint64_t count{};
            input >> delta >> count;
            result.delta_histogram[delta] = count;
        }
        std::size_t state_count{};
        input >> state_count;
        states.reserve(state_count);
        for (std::size_t i = 0; i < state_count; ++i) {
            std::string encoded;
            input >> encoded;
            auto serialized = from_hex(encoded);
            auto state = adapter.deserialize(serialized);
            if (adapter.full_evaluate(state) != result.objective_value) throw std::runtime_error("checkpoint contains a non-neutral state");
            const auto key = sha256(serialized);
            if (!seen.insert(key).second) throw std::runtime_error("checkpoint contains a duplicate state");
            states.push_back(std::move(state));
            result.nodes.push_back({key, serialized, result.objective_value});
        }
        std::size_t frontier_size{};
        input >> frontier_size;
        for (std::size_t i = 0; i < frontier_size; ++i) {
            std::size_t index{};
            input >> index;
            if (index >= states.size()) throw std::runtime_error("checkpoint frontier index is invalid");
            frontier.push(index);
        }
        std::size_t edge_count{};
        input >> edge_count;
        result.edges.reserve(edge_count);
        for (std::size_t i = 0; i < edge_count; ++i) {
            std::string from, to, move;
            EdgeRecord edge;
            input >> from >> to >> move >> edge.from_value >> edge.to_value;
            edge.from = from_hex(from);
            edge.to = from_hex(to);
            edge.move = from_hex(move);
            result.edges.push_back(std::move(edge));
        }
        if (!input) throw std::runtime_error("checkpoint is truncated or malformed");
    } else {
        states.push_back(seed);
        frontier.push(0);
        seen.insert(result.seed_hash);
        result.nodes.push_back({result.seed_hash, seed_serialized, result.objective_value});
    }

    while (!frontier.empty()) {
        if (result.expanded_state_count >= options.max_states) {
            result.exact = false;
            result.termination_reason = "state_limit";
            break;
        }
        const auto index = frontier.front();
        frontier.pop();
        const auto current_serialized = adapter.serialize(states[index]);
        const auto current_key = sha256(current_serialized);
        auto evaluated = evaluate_moves(adapter, states[index], result.objective_value, radius, options);
        ++result.expanded_state_count;
        for (auto& item : evaluated) {
            ++result.move_count;
            const auto delta = item.value - result.objective_value;
            ++result.delta_histogram[delta];
            if (!result.minimum_delta || delta < *result.minimum_delta) result.minimum_delta = delta;
            if (delta != 0) ++result.outgoing_edge_count;
            if (index == 0 && delta < 0) result.point_local_optimum = false;
            if (delta < 0) result.plateau_local_optimum = false;
            result.edges.push_back({current_key, item.next_key, item.move_key, result.objective_value, item.value});
            if (delta == 0 && seen.insert(item.next_key).second) {
                states.push_back(std::move(item.next));
                result.nodes.push_back({item.next_key, item.next_serialized, item.value});
                frontier.push(states.size() - 1U);
            }
        }
        save_checkpoint();
        if (result.expanded_state_count >= options.max_states && !frontier.empty()) {
            result.exact = false;
            result.termination_reason = "state_limit";
            break;
        }
    }
    result.neutral_state_count = result.nodes.size();
    if (!result.exact) result.plateau_local_optimum = false;
    std::vector<std::string> records;
    records.reserve(result.edges.size());
    for (const auto& edge : result.edges) {
        records.push_back(edge.from + "\t" + edge.move + "\t" + edge.to + "\t" + std::to_string(edge.from_value) + "\t" + std::to_string(edge.to_value) + "\n");
    }
    std::sort(records.begin(), records.end());
    std::string canonical;
    for (const auto& record : records) canonical += record;
    result.enumeration_digest = sha256(canonical);
    save_checkpoint();
    return result;
}

inline void write_outputs(const AuditResult& result,
                          const std::filesystem::path& certificate,
                          const std::filesystem::path& graph) {
    atomic_write(certificate, certificate_json(result));
    atomic_write(graph, graphml(result));
}

inline std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read " + path.string());
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

inline bool verify_certificate_bytes(const AuditResult& replayed, const std::filesystem::path& certificate) {
    return read_file(certificate) == certificate_json(replayed);
}

}  // namespace landscape_audit
