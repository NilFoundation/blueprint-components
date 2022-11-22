//---------------------------------------------------------------------------//
// Copyright (c) 2022 Polina Chernyshova <pockvokhbtra@nil.foundation>
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

#define BOOST_TEST_MODULE blueprint_plonk_unified_addition_test

#include <boost/test/unit_test.hpp>

#include <nil/crypto3/algebra/curves/vesta.hpp>
#include <nil/crypto3/algebra/fields/arithmetic_params/vesta.hpp>
#include <nil/crypto3/algebra/curves/pallas.hpp>
#include <nil/crypto3/algebra/fields/arithmetic_params/pallas.hpp>
#include <nil/crypto3/algebra/random_element.hpp>

#include <nil/crypto3/hash/algorithm/hash.hpp>
#include <nil/crypto3/hash/sha2.hpp>
#include <nil/crypto3/hash/keccak.hpp>

#include <nil/crypto3/zk/snark/arithmetization/plonk/params.hpp>

#include <nil/crypto3/zk/blueprint/plonk.hpp>
#include <nil/crypto3/zk/assignment/plonk.hpp>
#include <nil/crypto3/zk/components/systems/snark/plonk/kimchi/detail/batch_scalar/prepare_scalars.hpp>

#include "test_plonk_component.hpp"

using namespace nil::crypto3;

BOOST_AUTO_TEST_SUITE(blueprint_plonk_test_suite)

BOOST_AUTO_TEST_CASE(blueprint_plonk_prepare_scalars_vesta) {
    auto start = std::chrono::high_resolution_clock::now();

    using curve_type = algebra::curves::vesta;
    using BlueprintFieldType = typename curve_type::scalar_field_type;
    constexpr std::size_t WitnessColumns = 15;
    constexpr std::size_t PublicInputColumns = 1;
    constexpr std::size_t ConstantColumns = 1;
    constexpr std::size_t SelectorColumns = 4;
    using ArithmetizationParams =
        zk::snark::plonk_arithmetization_params<WitnessColumns, PublicInputColumns, ConstantColumns, SelectorColumns>;
    using ArithmetizationType = zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>;
    using AssignmentType = zk::blueprint_assignment_table<ArithmetizationType>;
    using hash_type = nil::crypto3::hashes::keccak_1600<256>;
    constexpr std::size_t Lambda = 40;
    constexpr typename BlueprintFieldType::value_type  vesta_base_field_modulus = 0x40000000000000000000000000000000224698fc0994a8dd8c46eb2100000001_cppui256;
    constexpr typename BlueprintFieldType::value_type pallas_base_field_modulus = 0x40000000000000000000000000000000224698fc094cf91b992d30ed00000001_cppui256;
    using var = zk::snark::plonk_variable<BlueprintFieldType>;

    constexpr std::size_t InputSize = 4;

    using component_type = zk::components::prepare_scalars<ArithmetizationType, curve_type, 
        InputSize, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                                                            11, 12, 13, 14>;

    std::vector<typename BlueprintFieldType::value_type> scalars;
    std::vector<var> scalars_var(InputSize);
    for (int i = 0; i < InputSize; ++i) {
        if (i == 0) {
            scalars.push_back(1);
        } else if (i == 1) {
            scalars.push_back(0);
        } else if (i == 2) {
            scalars.push_back(-1);
        } else {
            scalars.push_back(algebra::random_element<BlueprintFieldType>());
        }
    }


    // scalars[0] = 0x0000000000000000000000000000000000000000000000000000000000000001_cppui256;
    // scalars[1] = 0x20E959472C74FBEA783D62870B979033E1BD490EDF01DD51D69F4C89B52DAA3B_cppui256;
    // scalars[2] = 0x244DCFC9E49D7EF7EE803233A777F72016706EDB1534F75F4F9561FF4EB23255_cppui256;
    // scalars[3] = -1;
    // scalars[4] = 0;    
    

    typename BlueprintFieldType::value_type base = 2;
    typename BlueprintFieldType::value_type shift;
    typename BlueprintFieldType::value_type denominator;

    if (curve_type::base_field_type::modulus - vesta_base_field_modulus == 0) {
        shift = base.pow(255);
        denominator = 1;
    }
    if (curve_type::base_field_type::modulus - pallas_base_field_modulus == 0) {
        shift = base.pow(255) + 1;
        denominator = base;
    }

    std::vector<typename BlueprintFieldType::value_type> expected_res;
    for (int i = 0; i < InputSize; ++i) {
        typename BlueprintFieldType::value_type expected;
        if ((scalars[i] != 1) & (scalars[i] != 0) & (scalars[i] != -1)){
            expected = (scalars[i] - base.pow(255) - 1) / 2;
        } else {
            expected = (scalars[i] - shift) / denominator;
        }
        expected_res.push_back(expected);
        // std::cout << "scalars unprepared[" << i << "] = " << scalars[i].data << std::endl;
        // std::cout << "scalars prepared  [" << i << "] = " << expected.data << std::endl;
    }

    std::vector<typename BlueprintFieldType::value_type> public_input;
    for (int i = 0; i < InputSize; ++i) {
        public_input.push_back(scalars[i]);
        scalars_var[i] = var(0, public_input.size() - 1, false, var::column_type::public_input);
    }

    typename component_type::params_type params = {
        scalars_var};

    auto result_check = [&expected_res](AssignmentType &assignment,
        component_type::result_type &real_res) {
        for (int i = 0; i < InputSize; ++i) {
            // std::cout << "assertion[" << i << "]:" << std::endl;
            // std::cout << expected_res[i].data  << " == " << assignment.var_value(real_res.output[i]).data << std::endl;
            assert(expected_res[i] == assignment.var_value(real_res.output[i]));
        }
    };

    test_component<component_type, BlueprintFieldType, ArithmetizationParams, hash_type, Lambda>(params, public_input, result_check);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
    std::cout << "prepare scalars: " << duration.count() << "ms" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
