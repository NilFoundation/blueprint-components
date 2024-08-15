//---------------------------------------------------------------------------//
// Copyright (c) 2023 Polina Chernyshova <pockvokhbtra@nil.foundation>
//               2024 Valeh Farzaliyev <estoniaa@nil.foundation>
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

#define BOOST_TEST_MODULE plonk_keccak_test

#include <array>
#include <cstdlib>
#include <ctime>
#include <random>

#include <boost/test/unit_test.hpp>

#include <nil/crypto3/algebra/curves/vesta.hpp>
#include <nil/crypto3/algebra/fields/arithmetic_params/vesta.hpp>
#include <nil/crypto3/algebra/curves/pallas.hpp>
#include <nil/crypto3/algebra/fields/arithmetic_params/pallas.hpp>
#include <nil/crypto3/algebra/curves/alt_bn128.hpp>
#include <nil/crypto3/algebra/fields/arithmetic_params/alt_bn128.hpp>

#include <nil/crypto3/hash/algorithm/hash.hpp>
#include <nil/crypto3/hash/adaptor/hashed.hpp>
#include <nil/crypto3/hash/keccak.hpp>
// #include <nil/crypto3/random/algebraic_engine.hpp>

#include <nil/crypto3/zk/snark/arithmetization/plonk/params.hpp>

#include <nil/blueprint/blueprint/plonk/circuit.hpp>
#include <nil/blueprint/blueprint/plonk/assignment.hpp>

#include <nil/blueprint/components/hashes/keccak/keccak_component.hpp>

#include "../../test_plonk_component.hpp"

const int r[5][5] = {{0, 36, 3, 41, 18},
                     {1, 44, 10, 45, 2},
                     {62, 6, 43, 15, 61},
                     {28, 55, 25, 21, 56},
                     {27, 20, 39, 8, 14}};

template<typename BlueprintFieldType>
std::size_t number_bits(typename BlueprintFieldType::value_type value) {
    using integral_type = typename BlueprintFieldType::integral_type;

    integral_type integral_value = integral_type(value.data);
    std::size_t result = 0;
    while (integral_value > 0) {
        integral_value >>= 1;
        ++result;
    }
    return result;
}

template<typename BlueprintFieldType>
typename BlueprintFieldType::value_type to_sparse(typename BlueprintFieldType::value_type value) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;
    integral_type value_integral = integral_type(value.data);
    integral_type result_integral = 0;
    integral_type power = 1;
    for (int i = 0; i < 64; ++i) {
        integral_type bit = value_integral & 1;
        result_integral = result_integral + bit * power;
        value_integral = value_integral >> 1;
        power = power << 3;
    }
    return value_type(result_integral);
}

template<typename BlueprintFieldType>
typename BlueprintFieldType::value_type to_le(typename BlueprintFieldType::value_type value) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;
    integral_type value_integral = integral_type(value.data);
    integral_type result_integral = 0;
    for (int i = 0; i < 64; ++i) {
        integral_type bit = value_integral & 1;
        result_integral = (result_integral << 1) + bit;
        value_integral = value_integral >> 1;
    }
    return value_type(result_integral);
}

template<typename BlueprintFieldType>
typename BlueprintFieldType::value_type to_le_bytes(typename BlueprintFieldType::value_type value) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;
    integral_type value_integral = integral_type(value.data);
    integral_type result_integral = 0;
    for (int i = 0; i < 64; i += 8) {
        integral_type bit = value_integral & 0xff;
        result_integral = (result_integral << 8) + bit;
        value_integral = value_integral >> 8;
    }
    return value_type(result_integral);
}

template<typename BlueprintFieldType>
typename BlueprintFieldType::value_type unpack(typename BlueprintFieldType::value_type value) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;
    integral_type value_integral = integral_type(value.data);
    integral_type result_integral = 0;
    integral_type power = 1;
    while (value_integral >= 1) {
        integral_type bit = value_integral & 1;
        result_integral = result_integral + bit * power;
        value_integral = value_integral >> 3;
        power = power << 1;
    }
    return value_type(result_integral);
}

template<typename BlueprintFieldType>
std::vector<typename BlueprintFieldType::value_type>
    padding_function(std::vector<typename BlueprintFieldType::value_type> message, std::size_t num_bits) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;

    std::vector<value_type> result;
    std::size_t shift = 64 * message.size() - num_bits;

    if (shift > 0) {
        integral_type relay_value = integral_type(message[0].data);
        for (int i = 1; i < message.size(); ++i) {
            integral_type mask = (integral_type(1) << (64 - shift)) - 1;
            integral_type left_part = integral_type(message[i].data >> (64 - shift));
            integral_type right_part = integral_type(message[i].data) & mask;
            result.push_back(value_type((relay_value << shift) + left_part));
            relay_value = right_part;
        }
        relay_value <<= shift;
        relay_value += integral_type(1) << (shift - 8);
        result.push_back(value_type(relay_value));
    } else {
        for (int i = 0; i < message.size(); ++i) {
            result.push_back(message[i]);
        }
        result.push_back(value_type(integral_type(1) << 56));
    }

    while (result.size() % 17 != 0) {
        result.push_back(value_type(0));
    }

    for (int i = 0; i < result.size(); i++) {
        result[i] = to_le_bytes<BlueprintFieldType>(result[i]);
    }

    return result;
}

template<typename BlueprintFieldType, bool xor_with_mes, bool last_round_call>
std::array<typename BlueprintFieldType::value_type, 25>
    sparse_round_function(std::array<typename BlueprintFieldType::value_type, 25> inner_state,
                          std::array<typename BlueprintFieldType::value_type, 17>
                              padded_message_chunk,
                          typename BlueprintFieldType::value_type RC) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;

    std::array<std::array<integral_type, 5>, 5> inner_state_integral;
    std::array<integral_type, 17> padded_message_chunk_integral;
    integral_type RC_integral = integral_type(RC.data);
    for (int x = 0; x < 5; ++x) {
        for (int y = 0; y < 5; ++y) {
            inner_state_integral[x][y] = integral_type(inner_state[x + 5 * y].data);
        }
    }
    for (int i = 0; i < 17; ++i) {
        padded_message_chunk_integral[i] = integral_type(padded_message_chunk[i].data);
    }

    auto rot = [](integral_type x, const int s) {
        return ((x << (3 * s)) | (x >> (192 - 3 * s))) & ((integral_type(1) << 192) - 1);
    };

    if (xor_with_mes) {
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                if (last_round_call && (x + 5 * y == 16)) {
                    continue;
                }
                if (x + 5 * y < 17) {
                    inner_state_integral[x][y] = inner_state_integral[x][y] ^ padded_message_chunk_integral[x + 5 * y];
                }
            }
        }
        if (last_round_call) {
            value_type last_round_const = to_sparse<BlueprintFieldType>(value_type(0x8000000000000000));
            integral_type last_round_const_integral = integral_type(last_round_const.data);
            inner_state_integral[1][3] =
                inner_state_integral[1][3] ^ padded_message_chunk_integral[16] ^ last_round_const_integral;
        }
    }

    // theta
    std::array<integral_type, 5> C;
    for (int x = 0; x < 5; ++x) {
        for (int y = 0; y < 5; ++y) {
            C[x] ^= inner_state_integral[x][y];
        }
    }
    std::array<integral_type, 5> D;
    for (int x = 0; x < 5; ++x) {
        D[x] = C[(x + 4) % 5] ^ rot(C[(x + 1) % 5], 1);
    }
    for (int x = 0; x < 5; ++x) {
        for (int y = 0; y < 5; ++y) {
            inner_state_integral[x][y] ^= D[x];
        }
    }

    // rho and pi
    std::array<std::array<integral_type, 5>, 5> B;
    for (int x = 0; x < 5; ++x) {
        for (int y = 0; y < 5; ++y) {
            B[y][(2 * x + 3 * y) % 5] = rot(inner_state_integral[x][y], r[x][y]);
        }
    }

    // chi
    for (int x = 0; x < 5; ++x) {
        for (int y = 0; y < 5; ++y) {
            inner_state_integral[x][y] = B[x][y] ^ ((~B[(x + 1) % 5][y]) & B[(x + 2) % 5][y]);
        }
    }

    // iota
    inner_state_integral[0][0] = inner_state_integral[0][0] ^ RC_integral;
    for (int x = 0; x < 5; ++x) {
        for (int y = 0; y < 5; ++y) {
            inner_state[x + 5 * y] = value_type(inner_state_integral[x][y]);
        }
    }
    return inner_state;
}

template<typename BlueprintFieldType>
std::vector<typename BlueprintFieldType::value_type>
    bytes_to_bit_chunks64(std::string bytes) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;
    
    std::vector<int> bits;
    std::cout << "bits:\n";
    for (auto byte : bytes) {
        integral_type mask = integral_type(1) << 7;
        for (int i = 0; i < 8; ++i) {
            bits.push_back(int(byte & mask));
            mask >>= 1;
            std::cout << bits.back();
        }
    }
    std::cout << std::endl;
    std::size_t start_idx = 0;
    while (start_idx + 1 < bits.size() && bits[start_idx] == 0) {
        ++start_idx;
    }
    std::size_t end_idx = std::min(start_idx + 64, bits.size());

    std::vector<value_type> result;
    while (end_idx <= bits.size()) {
        integral_type value = 0;
        for (std::size_t i = start_idx; i < end_idx; ++i) {
            value = (value << 1) + bits[i];
        }
        result.push_back(value_type(value));
        start_idx = end_idx;
        end_idx = std::min(start_idx + 64, bits.size());
    }

    return result;
}

template<typename Container, typename BlueprintFieldType>
std::string bit_chunks64_to_bytes(Container bit_chunks) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;

    std::string result;
    for (value_type chunk : bit_chunks) {
        integral_type value = integral_type(chunk.data);
        integral_type mask = integral_type(7) << 7;
        for (int i = 0; i < 8; ++i) {
            result.push_back(std::string(value & mask));
            mask >>= 3;
        }
    }
    return result;
}

template<typename BlueprintFieldType>
std::vector<typename BlueprintFieldType::value_type>
    keccak_hash_call(std::vector<typename BlueprintFieldType::value_type> message, std::size_t num_bits) {
    using value_type = typename BlueprintFieldType::value_type;

    hashes::keccak_1600<224>::digest_type result = hash<hashes::keccak_1600<256>>(message.begin(), message.end());
    std::string result_str = std::to_string(result).data();
    std::cout << "Keccak hash call result string:\n" << result_str << std::endl;

    auto result_chunks = bytes_to_bit_chunks64<BlueprintFieldType>(result_str);
    std::cout << "Keccak hash call result chunks:\n";
    for (auto chunk : result_chunks) {
        std::cout << chunk.data << std::endl;
    }
    return result_chunks;
}

template<typename BlueprintFieldType>
std::array<typename BlueprintFieldType::value_type, 4>
    keccak_function(std::vector<typename BlueprintFieldType::value_type> message, std::size_t num_bits) {

    using value_type = typename BlueprintFieldType::value_type;

    std::array<typename BlueprintFieldType::value_type, 4> hash;
    std::vector<typename BlueprintFieldType::value_type> padded_message =
        padding_function<BlueprintFieldType>(message, num_bits);
    std::array<typename BlueprintFieldType::value_type, 25> inner_state;
    const typename BlueprintFieldType::value_type round_constant[24] = {value_type(1),
                                                                        value_type(0x8082),
                                                                        value_type(0x800000000000808a),
                                                                        value_type(0x8000000080008000),
                                                                        value_type(0x808b),
                                                                        value_type(0x80000001),
                                                                        value_type(0x8000000080008081),
                                                                        value_type(0x8000000000008009),
                                                                        value_type(0x8a),
                                                                        value_type(0x88),
                                                                        value_type(0x80008009),
                                                                        value_type(0x8000000a),
                                                                        value_type(0x8000808b),
                                                                        value_type(0x800000000000008b),
                                                                        value_type(0x8000000000008089),
                                                                        value_type(0x8000000000008003),
                                                                        value_type(0x8000000000008002),
                                                                        value_type(0x8000000000000080),
                                                                        value_type(0x800a),
                                                                        value_type(0x800000008000000a),
                                                                        value_type(0x8000000080008081),
                                                                        value_type(0x8000000000008080),
                                                                        value_type(0x80000001),
                                                                        value_type(0x8000000080008008)};

    std::size_t i = 0, j;
    std::array<typename BlueprintFieldType::value_type, 17> padded_message_chunk;
    // Absorbing
    std::size_t p17 = padded_message.size() / 17;
    for (i = 0; i < p17; i++) {
        for (j = 0; j < 17; j++)
            padded_message_chunk[j] = to_sparse<BlueprintFieldType>(padded_message[17 * i + j]);
        for (j = 0; j < 24; j++) {
            auto rc = to_sparse<BlueprintFieldType>(round_constant[j]);
            if (j == 0 && i == p17 - 1) {
                inner_state =
                    sparse_round_function<BlueprintFieldType, true, true>(inner_state, padded_message_chunk, rc);
            } else if (j == 0) {
                inner_state =
                    sparse_round_function<BlueprintFieldType, true, false>(inner_state, padded_message_chunk, rc);
            } else
                inner_state =
                    sparse_round_function<BlueprintFieldType, false, false>(inner_state, padded_message_chunk, rc);
        }
    }

    for (i = 0; i < 4; i++) {
        hash[i] = unpack<BlueprintFieldType>(inner_state[i]);
    }

    std::cout << "Testing bits to bytes: " << bit_chunks64_to_bytes<std::array<value_type, 4>, BlueprintFieldType>(hash) << std::endl;
    //check that computation is the same as keccak_1600_compressor
    // auto check_result = keccak_hash_call<BlueprintFieldType>(message, num_bits);
    for (int i = 0; i < 4; ++i) {
        std::cout << "hash:\n"
                  << hash[i].data << "\n";
    }

    return hash;
}

template<typename BlueprintFieldType, std::size_t WitnessesAmount>
auto test_keccak_inner(std::vector<typename BlueprintFieldType::value_type> message,
                       std::array<typename BlueprintFieldType::value_type, 4> expected_result, std::size_t num_blocks,
                       std::size_t num_bits, bool range_check_input, std::size_t limit_permutation_column) {
    constexpr std::size_t PublicInputColumns = 1;
    constexpr std::size_t ConstantColumns = 3;
    constexpr std::size_t SelectorColumns = 40;
    nil::crypto3::zk::snark::plonk_table_description<BlueprintFieldType> desc(WitnessesAmount, PublicInputColumns,
                                                                              ConstantColumns, SelectorColumns);
    using ArithmetizationType = nil::crypto3::zk::snark::plonk_constraint_system<BlueprintFieldType>;
    using AssignmentType = nil::blueprint::assignment<ArithmetizationType>;
    using hash_type = nil::crypto3::hashes::keccak_1600<256>;
    constexpr std::size_t Lambda = 1;

    using component_type = nil::blueprint::components::keccak<ArithmetizationType>;
    using var = nil::crypto3::zk::snark::plonk_variable<typename BlueprintFieldType::value_type>;

    std::vector<typename BlueprintFieldType::value_type> public_input;
    // std::cout << "message:\n";
    for (int i = 0; i < num_blocks; ++i) {
        public_input.push_back(message[i]);
        // std::cout << message[i].data << std::endl;
    }

    std::vector<var> message_vars;
    for (int i = 0; i < num_blocks; ++i) {
        message_vars.push_back(var(0, i, false, var::column_type::public_input));
    }
    typename component_type::input_type instance_input = {message_vars};

    auto result_check = [expected_result](AssignmentType &assignment, typename component_type::result_type &real_res) {
        assert(expected_result.size() == real_res.final_inner_state.size());
        for (int i = 0; i < expected_result.size(); ++i) {
            std::cout << "res:\n"
                      << expected_result[i].data << "\n"
                      << var_value(assignment, real_res.final_inner_state[i]).data << std::endl;
            assert(expected_result[i] == var_value(assignment, real_res.final_inner_state[i]));
        }
    };

    if (!(WitnessesAmount == 15 || WitnessesAmount == 9)) {
        BOOST_ASSERT_MSG(false, "Please add support for WitnessesAmount that you passed here!");
    }
    std::array<std::uint32_t, WitnessesAmount> witnesses;
    for (std::uint32_t i = 0; i < WitnessesAmount; i++) {
        witnesses[i] = i;
    }
    component_type component_instance =
        component_type(witnesses, std::array<std::uint32_t, 1> {0}, std::array<std::uint32_t, 1> {0}, num_blocks,
                       num_bits, range_check_input, limit_permutation_column);

    nil::crypto3::test_component<component_type, BlueprintFieldType, hash_type, Lambda>(
        component_instance, desc, public_input, result_check, instance_input,
        nil::blueprint::connectedness_check_type::type::STRONG,
        num_blocks, num_bits, range_check_input, limit_permutation_column);
}

// works
template<typename BlueprintFieldType, std::size_t WitnessesAmount, std::size_t LookupRows, std::size_t LookupColumns>
void test_keccak_0(std::size_t num_bytes = 1) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;

    std::vector<value_type> message;
    std::size_t num_blocks = (num_bytes + 7) / 8;
    std::size_t num_bits = num_bytes * 8;

    message.resize(num_blocks);
    for (std::size_t i = 0; i < num_blocks; i++) {
        message[i] = value_type(0);
    }
    const bool range_check_input = true;
    const std::size_t limit_permutation_column = 7;

    std::array<value_type, 4> expected_result = keccak_function<BlueprintFieldType>(message, num_bits);

    test_keccak_inner<BlueprintFieldType, WitnessesAmount>(message, expected_result, num_blocks, num_bits,
                                                           range_check_input, limit_permutation_column);
}

template<typename BlueprintFieldType, std::size_t WitnessesAmount, std::size_t LookupRows, std::size_t LookupColumns>
void test_keccak_hello_world() {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;

    std::vector<value_type> message = {value_type(0x68656c), value_type(0x6c6f20776f726c64)};
    const std::size_t num_blocks = 2;
    const std::size_t num_bits = 88;
    const bool range_check_input = false;
    const std::size_t limit_permutation_column = 7;

    std::array<value_type, 4> expected_result = keccak_function<BlueprintFieldType>(message, num_bits);

    test_keccak_inner<BlueprintFieldType, WitnessesAmount>(message, expected_result, num_blocks, num_bits,
                                                           range_check_input, limit_permutation_column);
}

template<typename BlueprintFieldType, std::size_t WitnessesAmount, std::size_t LookupRows, std::size_t LookupColumns>
void test_keccak_random(std::size_t message_size = 1, bool random_mask_zero = true, bool range_check_input = true,
                        std::size_t limit_permutation_column = 7) {
    using value_type = typename BlueprintFieldType::value_type;
    using integral_type = typename BlueprintFieldType::integral_type;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    integral_type mask = (integral_type(1) << 64) - 1;
    std::size_t power_for_mask = 64;
    if (random_mask_zero) {
        power_for_mask = dis(gen) % 63 + 1;
    }
    integral_type mask_zero = (integral_type(1) << power_for_mask) - 1;
    ;
    value_type message_zero =
        value_type((integral_type(dis(gen)) & mask_zero) | (integral_type(1) << (power_for_mask - 1)));
    std::vector<value_type> message;
    message.push_back(message_zero);
    for (std::size_t i = 1; i < message_size; i++) {
        message.push_back(value_type(integral_type(dis(gen)) & mask));
    }
    assert(message_size == message.size());
    std::size_t num_bits = 64 * (message_size - 1) + number_bits<BlueprintFieldType>(message[0]);
    std::size_t num_blocks = message_size;

    std::vector<value_type> expected_result = keccak_function<BlueprintFieldType>(message, num_bits);

    test_keccak_inner<BlueprintFieldType, WitnessesAmount>(message, expected_result, num_blocks, num_bits,
                                                           range_check_input, limit_permutation_column);
}


BOOST_AUTO_TEST_SUITE(blueprint_plonk_test_suite)

BOOST_AUTO_TEST_CASE(blueprint_plonk_hashes_keccak_pallas) {
    using field_type = nil::crypto3::algebra::curves::pallas::base_field_type;

    test_keccak_0<field_type, 9, 65536, 10>(136);
    test_keccak_0<field_type, 9, 65536, 10>(1);
    test_keccak_hello_world<field_type, 9, 65536, 10>();
}

BOOST_AUTO_TEST_CASE(blueprint_plonk_hashes_keccak_round_pallas_15) {
    using field_type = nil::crypto3::algebra::curves::vesta::scalar_field_type;

    test_keccak_0<field_type, 9, 65536, 10>(136);
    test_keccak_0<field_type, 9, 65536, 10>(1);
    test_keccak_hello_world<field_type, 9, 65536, 10>();
}

BOOST_AUTO_TEST_CASE(blueprint_plonk_hashes_keccak_bn254) {
    using field_type = nil::crypto3::algebra::curves::alt_bn128_254::base_field_type;

    test_keccak_0<field_type, 9, 65536, 10>(136);
    test_keccak_0<field_type, 9, 65536, 10>(1);
    test_keccak_hello_world<field_type, 9, 65536, 10>();
}

BOOST_AUTO_TEST_SUITE_END()