#ifndef POAC_CORE_RESOLVER_SAT_HPP
#define POAC_CORE_RESOLVER_SAT_HPP

// std
#include <iostream>
#include <vector>
#include <utility>
#include <string>
#include <numeric>
#include <algorithm>
#include <map>
#include <cmath>

// external
#include <boost/range/adaptor/indexed.hpp>
#include <mitama/result/result.hpp>

namespace poac::core::resolver::sat {
    enum class Status {
        satisfied, // found a satisfying assignment
        unsatisfied, // found no satisfying assignment
        normal, // Successful completion OR unsolved
    };

    std::vector<int>
    to_assignments(const std::vector<int>& literals) {
        std::vector<int> assignments;
        for (auto&& l : literals | boost::adaptors::indexed()) {
            const int literal = l.index() + 1;
            if (l.value() != -1) {
                assignments.emplace_back((l.value() % 2 == 0 ? 1 : -1) * literal);
            } else { // for literals that can take either value, arbitrarily assign them to be true
                assignments.emplace_back(literal);
            }
        }
        return assignments;
    }

    // the difference in number of occurrences
    template <
        template <class T, class=std::allocator<T>> typename TwoDim,
        template <class T, class=std::allocator<T>> typename OneDim,
        typename T, typename U
    >
    T calc_literal_polarity(const TwoDim<OneDim<T>>& rng, const U& i) {
        T acc = 0;
        for (const auto& rn : rng) {
            for (const auto& r : rn) {
                if (std::abs(r) == i) {
                    r > 0 ? ++acc : --acc;
                }
            }
        }
        return acc;
    }

    int literal_to_index(int l) {
        return std::abs(l) - 1;
    }

    // 1もしくは-1の数を clauses 全体から探索
    // 既に割り当て済みの変数は，delete_applyed_literal関数でclausesから削除ずみなので，現状のclausesに内在する変数から一番数が多い変数のindexを返す
    int maximum_literal_number_index(const std::vector<std::vector<int>>& clauses) {
        std::map<int, int> frequency;
        for (const auto& clause : clauses) {
            for (const auto& literal : clause) {
                auto result = frequency.insert(
                    { literal_to_index(literal), 1 }
                );
                if (!result.second) {
                    result.first->second++;
                }
            }
        }
        auto x = std::max_element(
            frequency.begin(), frequency.end(),
            [](const auto& p1, const auto& p2){
                return p1.second > p2.second;
            });
        return x->first;
    }

    // 変数割り当てが決定された変数をcluasesから削除する
    Status
    delete_set_literal(std::vector<std::vector<int>>& clauses, const int& index, const int& set_val) {
        for (auto itr1 = clauses.begin(); itr1 != clauses.end(); ++itr1) {
            for (auto itr2 = itr1->begin(); itr2 != itr1->end(); ++itr2) {
                // set_val -> unassigned(-1) -> always false
                // set_val -> true(0) -> value == index + 1
                // set_val -> false(1) -> value == -(index + 1)
                if (set_val >= 0 && (set_val == 0 ? index + 1 : -1 * (index + 1)) == *itr2) {
                    clauses.erase(itr1);
                    --itr1; // reset iterator
                    if (clauses.empty()) {
                        return Status::satisfied;
                    }
                    break; // to the next clause
                } else if (index == literal_to_index(*itr2)) { // the literal with opposite polarity
                    itr1->erase(itr2); // remove the literal from the clause
                    --itr2; // reset iterator
                    if (itr1->empty()) {
                        // unsatisfiable currently
                        return Status::unsatisfied;
                    }
                    break; // to the next clause
                }
            }
        }
        return Status::normal;
    }

    // unit resolution
    Status
    unit_propagate(std::vector<std::vector<int>>& clauses, std::vector<int>& literals) {
        bool unit_clause_found = true;
        while (unit_clause_found) {
            unit_clause_found = false;

            for (auto itr = clauses.begin(); itr != clauses.end(); ++itr) {
                if (itr->size() == 1) { // unit clause ({3}, {5}, ...)
                    unit_clause_found = true;

                    // 0 - if true, 1 - if false, set the literal
                    literals[literal_to_index(*itr->begin())] = *itr->begin() < 0;

                    const int index = literal_to_index(*itr->begin());
                    Status result = delete_set_literal(clauses, index, literals[index]);
                    if (result == Status::satisfied || result == Status::unsatisfied) {
                        return result;
                    }
                    // 今回のdeleteで，別のunit caluseができているかもしれないので，もう一度先頭からループし直す
                    break;
                } else if (itr->empty()) {
                    // the formula is unsatisfiable in this branch
                    return Status::unsatisfied;
                }
            }
        }
        return Status::normal;
    }

    // recursive DPLL algorithm
    [[nodiscard]] mitama::result<std::vector<int>, std::string>
    dpll(std::vector<std::vector<int>>& clauses, std::vector<int>& literals) {
        if (clauses.empty()) {
            return mitama::success(to_assignments(literals));
        } else if (
            Status result = unit_propagate(clauses, literals);
            result == Status::satisfied
        ) {
            return mitama::success(to_assignments(literals));
        } else if (result == Status::unsatisfied) {
            return mitama::failure(
                "could not solve dependencies.\n"
                "detail: given SAT problem was unsatisfied."
            );
        }

        // 最大の頻度を持つ変数が、次に割り当てられる値になります。
        const int i = maximum_literal_number_index(clauses);
        // need to apply twice, once true, the other false
        for (int j = 0; j < 2; ++j) {
            // copy the formula before recursing
            std::vector<int> new_literals = literals;

            // if the number of literals with positive polarity are greater
            if (calc_literal_polarity(clauses, i + 1) > 0) {
                new_literals[i] = j; // positive
            } else {
                new_literals[i] = (j + 1) % 2; // negative
            }

            // apply the change to all the clauses
            if (
                Status result = delete_set_literal(clauses, i, new_literals[i]);
                result == Status::satisfied
            ) {
                return mitama::success(to_assignments(new_literals));
            } else if (result == Status::unsatisfied) {
                // in this branch, return normally
                continue;
            }

            const auto result = dpll(clauses, new_literals);
            if (result.is_ok()) {
                return result;
            }
        }
        return mitama::failure(
            "could not solve dependencies.\n"
            "detail: given SAT problem was unsatisfied."
        );
    }

    [[nodiscard]] mitama::result<std::vector<int>, std::string>
    solve(std::vector<std::vector<int>> clauses, const unsigned long& variables) {
        // indexに対応するリテラル値の割り当て状態を表現する
        // a vector that stores the value assigned to each variable, where
        // -1 - unassigned, 0 - true, 1 - false
        std::vector<int> literals(variables, -1);
        return dpll(clauses, literals);
    }
} // end namespace
#endif // !POAC_CORE_RESOLVER_SAT_HPP
