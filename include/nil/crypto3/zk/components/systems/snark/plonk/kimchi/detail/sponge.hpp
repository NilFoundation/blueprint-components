//---------------------------------------------------------------------------//
// Copyright (c) 2021 Mikhail Komarov <nemo@nil.foundation>
// Copyright (c) 2021 Nikita Kaskov <nbering@nil.foundation>
// Copyright (c) 2022 Ilia Shirobokov <i.shirobokov@nil.foundation>
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

#ifndef CRYPTO3_ZK_BLUEPRINT_PLONK_KIMCHI_SPONGE_HPP
#define CRYPTO3_ZK_BLUEPRINT_PLONK_KIMCHI_SPONGE_HPP

#include <nil/marshalling/algorithms/pack.hpp>

#include <nil/crypto3/zk/snark/arithmetization/plonk/constraint_system.hpp>

#include <nil/crypto3/zk/blueprint/plonk.hpp>
#include <nil/crypto3/zk/assignment/plonk.hpp>
#include <nil/crypto3/zk/component.hpp>

#include <nil/crypto3/zk/components/hashes/poseidon/plonk/poseidon_15_wires.hpp>
#include <nil/crypto3/zk/components/algebra/fields/plonk/field_operations.hpp>

namespace nil {
    namespace crypto3 {
        namespace zk {
            namespace components {

                template<typename ArithmetizationType, typename CurveType, std::size_t... WireIndexes>
                class kimchi_sponge;

                template<typename BlueprintFieldType, typename ArithmetizationParams, typename CurveType,
                         std::size_t W0, std::size_t W1, std::size_t W2, std::size_t W3, std::size_t W4, std::size_t W5,
                         std::size_t W6, std::size_t W7, std::size_t W8, std::size_t W9, std::size_t W10,
                         std::size_t W11, std::size_t W12, std::size_t W13, std::size_t W14>
                class kimchi_sponge<snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>,
                                    CurveType, W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10, W11, W12, W13, W14> {

                    typedef snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>
                        ArithmetizationType;

                    using var = snark::plonk_variable<BlueprintFieldType>;
                    using poseidon_component =
                        zk::components::poseidon<ArithmetizationType, BlueprintFieldType, W0, W1, W2, W3, W4, W5, W6,
                                                 W7, W8, W9, W10, W11, W12, W13, W14>;
                    using add_component = zk::components::addition<ArithmetizationType, W0, W1, W2>;
                    std::size_t state_count = 0;
                    bool state_absorbed = true;

                    std::size_t add_input_gate_index;
                    std::size_t permute_gate_index;

                    std::array<var, poseidon_component::state_size> state = {var(W0, 0), var(W1, 0), var(W2, 0)};

                    var permute_assignment(blueprint_assignment_table<ArithmetizationType> &assignment,
                                           std::size_t &component_start_row) {

                        poseidon_component::result_type poseidon_res =
                            poseidon_component::generate_assignments(assignment, {state}, component_start_row);

                        component_start_row += poseidon_component::rows_amount;

                        for (std::size_t i = 0; i < poseidon_component::state_size; i++) {
                            state[i] = poseidon_res.output_state[i];
                        }
                    }

                    void add_input_assignment(blueprint_assignment_table<ArithmetizationType> &assignment,
                                              var &input,
                                              std::size_t state_index,
                                              std::size_t &row) {

                        var addition_result =
                            add_component::generate_assignment(assignment, {input, state[state_index]}, row);
                        row += add_component::rows_amount;
                        state[state_index] = addition_result.res;
                    }

                    void permute_circuit(blueprint<ArithmetizationType> &bp,
                                         blueprint_public_assignment_table<ArithmetizationType> &assignment,
                                         std::size_t &component_start_row) {

                        poseidon_component::result_type poseidon_res =
                            poseidon_component::generate_circuit(assignment, {state}, component_start_row);

                        component_start_row += poseidon_component::rows_amount;

                        for (std::size_t i = 0; i < poseidon_component::state_size; i++) {
                            state[i] = poseidon_res.output_state[i];
                        }
                    }

                    void add_input_circuit(blueprint<ArithmetizationType> &bp,
                                           blueprint_public_assignment_table<ArithmetizationType> &assignment,
                                           const var &input,
                                           std::size_t &component_start_row) {

                        var addition_result =
                            add_component::generate_circuit(assignment, {input, state[state_index]}, row);
                        row += add_component::rows_amount;
                        state[state_index] = addition_result.res;
                    }

                public:
                    void init_assignment(blueprint_assignment_table<ArithmetizationType> &assignment,
                                         std::size_t &component_start_row) {

                        for (std::size_t i = 0; i < poseidon_component::state_size; i++) {
                            assignment.witness(W0 + i)[component_start_row] = 0;
                            state[i] = var(W0 + i, component_start_row, false);
                        }

                        component_start_row++;
                    }

                    void init_generate_constraints(blueprint<ArithmetizationType> &bp,
                                                   blueprint_public_assignment_table<ArithmetizationType> &assignment,
                                                   const var &zero,
                                                   std::size_t &component_start_row) {

                        for (std::size_t i = 0; i < poseidon_component::state_size; i++) {
                            state[i] = var(W0 + i, component_start_row, false);
                            bp.add_copy_constraint(zero, state[i]);
                        }

                        component_start_row++;
                    }

                    void absorb_assignment(blueprint_assignment_table<ArithmetizationType> &assignment,
                                           var absorbing_value,
                                           std::size_t &component_start_row) {

                        if (this->state_absorbed) {
                            if (this->state_count == poseidon_component::rate) {
                                permute_assignment(assignment, component_start_row);

                                add_input_assignment(assignment, absorbing_value, 0, component_start_row);

                                this->state_count = 1;
                            } else {
                                add_input_assignment(assignment, absorbing_value, this->state_count,
                                                     component_start_row);

                                this->state_count++;
                            }
                        } else {
                            add_input_assignment(assignment, absorbing_value, 0, component_start_row);

                            this->state_absorbed = true;
                            this->state_count = 1;
                        }
                    }

                    void absorb_circuit(blueprint<ArithmetizationType> &bp,
                                        blueprint_public_assignment_table<ArithmetizationType> &assignment,
                                        const var &zero,
                                        std::size_t &component_start_row) {

                        if (this->state_absorbed) {
                            if (this->state_count == poseidon_component::rate) {
                                permute_circuit(assignment, component_start_row);

                                add_input_circuit(assignment, absorbing_value, 0, component_start_row);

                                this->state_count = 1;
                            } else {
                                add_input_circuit(assignment, absorbing_value, this->state_count, component_start_row);

                                this->state_count++;
                            }
                        } else {
                            add_input_circuit(assignment, absorbing_value, 0, component_start_row);

                            this->state_absorbed = true;
                            this->state_count = 1;
                        }
                    }

                    var squeeze_assignment(blueprint_assignment_table<ArithmetizationType> &assignment,
                                           var absorbing_value,
                                           std::size_t &component_start_row) {
                        if (!this->state_absorbed) {    // state = squeezed
                            if (this->state_count == poseidon_component::rate) {
                                permute_assignment(assignment, component_start_row);
                                this->state_count = 1;
                                // TODO: poseidon should return var
                                return var(W0, component_start_row - 1, false);
                            } else {
                                this->state_count++;

                                return var(W0 + this->state_count, component_start_row - 1, false);
                            }
                        } else {
                            permute_assignment(assignment, component_start_row);

                            this->state_absorbed = false;
                            this->state_count = 1;

                            return var(W0, component_start_row - 1, false);
                        }
                    }

                    void squeeze_circuit(blueprint<ArithmetizationType> &bp,
                                         blueprint_public_assignment_table<ArithmetizationType> &assignment,
                                         const var &zero,
                                         std::size_t &component_start_row) {

                        if (!this->state_absorbed) {    // state = squeezed
                            if (this->state_count == poseidon_component::rate) {
                                permute_circuit(assignment, component_start_row);
                                this->state_count = 1;
                                // TODO: poseidon should return var
                                return var(W0, component_start_row - 1, false);
                            } else {
                                this->state_count++;

                                return var(W0 + this->state_count, component_start_row - 1, false);
                            }
                        } else {
                            permute_circuit(assignment, component_start_row);

                            this->state_absorbed = false;
                            this->state_count = 1;

                            return var(W0, component_start_row - 1, false);
                        }
                    }
                };
            }    // namespace components
        }        // namespace zk
    }            // namespace crypto3
}    // namespace nil

#endif    // CRYPTO3_ZK_BLUEPRINT_PLONK_KIMCHI_SPONGE_HPP