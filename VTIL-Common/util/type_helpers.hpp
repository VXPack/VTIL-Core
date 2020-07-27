// Copyright (c) 2020 Can Boluk and contributors of the VTIL Project   
// All rights reserved.   
//    
// Redistribution and use in source and binary forms, with or without   
// modification, are permitted provided that the following conditions are met: 
//    
// 1. Redistributions of source code must retain the above copyright notice,   
//    this list of conditions and the following disclaimer.   
// 2. Redistributions in binary form must reproduce the above copyright   
//    notice, this list of conditions and the following disclaimer in the   
//    documentation and/or other materials provided with the distribution.   
// 3. Neither the name of VTIL Project nor the names of its contributors
//    may be used to endorse or promote products derived from this software 
//    without specific prior written permission.   
//    
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE   
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE  
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE   
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR   
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF   
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN   
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)   
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE  
// POSSIBILITY OF SUCH DAMAGE.        
//
#pragma once
#include <type_traits>
#include <array>
#include "intrinsics.hpp"

namespace vtil
{
	// Type tag.
	//
	template<typename T>
	struct type_tag { using type = T; };
	
	// Check for specialization.
	//
	namespace impl
	{
		template <template<typename...> typename Tmp, typename>
		static constexpr bool is_specialization_v = false;
		template <template<typename...> typename Tmp, typename... Tx>
		static constexpr bool is_specialization_v<Tmp, Tmp<Tx...>> = true;
	};
	template <template<typename...> typename Tmp, typename T>
	static constexpr bool is_specialization_v = impl::is_specialization_v<Tmp, std::remove_cvref_t<T>>;

	// Checks if the given lambda can be evaluated in compile time.
	//
	template<typename F, std::enable_if_t<F{}(), int> = 0>
	static constexpr bool is_constexpr( F )   { return true; }
	static constexpr bool is_constexpr( ... ) { return false; }

	// Commonly used concepts.
	//
	template<typename T>
	concept Iterable = requires( T v ) { std::begin( v ); std::end( v ); };
	template<typename T>
	concept Integral = std::is_integral_v<T>;
	template<typename T, typename... Args>
	concept ConstructableWith = requires { T( std::declval<Args>()... ); };

	// Constructs a static constant given the type and parameters, returns a reference to it.
	//
	namespace impl
	{
		template<typename T, auto... params>
		struct static_allocator { inline static const T value = { params... }; };
	};
	template<typename T, auto... params>
	static constexpr const T& make_static() noexcept { return impl::static_allocator<T, params...>::value; }

	// Default constructs the type and returns a reference to the static allocator. 
	// This useful for many cases, like:
	//  1) Function with return value of (const T&) that returns an external reference or if not applicable, a default value.
	//  2) Using non-constexpr types in constexpr structures by deferring the construction.
	//
	template<typename T> static constexpr const T& make_default() noexcept { return make_static<T>(); }

	// Special type that collapses to a constant reference to the default constructed value of the type.
	//
	static constexpr struct
	{
		template<typename T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
		constexpr operator const T&() const noexcept { return make_default<T>(); }

		template<typename T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
		constexpr operator T() const noexcept { static_assert( sizeof( T ) == -1, "Static default immediately decays, unnecessary use." ); unreachable(); }
	} static_default;

	// Implementation of type-helpers for the functions below.
	//
	namespace impl
	{
		template<typename T, typename = void> struct make_const   {};
		template<typename T> struct make_const<T&&, void>   { using type = std::add_const_t<T>&&;   };
		template<typename T> struct make_const<T&, void>    { using type = std::add_const_t<T>&;    };
		template<typename T> struct make_const<T*, void>    { using type = std::add_const_t<T>*;    };

		template<typename T, typename = void> struct make_mutable {};
		template<typename T> struct make_mutable<T&&, void> { using type = std::remove_const_t<T>&&; };
		template<typename T> struct make_mutable<T&, void>  { using type = std::remove_const_t<T>&;  };
		template<typename T> struct make_mutable<T*, void>  { using type = std::remove_const_t<T>*;  };

		template<typename T, typename = void> struct is_const_underlying : std::false_type {};
		template<typename T> struct is_const_underlying<const T&&, void> : std::true_type  {};
		template<typename T> struct is_const_underlying<const T&, void>  : std::true_type  {};
		template<typename T> struct is_const_underlying<const T*, void>  : std::true_type  {};

		template<typename B, typename T>
		struct carry_const
		{
			using type = std::conditional_t<
				is_const_underlying<B>::value,
				typename make_const<T>::type, 
				typename make_mutable<T>::type
			>;
		};
	};

	// Checks the const qualifiers of the underlying object.
	//
	template<typename T> static constexpr bool is_const_underlying_v = impl::is_const_underlying<T>::value;
	template<typename T> static constexpr bool is_mutable_underlying_v = !impl::is_const_underlying<T>::value;

	// Converts from a non-const qualified ref/ptr to a const-qualified ref/ptr.
	//
	template<typename T> using make_const_t = typename impl::make_const<T>::type;
	template<typename T> static constexpr make_const_t<T> make_const( T&& x ) noexcept { return ( make_const_t<T> ) x; }

	// Converts from a const qualified ref/ptr to a non-const-qualified ref/ptr.
	// - Make sure the use is documented, this is very hacky behaviour!
	//
	template<typename T> using make_mutable_t = typename impl::make_mutable<T>::type;
	template<typename T> static constexpr make_mutable_t<T> make_mutable( T&& x ) noexcept { return ( make_mutable_t<T> ) x; }

	// Converts from any ref/ptr to a const/mutable one based on the condition given.
	//
	template<bool C, typename T> using make_const_if_t = std::conditional_t<C, make_const_t<T>, make_mutable_t<T>>;
	template<bool C, typename T> using make_mutable_if_t = std::conditional_t<C, make_mutable_t<T>, make_const_t<T>>;
	template<bool C, typename T> static constexpr make_const_if_t<C, T> make_const_if( T&& value ) noexcept { return ( make_const_if_t<C, T> ) value; }
	template<bool C, typename T> static constexpr make_mutable_if_t<C, T> make_mutable_if( T&& value ) noexcept { return ( make_mutable_if_t<C, T> ) value; }

	// Carries constant qualifiers of first type into second.
	//
	template<typename B, typename T> using carry_const_t = make_const_if_t<is_const_underlying_v<B>, T>;
	template<typename B, typename T> static constexpr carry_const_t<B, T> carry_const( B&& base, T&& value ) noexcept { return ( carry_const_t<B, T> ) value; }

	// Creates a copy of the given value.
	//
	template<typename T> __forceinline static constexpr T make_copy( const T& x ) { return x; }

	// Makes a null pointer to type.
	//
	template<typename T> static constexpr T* make_null() noexcept { return ( T* ) nullptr; }

	// Returns the offset of given member reference.
	//
	template<typename V, typename C> 
	static constexpr int32_t make_offset( V C::* ref ) noexcept { return ( int32_t ) ( uint64_t ) &( make_null<C>()->*ref ); }

	// Gets the type at the given offset.
	//
	template<typename T = void, typename B = void>
	static auto* ptr_at( B* base, int32_t off ) noexcept { return carry_const( base, ( T* ) ( ( ( uint64_t ) base ) + off ) ); }
	template<typename T, typename B>
	static auto& ref_at( B* base, int32_t off ) noexcept { return *ptr_at<T>(base, off); }

	// Member reference helper.
	//
	template<typename C, typename M>
	using member_reference_t = M C::*;

	// Implement helpers for basic series creation.
	//
	namespace impl
	{
		template<typename Ti, typename T, Ti... I>
		static constexpr auto make_expanded_series( T&& f, std::integer_sequence<Ti, I...> )
		{
			return std::array{ f( I )... };
		}

		template<typename Ti, template<auto> typename Tr, typename T, Ti... I>
		static constexpr auto make_visitor_series( T&& f, std::integer_sequence<Ti, I...> )
		{
			return std::array{ f( type_tag<Tr<I>>{} )... };
		}
	};
	template<auto N, typename T>
	static constexpr auto make_expanded_series( T&& f )
	{
		return impl::make_expanded_series<decltype( N )>( std::forward<T>( f ), std::make_integer_sequence<decltype( N ), N>{} );
	}
	template<auto N, template<auto> typename Tr, typename T>
	static constexpr auto make_visitor_series( T&& f )
	{
		return impl::make_visitor_series<decltype( N ), Tr, T>( std::forward<T>( f ), std::make_integer_sequence<decltype( N ), N>{} );
	}
};