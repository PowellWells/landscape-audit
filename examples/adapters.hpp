#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace examples {

struct NeutralTrap {
    using State = std::uint8_t;
    struct Move { std::uint8_t target{}; };

    std::string instance_id() const { return "neutral-trap-v1"; }
    std::string neighborhood_id(std::size_t radius) const { return "neutral-trap/r" + std::to_string(radius); }
    std::string generator_version() const { return "neutral-trap-1"; }
    std::string serialize(State state) const { return std::string(1, static_cast<char>(state)); }
    State deserialize(std::string_view bytes) const {
        if (bytes.size() != 1 || static_cast<unsigned char>(bytes[0]) > 2) throw std::runtime_error("invalid neutral-trap state");
        return static_cast<State>(bytes[0]);
    }
    std::int64_t full_evaluate(State state) const { return state == 2 ? 0 : 1; }
    std::vector<Move> moves(State state, std::size_t radius) const {
        if (radius != 1) throw std::runtime_error("neutral-trap supports radius 1");
        if (state == 0) return {{1}};
        if (state == 1) return {{0}, {2}};
        return {{1}};
    }
    std::string move_key(const Move& move) const { return "to-" + std::to_string(move.target); }
    State apply(State, const Move& move) const { return move.target; }
    std::int64_t delta(State state, const Move& move) const { return full_evaluate(move.target) - full_evaluate(state); }
    State seed() const { return 0; }
};

struct MaxSat {
    using State = std::uint8_t;
    struct Move { std::uint8_t first{}, second{255}; };
    using Literal = int;
    using Clause = std::vector<Literal>;
    const std::vector<Clause> clauses{{1, 2}, {-1, 3}, {-2, -3}, {4}, {-4, 1}};

    std::string instance_id() const { return "maxsat-five-clauses-v1"; }
    std::string neighborhood_id(std::size_t radius) const { return "boolean-flip/r" + std::to_string(radius); }
    std::string generator_version() const { return "maxsat-adapter-1"; }
    std::string serialize(State state) const { return std::string(1, static_cast<char>(state)); }
    State deserialize(std::string_view bytes) const {
        if (bytes.size() != 1 || static_cast<unsigned char>(bytes[0]) > 15) throw std::runtime_error("invalid Max-SAT state");
        return static_cast<State>(bytes[0]);
    }
    static bool literal_value(State state, Literal literal) {
        const auto variable = static_cast<unsigned>(literal > 0 ? literal - 1 : -literal - 1);
        const bool value = (state & (1U << variable)) != 0;
        return literal > 0 ? value : !value;
    }
    bool satisfied(State state, const Clause& clause) const {
        return std::any_of(clause.begin(), clause.end(), [&](Literal literal) { return literal_value(state, literal); });
    }
    std::int64_t full_evaluate(State state) const {
        std::int64_t unsatisfied = 0;
        for (const auto& clause : clauses) unsatisfied += satisfied(state, clause) ? 0 : 1;
        return unsatisfied;
    }
    std::vector<Move> moves(State, std::size_t radius) const {
        std::vector<Move> result;
        if (radius == 1) {
            for (std::uint8_t i = 0; i < 4; ++i) result.push_back({i, 255});
        } else if (radius == 2) {
            for (std::uint8_t i = 0; i < 4; ++i) for (std::uint8_t j = i + 1; j < 4; ++j) result.push_back({i, j});
        } else {
            throw std::runtime_error("Max-SAT supports radius 1 or 2");
        }
        return result;
    }
    std::string move_key(const Move& move) const {
        return move.second == 255 ? "flip-" + std::to_string(move.first) : "flip-" + std::to_string(move.first) + "-" + std::to_string(move.second);
    }
    State apply(State state, const Move& move) const {
        state ^= static_cast<State>(1U << move.first);
        if (move.second != 255) state ^= static_cast<State>(1U << move.second);
        return state;
    }
    std::int64_t delta(State state, const Move& move) const {
        const auto next = apply(state, move);
        std::int64_t change = 0;
        for (const auto& clause : clauses) {
            bool affected = false;
            for (const auto literal : clause) {
                const auto variable = static_cast<std::uint8_t>(literal > 0 ? literal - 1 : -literal - 1);
                affected = affected || variable == move.first || variable == move.second;
            }
            if (affected) change += static_cast<std::int64_t>(!satisfied(next, clause)) - static_cast<std::int64_t>(!satisfied(state, clause));
        }
        return change;
    }
    State seed() const { return 0; }
};

struct GraphColoring {
    using State = std::array<std::uint8_t, 5>;
    struct Move { std::uint8_t first{}, first_colour{}, second{255}, second_colour{}; };
    const std::array<std::pair<std::uint8_t, std::uint8_t>, 6> edges{{{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 0}, {0, 2}}};

    std::string instance_id() const { return "five-vertex-three-colouring-v1"; }
    std::string neighborhood_id(std::size_t radius) const { return "vertex-recolour/r" + std::to_string(radius); }
    std::string generator_version() const { return "graph-colouring-adapter-1"; }
    std::string serialize(const State& state) const { return std::string(state.begin(), state.end()); }
    State deserialize(std::string_view bytes) const {
        if (bytes.size() != 5) throw std::runtime_error("invalid graph-colouring state");
        State state{};
        for (std::size_t i = 0; i < state.size(); ++i) {
            state[i] = static_cast<std::uint8_t>(bytes[i]);
            if (state[i] > 2) throw std::runtime_error("invalid colour");
        }
        return state;
    }
    std::int64_t full_evaluate(const State& state) const {
        std::int64_t conflicts = 0;
        for (const auto [u, v] : edges) conflicts += state[u] == state[v] ? 1 : 0;
        return conflicts;
    }
    std::vector<Move> moves(const State& state, std::size_t radius) const {
        std::vector<Move> singles;
        for (std::uint8_t vertex = 0; vertex < state.size(); ++vertex) {
            for (std::uint8_t colour = 0; colour < 3; ++colour) if (colour != state[vertex]) singles.push_back({vertex, colour, 255, 0});
        }
        if (radius == 1) return singles;
        if (radius != 2) throw std::runtime_error("graph colouring supports radius 1 or 2");
        std::vector<Move> pairs;
        for (std::size_t i = 0; i < singles.size(); ++i) for (std::size_t j = i + 1; j < singles.size(); ++j) {
            if (singles[i].first != singles[j].first) pairs.push_back({singles[i].first, singles[i].first_colour, singles[j].first, singles[j].first_colour});
        }
        return pairs;
    }
    std::string move_key(const Move& move) const {
        auto text = "recolour-" + std::to_string(move.first) + "-" + std::to_string(move.first_colour);
        if (move.second != 255) text += "-and-" + std::to_string(move.second) + "-" + std::to_string(move.second_colour);
        return text;
    }
    State apply(State state, const Move& move) const {
        state[move.first] = move.first_colour;
        if (move.second != 255) state[move.second] = move.second_colour;
        return state;
    }
    std::int64_t delta(const State& state, const Move& move) const {
        const auto next = apply(state, move);
        std::int64_t change = 0;
        for (const auto [u, v] : edges) {
            if (u == move.first || v == move.first || u == move.second || v == move.second) {
                change += static_cast<std::int64_t>(next[u] == next[v]) - static_cast<std::int64_t>(state[u] == state[v]);
            }
        }
        return change;
    }
    State seed() const { return {0, 0, 0, 1, 1}; }
};

struct Scheduling {
    using State = std::array<std::uint8_t, 5>;
    struct Move { std::uint8_t first{}, second{}; };
    const std::array<std::int64_t, 5> processing{3, 1, 4, 2, 2};
    const std::array<std::int64_t, 5> due{4, 3, 12, 8, 7};
    const std::array<std::int64_t, 5> weight{2, 4, 1, 3, 2};

    std::string instance_id() const { return "weighted-tardiness-five-jobs-v1"; }
    std::string neighborhood_id(std::size_t radius) const { return radius == 1 ? "adjacent-swap/r1" : "all-swap/r2"; }
    std::string generator_version() const { return "scheduling-adapter-1"; }
    std::string serialize(const State& state) const { return std::string(state.begin(), state.end()); }
    State deserialize(std::string_view bytes) const {
        if (bytes.size() != 5) throw std::runtime_error("invalid schedule state");
        State state{};
        std::array<bool, 5> seen{};
        for (std::size_t i = 0; i < state.size(); ++i) {
            state[i] = static_cast<std::uint8_t>(bytes[i]);
            if (state[i] >= 5 || seen[state[i]]) throw std::runtime_error("schedule is not a permutation");
            seen[state[i]] = true;
        }
        return state;
    }
    std::int64_t full_evaluate(const State& state) const {
        std::int64_t time = 0, cost = 0;
        for (const auto job : state) {
            time += processing[job];
            cost += weight[job] * std::max<std::int64_t>(0, time - due[job]);
        }
        return cost;
    }
    std::vector<Move> moves(const State&, std::size_t radius) const {
        std::vector<Move> result;
        if (radius == 1) {
            for (std::uint8_t i = 0; i < 4; ++i) result.push_back({i, static_cast<std::uint8_t>(i + 1)});
        } else if (radius == 2) {
            for (std::uint8_t i = 0; i < 5; ++i) for (std::uint8_t j = i + 1; j < 5; ++j) result.push_back({i, j});
        } else {
            throw std::runtime_error("scheduling supports radius 1 or 2");
        }
        return result;
    }
    std::string move_key(const Move& move) const { return "swap-" + std::to_string(move.first) + "-" + std::to_string(move.second); }
    State apply(State state, const Move& move) const { std::swap(state[move.first], state[move.second]); return state; }
    std::int64_t delta(const State& state, const Move& move) const {
        const auto first = std::min(move.first, move.second);
        const auto last = std::max(move.first, move.second);
        auto next = apply(state, move);
        std::int64_t prefix_time = 0;
        for (std::size_t position = 0; position < first; ++position) prefix_time += processing[state[position]];
        const auto segment_cost = [&](const State& order) {
            auto time = prefix_time;
            std::int64_t cost = 0;
            for (std::size_t position = first; position <= last; ++position) {
                const auto job = order[position];
                time += processing[job];
                cost += weight[job] * std::max<std::int64_t>(0, time - due[job]);
            }
            return cost;
        };
        return segment_cost(next) - segment_cost(state);
    }
    State seed() const { return {0, 1, 2, 3, 4}; }
};

}  // namespace examples
