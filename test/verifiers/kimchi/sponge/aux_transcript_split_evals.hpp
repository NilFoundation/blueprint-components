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

#ifndef CRYPTO3_ZK_BLUEPRINT_PLONK_AUXILIARY_SPONGE_SPLIT_EVALS_HPP
#define CRYPTO3_ZK_BLUEPRINT_PLONK_AUXILIARY_SPONGE_SPLIT_EVALS_HPP

#include <nil/marshalling/algorithms/pack.hpp>

#include <nil/crypto3/zk/snark/arithmetization/plonk/constraint_system.hpp>

#include <nil/crypto3/zk/blueprint/plonk.hpp>
#include <nil/crypto3/zk/assignment/plonk.hpp>
#include <nil/crypto3/zk/algorithms/generate_circuit.hpp>
#include <nil/crypto3/zk/components/systems/snark/plonk/kimchi/detail/transcript_fr.hpp>
#include <nil/crypto3/zk/components/systems/snark/plonk/pickles/types/instance.hpp>

namespace nil {
    namespace crypto3 {
        namespace zk {
            namespace components {

                template<typename ArithmetizationType,
                         typename CurveType,
                         typename KimchiParamsType,
                         std::size_t... WireIndexes>
                class aux_split_evals;

                template<typename BlueprintFieldType,
                         typename ArithmetizationParams,
                         typename CurveType,
                         typename KimchiParamsType,
                         std::size_t W0,
                         std::size_t W1,
                         std::size_t W2,
                         std::size_t W3,
                         std::size_t W4,
                         std::size_t W5,
                         std::size_t W6,
                         std::size_t W7,
                         std::size_t W8,
                         std::size_t W9,
                         std::size_t W10,
                         std::size_t W11,
                         std::size_t W12,
                         std::size_t W13,
                         std::size_t W14>
                class aux_split_evals<
                    snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>,
                    CurveType,
                    KimchiParamsType,
                    W0, W1, W2, W3,
                    W4, W5, W6, W7,
                    W8, W9, W10, W11,
                    W12, W13, W14> {

                    typedef snark::plonk_constraint_system<BlueprintFieldType,
                        ArithmetizationParams> ArithmetizationType;

                    using var = snark::plonk_variable<BlueprintFieldType>;
                    using transcript_type =
                        zk::components::kimchi_transcript_fr<ArithmetizationType, CurveType, KimchiParamsType, W0, W1, W2, W3, W4, W5, W6,
                                                                            W7, W8, W9, W10, W11, W12, W13, W14>;

                    using evals_type = typename zk::components::proof_type<BlueprintFieldType, KimchiParamsType>
                                                  ::prev_evals_type;
                public:
                    constexpr static const std::size_t selector_seed = 0x0fd9;
                    constexpr static const std::size_t rows_amount =
                        transcript_type::init_rows +
                        transcript_type::absorb_split_evaluations_rows +
                        2 * transcript_type::challenge_rows;
                    constexpr static const std::size_t gates_amount = 0;
                    constexpr static const std::size_t state_size = transcript_type::state_size;

                    struct params_type {
                        evals_type input;
                    };

                    struct result_type {
                        struct {
                            var challenge_1;
                            var challenge_2;
                            std::array<var, transcript_type::state_size> state;
                        } output;

                        result_type() = default;
                        result_type(var challenge_1, var challenge_2, std::array<var, transcript_type::state_size> state) :
                            output{challenge_1, challenge_2, state} {
                        }
                    };

                    static result_type generate_circuit(blueprint<ArithmetizationType> &bp,
                        blueprint_public_assignment_table<ArithmetizationType> &assignment,
                        const params_type &params,
                        const std::size_t start_row_index){

                        std::size_t row = start_row_index;
                        var zero = var(0, start_row_index, false, var::column_type::constant);
                        generate_assignments_constant(bp, assignment, params, row);

                        transcript_type transcript;
                        transcript.init_circuit(bp, assignment, zero, row);
                        row += transcript_type::init_rows;

                        transcript.absorb_split_evaluations_circuit(bp, assignment, params.input, row);
                        row += transcript_type::absorb_split_evaluations_rows;

                        var challenge_1 = transcript.challenge_circuit(bp, assignment, row);
                        row += transcript_type::challenge_rows;
                        var challenge_2 = transcript.challenge_circuit(bp, assignment, row);
                        row += transcript_type::challenge_rows;
                        std::array<var, transcript_type::state_size> state = transcript.state();

                        assert(row == start_row_index + rows_amount);
                        return result_type(challenge_1, challenge_2, state);
                    }

                    static result_type generate_assignments(
                            blueprint_assignment_table<ArithmetizationType> &assignment,
                            const params_type &params,
                            const std::size_t start_row_index){
                        std::size_t row = start_row_index;
                        var zero = var(0, start_row_index, false, var::column_type::constant);

                        transcript_type transcript;
                        transcript.init_assignment(assignment, zero, row);
                        row += transcript_type::init_rows;

                        transcript.absorb_split_evaluations_assignment(assignment, params.input, row);
                        row += transcript_type::absorb_split_evaluations_rows;

                        // real challenges are generated by .challenge_assignment() method
                        // this is a dummy full squeeze as I was unable to procure testing data
                        var challenge_1 = transcript.challenge_assignment(assignment, row);
                        row += transcript_type::challenge_rows;
                        var challenge_2 = transcript.challenge_assignment(assignment, row);
                        row += transcript_type::challenge_rows;
                        std::array<var, transcript_type::state_size> state = transcript.state();

                        assert(row == start_row_index + rows_amount);
                        return result_type(challenge_1, challenge_2, state);
                    }

                private:
                    static void generate_assignments_constant(
                            blueprint<ArithmetizationType> &bp,
                            blueprint_public_assignment_table<ArithmetizationType> &assignment,
                            const params_type &params,
                            std::size_t component_start_row) {
                        std::size_t row = component_start_row;

                        assignment.constant(0)[row] = 0;
                        row++;
                    }
                };
            }    // namespace components
        }        // namespace zk
    }            // namespace crypto3
}    // namespace nil

#endif // CRYPTO3_ZK_BLUEPRINT_PLONK_AUXILIARY_SPONGE_SPLIT_EVALS_HPP