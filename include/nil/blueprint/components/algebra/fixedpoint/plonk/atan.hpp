#ifndef CRYPTO3_BLUEPRINT_PLONK_FIXEDPOINT_ATAN_HPP
#define CRYPTO3_BLUEPRINT_PLONK_FIXEDPOINT_ATAN_HPP

#include "nil/blueprint/components/algebra/fixedpoint/plonk/tan.hpp"
#include "nil/blueprint/components/algebra/fixedpoint/plonk/range.hpp"

namespace nil {
    namespace blueprint {
        namespace components {

            // Works by proving that the output y = floor(atan(x))  The error of the output is at most 2^{-16}.

            /**
             * Component representing a atan operation with input x and output y, where y =
             * floor(atan(x)).
             *
             * The delta of y is equal to the delta of x.
             *
             * Input:    x  ... field element
             * Output:   y  ... atan(x) (field element)
             */
            template<typename ArithmetizationType, typename FieldType, typename NonNativePolicyType>
            class fix_atan;

            template<typename BlueprintFieldType, typename ArithmetizationParams, typename NonNativePolicyType>
            class fix_atan<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>,
                           BlueprintFieldType, NonNativePolicyType>
                : public plonk_component<BlueprintFieldType, ArithmetizationParams, 2, 0> {

            public:
                using value_type = typename BlueprintFieldType::value_type;

                using tan_component =
                    fix_tan<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>,
                            BlueprintFieldType, basic_non_native_policy<BlueprintFieldType>>;

                using range_component =
                    fix_range<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>,
                              BlueprintFieldType, basic_non_native_policy<BlueprintFieldType>>;

            private:
                tan_component tan;
                range_component range;

                static uint8_t M(uint8_t m) {
                    if (m == 0 || m > 2) {
                        BLUEPRINT_RELEASE_ASSERT(false);
                    }
                    return m;
                }

                tan_component instantiate_tan(uint8_t m1, uint8_t m2) const {
                    std::vector<std::uint32_t> witness_list;

                    auto witness_columns = tan_component::get_witness_columns(m1, m2);
                    BLUEPRINT_RELEASE_ASSERT(this->witness_amount() >= witness_columns);
                    witness_list.reserve(witness_columns);
                    for (auto i = 0; i < witness_columns; i++) {
                        witness_list.push_back(this->W(i));
                    }
                    return tan_component(witness_list, std::array<std::uint32_t, 1>({this->C(0)}),
                                         std::array<std::uint32_t, 0>(), m1, m2);
                }

                range_component instantiate_range(uint8_t m1, uint8_t m2) const {
                    std::vector<std::uint32_t> witness_list;

                    auto witness_columns = range_component::get_witness_columns(this->witness_amount(), m1, m2);
                    BLUEPRINT_RELEASE_ASSERT(this->witness_amount() >= witness_columns);
                    witness_list.reserve(witness_columns);
                    for (auto i = 0; i < witness_columns; i++) {
                        witness_list.push_back(this->W(i));
                    }
                    // TODO set to range -pi/2 < x < pi/2
                    auto high = 0;
                    auto low = -high;
                    return range_component(witness_list, std::array<std::uint32_t, 2>({this->C(0), this->C(1)}),
                                           std::array<std::uint32_t, 0>(), m1, m2, low, high);
                }

            public:
                uint8_t get_m() const {
                    return range.get_m();
                }

                uint8_t get_m1() const {
                    return range.get_m1();
                }

                uint8_t get_m2() const {
                    return range.get_m2();
                }

                value_type calc_atan(const value_type &x, uint8_t m1, uint8_t m2) const {
                    if (m1 == 1 && m2 == 1) {
                        auto el = FixedPoint<BlueprintFieldType, 1, 1>(x, 16);
                        return el.atan().get_value();
                    } else if (m1 == 2 && m2 == 1) {
                        auto el = FixedPoint<BlueprintFieldType, 2, 1>(x, 16);
                        return el.atan().get_value();
                    } else if (m1 == 1 && m2 == 2) {
                        auto el = FixedPoint<BlueprintFieldType, 1, 2>(x, 32);
                        return el.atan().get_value();
                    } else if (m1 == 2 && m2 == 2) {
                        auto el = FixedPoint<BlueprintFieldType, 2, 2>(x, 32);
                        return el.atan().get_value();
                    } else {
                        BLUEPRINT_RELEASE_ASSERT(false);
                        return 0;
                    }
                }

                const tan_component &get_tan_component() const {
                    return tan;
                }

                const range_component &get_range_component() const {
                    return range;
                }

                using component_type = plonk_component<BlueprintFieldType, ArithmetizationParams, 2, 0>;

                using var = typename component_type::var;
                using manifest_type = plonk_component_manifest;
                using lookup_table_definition =
                    typename nil::crypto3::zk::snark::lookup_table_definition<BlueprintFieldType>;
                using range_table = fixedpoint_range_table<BlueprintFieldType>;

                class gate_manifest_type : public component_gate_manifest {
                public:
                    std::uint32_t gates_amount() const override {
                        return fix_atan::gates_amount;
                    }
                };

                static gate_manifest get_gate_manifest(std::size_t witness_amount, std::size_t lookup_column_amount,
                                                       uint8_t m1 = 0, uint8_t m2 = 0) {
                    gate_manifest manifest =
                        gate_manifest(gate_manifest_type())
                            .merge_with(tan_component::get_gate_manifest(witness_amount, lookup_column_amount, m1, m2))
                            .merge_with(
                                range_component::get_gate_manifest(witness_amount, lookup_column_amount, m1, m2));
                    return manifest;
                }

                static manifest_type get_manifest(uint8_t m1, uint8_t m2) {
                    manifest_type manifest = manifest_type(std::shared_ptr<manifest_param>(new manifest_range_param(
                                                               std::max(6, 2 * (M(m1) + M(m2))), 6 + 2 * (m2 + m1))),
                                                           false)
                                                 .merge_with(tan_component::get_manifest(m1, m2))
                                                 .merge_with(range_component::get_manifest(m1, m2));
                    return manifest;
                }

                static std::size_t get_atan_rows_amount(std::size_t witness_amount, std::size_t lookup_column_amount,
                                                        uint8_t m1, uint8_t m2) {
                    if (6 + 2 * (M(m2) + M(m1)) <= witness_amount) {
                        return 1;
                    } else {
                        return 2;
                    }
                }

                static std::size_t get_rows_amount(std::size_t witness_amount, std::size_t lookup_column_amount,
                                                   uint8_t m1, uint8_t m2) {
                    auto tan_rows = tan_component::get_rows_amount(witness_amount, lookup_column_amount, m1, m2);
                    auto range_rows = range_component::get_rows_amount(witness_amount, lookup_column_amount, m1, m2);
                    auto atan_rows = get_atan_rows_amount(witness_amount, lookup_column_amount, m1, m2);
                    return 2 * tan_rows + range_rows + atan_rows;
                }

// Includes the constraints + lookup_gates
#ifdef TEST_WITHOUT_LOOKUP_TABLES
                constexpr static const std::size_t gates_amount = 1;
#else
                constexpr static const std::size_t gates_amount = 2;
#endif    // TEST_WITHOUT_LOOKUP_TABLES

                const std::size_t rows_amount = get_rows_amount(this->witness_amount(), 0, get_m1(), get_m2());
                const std::size_t atan_rows_amount =
                    get_atan_rows_amount(this->witness_amount(), 0, get_m1(), get_m2());

                struct input_type {
                    var x = var(0, 0, false);

                    std::vector<std::reference_wrapper<var>> all_vars() {
                        return {x};
                    }
                };

                struct var_positions {
                    CellPosition x, y, tan1_out, tan2_in, tan2_out, in_range, a0, b0;
                    int64_t tan1_row, tan2_row, range_row;
                };

                var_positions get_var_pos(const int64_t start_row_index) const {

                    auto m = this->get_m();
                    var_positions pos;
                    pos.tan1_row = start_row_index;
                    pos.tan2_row = pos.tan1_row + tan.rows_amount;
                    pos.range_row = pos.tan2_row + tan.rows_amount;
                    int64_t row_index = pos.range_row + range.rows_amount;

                    switch (this->atan_rows_amount) {
                        case 1:

                            // trace layout (6 + 2*m col(s), 2 constant col(s), 1 row(s))
                            //
                            //                |                witness
                            //     r\c        | 0 | 1 |     2    |    3    |     4    |     5    |
                            // +--------------+---+---+----------+---------+----------+----------| ...
                            // | tan1_row(s)  |              <tan_witnesses>
                            // | tan2_row(s)  |              <tan_witnesses>
                            // | range_row(s) |              <range_witnesses>
                            // |      0       | x | y | tan1_out | tan2_in | tan2_out | in_range |

                            //            witness                |    constant   |
                            //     | 6  |..|6+m-1 |6+m |..|6+2m-1|   0   |   1   |
                            // ... +----+--+------+----+--+------+-------+-------+
                            //            <tan_witnesses>        |  <tan_const>  |
                            //            <tan_witnesses>        |  <tan_const>  |
                            //            <range_witnesses>      | <range_const> |
                            //     | a0 |..| am-1 | b0 |..| bm-1 |   -   |   -   |

                            pos.x = CellPosition(this->W(0), row_index);
                            pos.y = CellPosition(this->W(1), row_index);
                            pos.tan1_out = CellPosition(this->W(2), row_index);
                            pos.tan2_in = CellPosition(this->W(3), row_index);
                            pos.tan2_out = CellPosition(this->W(4), row_index);
                            pos.in_range = CellPosition(this->W(5), row_index);
                            pos.a0 = CellPosition(this->W(6 + 0 * m), row_index);    // occupies m cells
                            pos.b0 = CellPosition(this->W(6 + 1 * m), row_index);    // occupies m cells
                            break;
                        case 2:

                            // trace layout (max(6, 2 * m), 2 constant col(s), 2 row(s))
                            //
                            //                |           witness              |   constant    |
                            //      r\c       |  0 |..|  m-1  | m  | .. | 2m-1 |   0   |   1   |
                            // +--------------+----+--+-------+----+----+------+------+--------+
                            // | tan1_row(s)  |       <tan_witnesses>          | <tan_const>   |
                            // | tan1_row(s)  |       <tan_witnesses>          | <tan_const>   |
                            // | range_row(s) |       <range_witnesses>        | <range_const> |
                            // |      0       | a0 |..| am-1  | b0 | .. | bm-1 |   -   |   -   |

                            //               |                  witness                         |   constant  |
                            //      r\c      | 0 | 1 |    2     |    3    |    4     |    5     |   0  |   1  |
                            // +-------------+---+---+----------+---------+----------+----------+------+------+
                            // |      1      | x | y | tan1_out | tan2_in | tan2_out | in_range |   -  |   -  |

                            pos.a0 = CellPosition(this->W(0 + 0 * m), row_index);    // occupies m cells
                            pos.b0 = CellPosition(this->W(0 + 1 * m), row_index);    // occupies m cells
                            pos.x = CellPosition(this->W(0), row_index + 1);
                            pos.y = CellPosition(this->W(1), row_index + 1);
                            pos.tan1_out = CellPosition(this->W(2), row_index + 1);
                            pos.tan2_in = CellPosition(this->W(3), row_index + 1);
                            pos.tan2_out = CellPosition(this->W(4), row_index + 1);
                            pos.in_range = CellPosition(this->W(5), row_index + 1);
                            break;
                        default:
                            BLUEPRINT_RELEASE_ASSERT(false &&
                                                     "atan rows_amount (i.e., without tan and range) must be 1 or 2");
                    }
                    return pos;
                }

                struct result_type {
                    var output = var(0, 0, false);
                    result_type(const fix_atan &component, std::uint32_t start_row_index) {
                        const auto var_pos = component.get_var_pos(static_cast<int64_t>(start_row_index));
                        output = var(splat(var_pos.y), false);
                    }

                    result_type(const fix_atan &component, std::size_t start_row_index) {
                        const auto var_pos = component.get_var_pos(static_cast<int64_t>(start_row_index));
                        output = var(splat(var_pos.y), false);
                    }

                    std::vector<var> all_vars() const {
                        return {output};
                    }
                };

// Allows disabling the lookup tables for faster testing
#ifndef TEST_WITHOUT_LOOKUP_TABLES
                std::vector<std::shared_ptr<lookup_table_definition>> component_custom_lookup_tables() {
                    auto result = tan.component_custom_lookup_tables();
                    auto range_tables = range.component_custom_lookup_tables();
                    result.reserve(result.size() + range_tables.size());
                    result.insert(result.end(), range_tables.begin(), range_tables.end());
                    return result;
                }

                std::map<std::string, std::size_t> component_lookup_tables() {
                    auto result = tan.component_lookup_tables();
                    auto range_tables = range.component_lookup_tables();
                    result.insert(range_tables.begin(), range_tables.end());
                    return result
                }
#endif

                template<typename WitnessContainerType, typename ConstantContainerType,
                         typename PublicInputContainerType>
                fix_atan(WitnessContainerType witness, ConstantContainerType constant,
                         PublicInputContainerType public_input, uint8_t m1, uint8_t m2) :
                    component_type(witness, constant, public_input, get_manifest(m1, m2)),
                    tan(instantiate_tan(m1, m2)), range(instantiate_range(m1, m2)) {};

                fix_atan(std::initializer_list<typename component_type::witness_container_type::value_type> witnesses,
                         std::initializer_list<typename component_type::constant_container_type::value_type> constants,
                         std::initializer_list<typename component_type::public_input_container_type::value_type>
                             public_inputs,
                         uint8_t m1, uint8_t m2) :
                    component_type(witnesses, constants, public_inputs, get_manifest(m1, m2)),
                    tan(instantiate_tan(m1, m2)), range(instantiate_range(m1, m2)) {};
            };

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            using plonk_fixedpoint_atan =
                fix_atan<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>,
                         BlueprintFieldType, basic_non_native_policy<BlueprintFieldType>>;

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::result_type generate_assignments(
                const plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams> &component,
                assignment<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    &assignment,
                const typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::input_type
                    instance_input,
                const std::uint32_t start_row_index) {

                using var = typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::var;

                const auto var_pos = component.get_var_pos(static_cast<int64_t>(start_row_index));

                auto tan_comp = component.get_tan_component();
                auto range_comp = component.get_range_component();

                // Tan inputs
                typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::tan_component::input_type
                    tan1_input,
                    tan2_input;
                tan1_input.x = var(splat(var_pos.y), false);
                tan2_input.x = var(splat(var_pos.tan2_in), false);

                // Range input
                typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::range_component::input_type
                    range_input;
                range_input.x = var(splat(var_pos.y), false);

                ////////////////////////////////////////////////////////
                // Build the trace
                ////////////////////////////////////////////////////////

                auto m1 = component.get_m1();
                auto m2 = component.get_m2();

                auto x_val = var_value(assignment, instance_input.x);
                auto y_val = component.calc_atan(x_val, m1, m2);

                auto tan2_in_val = y_val - 1;

                assignment.witness(splat(var_pos.x)) = x_val;
                assignment.witness(splat(var_pos.y)) = y_val;
                assignment.witness(splat(var_pos.tan2_in)) = tan2_in_val;

                // Assign tan gadgets
                auto tan1_out = generate_assignments(tan_comp, assignment, tan1_input, var_pos.tan1_row);
                auto tan2_out = generate_assignments(tan_comp, assignment, tan2_input, var_pos.tan2_row);

                auto tan1_out_val = var_value(assignment, tan1_out.output);
                auto tan2_out_val = var_value(assignment, tan2_out.output);
                assignment.witness(splat(var_pos.tan1_out)) = tan1_out_val;
                assignment.witness(splat(var_pos.tan2_out)) = tan2_out_val;

                // Assign range gadgets
                auto range_out = generate_assignments(range_comp, assignment, range_input, var_pos.range_row);

                auto range_out_val = var_value(assignment, range_out.in);
                assignment.witness(splat(var_pos.in_range)) = range_out_val;

                // Decompositions
                auto a_val = tan1_out_val - x_val;
                auto b_val = x_val - tan2_out_val - 1;

                std::vector<uint16_t> a0_val;
                std::vector<uint16_t> b0_val;

                bool sign = FixedPointHelper<BlueprintFieldType>::decompose(a_val, a0_val);
                if (sign) {
                    std::cout << tan1_out_val << " " << x_val << std::endl;
                }
                BLUEPRINT_RELEASE_ASSERT(!sign);
                sign = FixedPointHelper<BlueprintFieldType>::decompose(b_val, b0_val);
                BLUEPRINT_RELEASE_ASSERT(!sign);

                // is ok because decomp is at least of size 4 and the biggest we have is 32.32
                auto m = component.get_m();
                BLUEPRINT_RELEASE_ASSERT(a0_val.size() >= m);
                BLUEPRINT_RELEASE_ASSERT(b0_val.size() >= m);

                for (auto i = 0; i < m; i++) {
                    assignment.witness(var_pos.a0.column() + i, var_pos.a0.row()) = a0_val[i];
                    assignment.witness(var_pos.b0.column() + i, var_pos.b0.row()) = b0_val[i];
                }

                return typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::result_type(
                    component, start_row_index);
            }

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            std::size_t generate_gates(
                const plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams> &component,
                circuit<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>> &bp,
                assignment<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    &assignment,
                const typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::input_type
                    &instance_input) {

                using var = typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::var;
                auto m = component.get_m();
                const int64_t start_row_index = 1 - static_cast<int64_t>(component.rows_amount);
                const auto var_pos = component.get_var_pos(start_row_index);

                auto a0 = nil::crypto3::math::expression(var(splat(var_pos.a0)));
                auto b0 = nil::crypto3::math::expression(var(splat(var_pos.b0)));
                for (auto i = 1; i < m; i++) {
                    a0 += var(var_pos.a0.column() + i, var_pos.a0.row()) * (1ULL << (16 * i));
                    b0 += var(var_pos.b0.column() + i, var_pos.b0.row()) * (1ULL << (16 * i));
                }

                auto x = var(splat(var_pos.x));
                auto y = var(splat(var_pos.y));
                auto in_range = var(splat(var_pos.in_range));
                auto tan1_out = var(splat(var_pos.tan1_out));
                auto tan2_in = var(splat(var_pos.tan2_in));
                auto tan2_out = var(splat(var_pos.tan2_out));

                auto constraint_1 = tan1_out - x - a0;
                auto constraint_2 = x - tan2_out - 1 - b0;
                auto constraint_3 = y - 1 - tan2_in;
                auto constraint_4 = in_range - 1;

                // TODO activate again
                // return bp.add_gate({constraint_1, constraint_2, constraint_3, constraint_4});
                return bp.add_gate({constraint_1, constraint_2, constraint_3});
            }

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            std::size_t generate_lookup_gates(
                const plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams> &component,
                circuit<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>> &bp,
                assignment<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    &assignment,
                const typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::input_type
                    &instance_input) {
                int64_t start_row_index = 1 - static_cast<int64_t>(component.rows_amount);
                const auto var_pos = component.get_var_pos(start_row_index);
                auto m = component.get_m();

                const auto &lookup_tables_indices = bp.get_reserved_indices();

                using var = typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::var;
                using constraint_type = typename crypto3::zk::snark::plonk_lookup_constraint<BlueprintFieldType>;
                using range_table =
                    typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::range_table;

                std::vector<constraint_type> constraints;
                constraints.reserve(2 * m);

                auto table_id = lookup_tables_indices.at(range_table::FULL_TABLE_NAME);
                BLUEPRINT_RELEASE_ASSERT(var_pos.a0.row() == var_pos.b0.row());

                for (auto i = 0; i < m; i++) {
                    constraint_type constraint_a, constraint_b;
                    constraint_a.table_id = table_id;
                    constraint_b.table_id = table_id;

                    // We put row=0 here and enable the selector in the correct one
                    auto ai = var(var_pos.a0.column() + i, 0);
                    auto bi = var(var_pos.b0.column() + i, 0);
                    constraint_a.lookup_input = {ai};
                    constraint_b.lookup_input = {bi};
                    constraints.push_back(constraint_a);
                    constraints.push_back(constraint_b);
                }

                return bp.add_lookup_gate(constraints);
            }

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            void generate_copy_constraints(
                const plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams> &component,
                circuit<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>> &bp,
                assignment<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    &assignment,
                const typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::input_type
                    &instance_input,
                const std::size_t start_row_index) {

                using var = typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::var;

                const auto var_pos = component.get_var_pos(static_cast<int64_t>(start_row_index));

                auto tan_comp = component.get_tan_component();
                auto range_comp = component.get_range_component();

                auto tan1_res = tan_comp.get_result((std::size_t)var_pos.tan1_row);
                auto tan2_res = tan_comp.get_result((std::size_t)var_pos.tan2_row);
                auto range_res = range_comp.get_result((std::size_t)var_pos.range_row);

                auto x = var(splat(var_pos.x), false);
                auto tan1_out = var(splat(var_pos.tan1_out), false);
                auto tan2_out = var(splat(var_pos.tan2_out), false);
                auto in_range = var(splat(var_pos.in_range), false);

                bp.add_copy_constraint({instance_input.x, x});
                bp.add_copy_constraint({tan1_res.output, tan1_out});
                bp.add_copy_constraint({tan2_res.output, tan2_out});
                bp.add_copy_constraint({range_res.in, in_range});
            }

            template<typename BlueprintFieldType, typename ArithmetizationParams>
            typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::result_type generate_circuit(
                const plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams> &component,
                circuit<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>> &bp,
                assignment<crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>>
                    &assignment,
                const typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::input_type
                    &instance_input,
                const std::size_t start_row_index) {

                using var = typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::var;

                const auto var_pos = component.get_var_pos(static_cast<int64_t>(start_row_index));
                auto tan_comp = component.get_tan_component();
                auto range_comp = component.get_range_component();

                // Tan inputs
                typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::tan_component::input_type
                    tan1_input,
                    tan2_input;
                tan1_input.x = var(splat(var_pos.y), false);
                tan2_input.x = var(splat(var_pos.tan2_in), false);

                // Range input
                typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::range_component::input_type
                    range_input;
                range_input.x = var(splat(var_pos.y), false);

                // Enable the tan components
                generate_circuit(tan_comp, bp, assignment, tan1_input, var_pos.tan1_row);
                generate_circuit(tan_comp, bp, assignment, tan2_input, var_pos.tan2_row);

                // Enable the range component
                generate_circuit(range_comp, bp, assignment, range_input, var_pos.range_row);

                // Enable the atan component
                std::size_t selector_index = generate_gates(component, bp, assignment, instance_input);

// Allows disabling the lookup tables for faster testing
#ifndef TEST_WITHOUT_LOOKUP_TABLES
                // Enable the atan lookup tables
                std::size_t lookup_selector_index = generate_lookup_gates(component, bp, assignment, instance_input);
                assignment.enable_selector(lookup_selector_index, var_pos.a0.row());    // same as b0.row()
#endif

                // selector goes onto last row and gate uses all rows
                assignment.enable_selector(selector_index, start_row_index + component.rows_amount - 1);

                generate_copy_constraints(component, bp, assignment, instance_input, start_row_index);

                return typename plonk_fixedpoint_atan<BlueprintFieldType, ArithmetizationParams>::result_type(
                    component, start_row_index);
            }

        }    // namespace components
    }        // namespace blueprint
}    // namespace nil

#endif    // CRYPTO3_BLUEPRINT_PLONK_FIXEDPOINT_ATAN_HPP