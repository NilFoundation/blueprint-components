//---------------------------------------------------------------------------//
// Copyright (c) 2018-2022 Mikhail Komarov <nemo@nil.foundation>
// Copyright (c) 2020-2022 Nikita Kaskov <nbering@nil.foundation>
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

#define BOOST_TEST_MODULE fixed_base_scalar_mul_test

#include <boost/test/unit_test.hpp>

#include <nil/crypto3/algebra/curves/pallas.hpp>
#include <nil/crypto3/algebra/fields/arithmetic_params/pallas.hpp>
#include <nil/crypto3/algebra/random_element.hpp>

#include <nil/blueprint_mc/components/algebra/curves/pasta/plonk/fixed_base_scalar_mul_15_wires.hpp>

#include "../../../test_plonk_component_mc.hpp"

using namespace nil::crypto3;

BOOST_AUTO_TEST_SUITE(fixed_base_scalar_mul_test_suite)

BOOST_AUTO_TEST_CASE(fixed_base_scalar_mul_15_wires_test_case) {

	using curve_type = algebra::curves::pallas;
	using BlueprintFieldType = typename curve_type::base_field_type;
	constexpr std::size_t WitnessColumns = 15;
    constexpr std::size_t PublicInputColumns = 5;
    constexpr std::size_t ConstantColumns = 0;
    constexpr std::size_t SelectorColumns = 5;
	using ArithmetizationParams = zk::snark::plonk_arithmetization_params<WitnessColumns,
        PublicInputColumns, ConstantColumns, SelectorColumns>;
	using ArithmetizationType = zk::snark::plonk_constraint_system<BlueprintFieldType, ArithmetizationParams>;

	using component_type = nil::blueprint_mc::components::element_g1_fixed_base_scalar_mul<ArithmetizationType,
		curve_type, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14>;

	typename curve_type::template g1_type<>::value_type B =
		curve_type::template g1_type<>::value_type::one();
	typename curve_type::scalar_field_type::value_type a =
		curve_type::scalar_field_type::value_type::one();
	typename curve_type::scalar_field_type::value_type s =
		curve_type::scalar_field_type::value_type::one();
	typename curve_type::template g1_type<>::value_type P =
		curve_type::template g1_type<>::value_type::one();
	typename component_type::public_params_type init_params = {B};
	typename component_type::private_params_type assignment_params = {a, s, P};

	nil::blueprint_mc::test_component<component_type, BlueprintFieldType, ArithmetizationParams> (init_params, assignment_params);
}

BOOST_AUTO_TEST_SUITE_END()