/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/


#ifndef _UINT33_T_H
#define _UINT33_T_H

#include <cassert>
#include <cstdint>
#include <limits>

struct uint33_t
{
	uint64_t value : 33;
	inline constexpr operator double() const { return static_cast<double>(value);}
	inline constexpr operator bool() const { return static_cast<bool>(value);}
	uint33_t& operator=(const uint64_t& i) { value = i; return *this;}
	constexpr static uint33_t max_value() {return {0x1ffffffff};};
	constexpr static uint33_t half_max() {return {uint33_t::max_value().value/2};}
};

inline constexpr uint33_t operator+(const uint33_t& l, const uint33_t& r) { return {l.value + r.value}; }
inline constexpr uint33_t operator-(const uint33_t& l, const uint33_t& r) { return {l.value - r.value}; }
inline constexpr bool operator<(const uint33_t& l, const uint33_t& r) { return l.value < r.value; }
inline constexpr bool operator>(const uint33_t& l, const uint33_t& r) { return r < l; }

inline constexpr bool operator==(const uint33_t& l, const uint33_t& r) { return l.value == r.value; }
inline constexpr bool operator!=(const uint33_t& l, const uint33_t& r) { return !(l == r);}

template<typename Integer>
inline constexpr bool operator==(const uint33_t& l, const Integer& r) { return l == uint33_t{static_cast<uint64_t>(r)}; }

template<typename Integer>
inline constexpr bool operator!=(const uint33_t& l, const Integer& r) { return !(l == r);}

template<typename Integer>
inline constexpr bool operator==(const Integer& l, const uint33_t& r) { return r == l; }

template<typename Integer>
inline constexpr bool operator!=(const Integer& l, const uint33_t& r) { return !(l == r);}

namespace test_variables
{
	constexpr uint33_t max_val{0x1ffffffff};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverflow"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
#endif
    //overflow on purpose to check the behaviour of the uint33_t in such scenario
	constexpr uint33_t from_max_u64{std::numeric_limits<uint64_t>::max()};
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

	constexpr uint33_t big_val{0x1fffffffc};
	constexpr uint33_t two{2};
	constexpr uint33_t three{3};
	constexpr uint33_t four{4};
	constexpr uint33_t seven{7};
	constexpr uint33_t zero{0};

	static_assert(four.value == 4, "value representation");
	static_assert(two.value == 2, "value representation");
	static_assert(zero.value == 0, "value representation");
	static_assert(max_val.value == 0x1ffffffff, "value representation");
	static_assert(from_max_u64.value == 0x1ffffffff, "value representation");

	static_assert((double)four == 4.0, "implicit double operator");
	static_assert((bool)four, "implicit bool operator - positive gives true");
	static_assert(!!four, "implicit bool operator - positive gives true");
	static_assert(!((bool)zero), "implicit bool operator - zero gives false");
	static_assert(!zero, "implicit bool operator - zero gives false");

	static_assert(three + four == seven, "addition");

	static_assert(max_val + four == three, "addition");
	static_assert(four + max_val == three, "addition");

	static_assert(seven - four == three, "substraction");
	static_assert(three - seven == (max_val - three), "substraction");
	static_assert(three - seven == (zero - four), "substraction");

	static_assert(three - big_val == seven, "substraction");


	static_assert(three == three, "equality comparison");
	static_assert(!(three == four), "equality comparison");
	static_assert(three != four, "equality comparison");
	static_assert(!(three != three), "equality comparison");
	static_assert(from_max_u64 == max_val, "equality comparison");


	static_assert(three == 3, "int equality comparison");
	static_assert(three != 4, "int equality comparison");
	static_assert(!(three == 4), "int equality comparison");

	static_assert(three < four, "lt comparison");
	static_assert(!(three < three), "lt comparison");
	static_assert(!(three < two), "lt comparison");

	static_assert(four > three, "gt comparison");
	static_assert(!(three > three), "gt comparison");
	static_assert(!(two > three), "gt comparison");


#ifndef NDEBUG
namespace
{
	//C++11 does not support complex contexpr expressions, hence moving to runtime test
	int test()
	{
		uint33_t x{3};
		x = 4;
		assert(x == four && "int assignment");
		return 42;
	}
	int test_ = test();
}
#endif
}

#endif