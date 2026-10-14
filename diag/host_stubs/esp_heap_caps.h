#pragma once
#include <cstddef>
constexpr unsigned MALLOC_CAP_INTERNAL = 1U, MALLOC_CAP_8BIT = 2U;
std::size_t heap_caps_get_free_size(unsigned);
std::size_t heap_caps_get_largest_free_block(unsigned);
std::size_t heap_caps_get_minimum_free_size(unsigned);
