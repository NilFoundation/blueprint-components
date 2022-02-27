//---------------------------------------------------------------------------//
// Copyright (c) 2020-2021 Mikhail Komarov <nemo@nil.foundation>
// Copyright (c) 2020-2021 Nikita Kaskov <nbering@nil.foundation>
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

#ifndef CRYPTO3_ZK_BLUEPRINT_BLUEPRINT_PLONK_HPP
#define CRYPTO3_ZK_BLUEPRINT_BLUEPRINT_PLONK_HPP

#include <nil/crypto3/zk/snark/relations/plonk/plonk.hpp>
#include <nil/crypto3/zk/snark/relations/plonk/constraint.hpp>
#include <nil/crypto3/zk/snark/relations/plonk/copy_constraint.hpp>
#include <nil/crypto3/zk/snark/relations/plonk/lookup_constraint.hpp>

namespace nil {
    namespace crypto3 {
        namespace zk {
            namespace components {

                template<typename ArithmetizationType, std::size_t... BlueprintParams>
                class blueprint;

                template<typename BlueprintFieldType>
                class blueprint<snark::plonk_constraint_system<BlueprintFieldType>> :
                    public snark::plonk_constraint_system<BlueprintFieldType> {

                    typedef snark::plonk_constraint_system<BlueprintFieldType> ArithmetizationType;
                public:
                    
                    blueprint() : ArithmetizationType(){
                        this->_rows_amount = 0;
                    }

                    std::size_t allocate_rows(std::size_t required_amount = 1) {
                        std::size_t result = this->_rows_amount;
                        this->_rows_amount += required_amount;
                        return result;
                    }

                    std::size_t allocate_row() {
                        return allocate_rows(1);
                    }

                    void add_gate(std::size_t selector_index, const snark::plonk_constraint<BlueprintFieldType> &constraint) {
                        this->_gates.emplace_back(selector_index, constraint);
                    }

                    void add_gate(std::size_t selector_index,
                                  const std::initializer_list<snark::plonk_constraint<BlueprintFieldType>> &constraints) {
                        this->_gates.emplace_back(selector_index, constraints);
                    }

                    void add_copy_constraint(const snark::plonk_copy_constraint<BlueprintFieldType> &copy_constraint) {
                        this->_copy_constraints.emplace_back(copy_constraint);
                    }

                    void add_lookup_constraint(const snark::plonk_lookup_constraint<BlueprintFieldType> &copy_constraint) {
                        this->_lookup_constraints.emplace_back({});
                    }
                };
            }    // namespace components
        }        // namespace zk
    }            // namespace crypto3
}    // namespace nil
#endif    // CRYPTO3_ZK_BLUEPRINT_BLUEPRINT_PLONK_HPP
