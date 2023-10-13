//---------------------------------------------------------------------------//
// Copyright (c) 2023 Dmitrii Tabalin <d.tabalin@nil.foundation>
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//---------------------------------------------------------------------------//

#ifndef CRYPTO3_BLUEPRINT_UTILS_PLONK_CONNECTEDNESS_CHECK_HPP
#define CRYPTO3_BLUEPRINT_UTILS_PLONK_CONNECTEDNESS_CHECK_HPP

#include <nil/crypto3/zk/snark/arithmetization/plonk/constraint_system.hpp>
#include <nil/crypto3/zk/snark/arithmetization/plonk/constraint.hpp>
#include <nil/crypto3/zk/snark/arithmetization/plonk/gate.hpp>
#include <nil/crypto3/zk/snark/arithmetization/plonk/variable.hpp>

#include <nil/blueprint/blueprint/plonk/assignment.hpp>
#include <nil/blueprint/blueprint/plonk/circuit.hpp>
#include <nil/crypto3/zk/snark/arithmetization/plonk/copy_constraint.hpp>
#include <nil/crypto3/zk/math/expression.hpp>

#include <boost/pending/disjoint_sets.hpp>
#include <boost/assert.hpp>

#include <vector>
#include <set>
#include <array>
#include <iostream>
#include <algorithm>

namespace nil {
    namespace blueprint {
        namespace detail {
            template<typename ArithmetizationParams>
            constexpr std::size_t get_row_size() {
                return ArithmetizationParams::witness_columns +
                        ArithmetizationParams::constant_columns;
            }

            template<typename ArithmetizationParams>
            std::size_t get_outputless_var_amount(std::size_t start_row_index, std::size_t rows_amount) {
                return rows_amount * get_row_size<ArithmetizationParams>();
            }

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            std::size_t copy_var_address(
                std::size_t start_row_index, std::size_t rows_amount,
                const nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type> &variable) {

                using var = nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>;
                const std::size_t row_size = get_row_size<ArithmetizationParams>();
                const std::size_t ouptutless_var_amount =
                    get_outputless_var_amount<ArithmetizationParams>(start_row_index, rows_amount);

                if (variable.type == var::column_type::public_input) {
                    // Assumes a single file public input
                    return ouptutless_var_amount + variable.rotation;
                } else if (variable.type == var::column_type::witness) {
                    return (variable.rotation - start_row_index) * row_size + variable.index;
                } else {
                    // Constant
                    return (variable.rotation - start_row_index) * row_size + row_size - 1;
                }
            };

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            void export_connectedness_zones(
                boost::disjoint_sets_with_storage<> zones,
                const nil::blueprint::assignment<
                        nil::crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    &assignment,
                const std::vector<nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>>
                    &input_variables,
                const std::size_t start_row_index, std::size_t rows_amount,
                std::ostream &os) {

                using var = nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>;
                const std::size_t row_size = get_row_size<ArithmetizationParams>();
                const std::size_t ouptutless_var_amount =
                    get_outputless_var_amount<ArithmetizationParams>(start_row_index, rows_amount);
                const std::size_t end_row = start_row_index + rows_amount;

                nil::blueprint::assignment<
                    nil::crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    output_assignment;

                // We do '+1' in all the assignments to separate the unassigned cells (0 by default)
                // from the ones which actually got checked.
                for (std::size_t witness_column = 0; witness_column < row_size; witness_column++) {
                    std::size_t last_row =
                        std::min<std::size_t>(end_row, assignment.witness_column_size(witness_column));
                    for (std::size_t row = start_row_index; row < last_row; row++) {
                        output_assignment.witness(witness_column, row) =
                            zones.find_set(copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                                start_row_index, rows_amount,
                                var(witness_column, row, false, var::column_type::witness))) + 1;
                    }
                }
                for (std::size_t constant_column = 0; constant_column < ArithmetizationParams::constant_columns;
                     constant_column++) {

                    std::size_t last_row =
                        std::min<std::size_t>(end_row, assignment.constant_column_size(constant_column));
                    for (std::size_t row = start_row_index; row < last_row; row++) {
                        output_assignment.constant(constant_column, row) =
                            zones.find_set(copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                                start_row_index, rows_amount,
                                var(constant_column, row, false, var::column_type::constant))) + 1;
                    }
                }
                for (auto &variable : input_variables) {
                    switch (variable.type) {
                        case var::column_type::constant:
                            output_assignment.constant(variable.index, variable.rotation) =
                                zones.find_set(copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                                    start_row_index, rows_amount, variable)) + 1;
                            break;
                        case var::column_type::public_input:
                            output_assignment.public_input(variable.index, variable.rotation) =
                                zones.find_set(copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                                    start_row_index, rows_amount, variable)) + 1;
                            break;
                        case var::column_type::witness:
                            output_assignment.witness(variable.index, variable.rotation) =
                                zones.find_set(copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                                    start_row_index, rows_amount, variable)) + 1;
                            break;
                        case var::column_type::selector:
                            BOOST_ASSERT_MSG(false, "Selector variables should not be input variables.");
                            break;
                    }
                }
                // Copy selectors over from assignment
                for (std::size_t selector = 0; selector < assignment.selectors_amount(); selector++) {
                    std::size_t last_row =
                        std::min<std::size_t>(end_row, assignment.selector_column_size(selector));
                    for (std::size_t row = start_row_index; row < last_row; row++) {
                        output_assignment.selector(selector, row) =
                            assignment.selector(selector, row);
                    }
                }
                output_assignment.export_table(os);
            }

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            void mark_set(
                const nil::blueprint::assignment<
                    nil::crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    &assignment,
                boost::disjoint_sets_with_storage<> &zones,
                const std::set<nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>>
                    &variable_set,
                const std::function<std::size_t(std::size_t, std::size_t,
                    nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>)>
                    &gate_var_address,
                std::size_t selector_index,
                std::size_t start_row_index,
                std::size_t end_row_index) {

                std::size_t last_row =
                    std::min<std::size_t>(end_row_index, assignment.selector_column_size(selector_index));
                for (std::size_t row = start_row_index; row < last_row; row++) {
                    if (assignment.selector(selector_index, row) != 0) {
                        for (const auto &variable : variable_set) {
                            zones.union_set(gate_var_address(start_row_index, row, variable),
                                        gate_var_address(start_row_index, row, *variable_set.begin()));
                        }
                    }
                }
            }

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            boost::disjoint_sets_with_storage<> generate_connectedness_zones(
                const nil::blueprint::assignment<
                    nil::crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    &assignment,
                const nil::blueprint::circuit<
                    nil::crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    &bp,
                const std::vector<nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>>
                    &input_variables,
                const std::size_t start_row_index, std::size_t rows_amount) {

                using var = nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>;

                constexpr std::size_t row_size = get_row_size<ArithmetizationParams>();
                std::size_t ouptutless_var_amount =
                    get_outputless_var_amount<ArithmetizationParams>(start_row_index, rows_amount);
                const std::size_t var_amount = ouptutless_var_amount + input_variables.size();
                boost::disjoint_sets_with_storage<> zones(var_amount);
                auto gate_var_address = [](std::size_t start_row_index, std::size_t row, const var &variable) {
                    if (variable.type == var::column_type::witness) {
                        return (row - start_row_index + variable.rotation) * row_size + variable.index;
                    } else {
                        // Constant
                        return (row - start_row_index + variable.rotation) * row_size + row_size - 1;
                    }
                };
                constexpr std::size_t gate_size = 3 * row_size;
                const std::size_t end_row_index = start_row_index + rows_amount;
                for (const auto &gate : bp.gates()) {
                    std::set<var> variable_set;
                    std::function<void(var)> variable_extractor = [&variable_set](var variable) {
                        variable_set.insert(variable);
                    };
                    nil::crypto3::math::expression_for_each_variable_visitor<var> visitor(variable_extractor);
                    for (const auto &constraint : gate.constraints) {
                        visitor.visit(constraint);
                    }
                    mark_set(assignment, zones, variable_set, gate_var_address, gate.selector_index,
                             start_row_index, end_row_index);
                }
                for (auto &lookup_gate : bp.lookup_gates()) {
                    std::set<var> variable_set;
                    std::function<void(var)> variable_extractor = [&variable_set](var variable) {
                        variable_set.insert(variable);
                    };
                    nil::crypto3::math::expression_for_each_variable_visitor<var> visitor(variable_extractor);
                    for (const auto &lookup_constraint : lookup_gate.constraints) {
                        for (const auto &lookup_input : lookup_constraint.lookup_input) {
                            visitor.visit(lookup_input);
                        }
                    }
                    mark_set(assignment, zones, variable_set, gate_var_address, lookup_gate.tag_index,
                             start_row_index, end_row_index);
                }
                for (auto &constraint : bp.copy_constraints()) {
                    zones.union_set(
                        copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                            start_row_index, rows_amount, constraint.first),
                        copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                            start_row_index, rows_amount, constraint.second));
                }
                return zones;
            }
        }    // namespace detail

        // Ensure that output and input variables are connected via constraints.
        // This failing basically guarantees that the circuit is broken (or the check is).
        // There might exists rare components for which a lower level of connectedness is sufficient:
        // technically this checks that all inputs can affect all outputs.
        // Haven't seen a use case for a weaker check yet.
        template<typename BlueprintFieldType, typename ArithmetizationParams>
        bool check_connectedness(
            const nil::blueprint::assignment<
                nil::crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                &assignment,
            const nil::blueprint::circuit<
                nil::crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                &bp,
            const std::vector<nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>>
                &input_variables,
            const std::vector<nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>>
                &output_variables,
            std::size_t start_row_index, std::size_t rows_amount) {

            using detail::copy_var_address;
            auto zones = detail::generate_connectedness_zones(assignment, bp, input_variables,
                                                              start_row_index, rows_amount);
            std::size_t expected_zone = zones.find_set(
                copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                    start_row_index, rows_amount, input_variables[0]));
            for (auto &variable : input_variables) {
                if (zones.find_set(copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                                        start_row_index, rows_amount, variable)) != expected_zone) {
                    return false;
                }
            }
            for (auto &variable : output_variables) {
                if (zones.find_set(copy_var_address<BlueprintFieldType, ArithmetizationParams>(
                                        start_row_index, rows_amount, variable)) != expected_zone) {
                    return false;
                }
            }
            return true;
        }
    }   // namespace blueprint
}    // namespace nil

#endif // CRYPTO3_BLUEPRINT_UTILS_PLONK_CONNECTEDNESS_CHECK_HPP