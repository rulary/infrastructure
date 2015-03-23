#ifndef _CPP11TEMPLATE_ALIAS_H_
#define _CPP11TEMPLATE_ALIAS_H_

#include <vector>
#include <list>

/* alias of std::allocator; just for distinguishments */
template <typename _T>
using allocatorA = std::allocator< _T >;

template <typename _T>
using allocatorB = std::allocator< _T >;

// dummy type

struct DummyType;

// template alias for stl containers
template <typename T = DummyType, template<typename> class allocator = std::allocator >
struct StlVectorTemplate
{
    template <typename _T>
    using TPL1 = std::vector< _T, allocator<_T> >;

    template <template<typename> class _allocator>
    using TPL2 = std::vector< T, _allocator<T> >;
};

template <typename T = DummyType, template<typename> class allocator = std::allocator >
struct StlListTemplate
{
    template <typename _T>
    using TPL1 = std::list< _T, allocator<_T> >;

    template <template<typename> class _allocator>
    using TPL2 = std::list< T, _allocator<T> >;
};

/* enumerable abstract */
template<template<typename> class C, typename T>
struct Enumerable
{
    typedef C<T> Container;

    Enumerable() {}

    Enumerable(const Enumerable& rv) { data = rv.data; }

    Enumerable(Enumerable&& rv) { data = std::move(rv.data); }

    Enumerable& operator=(const Enumerable& other)
    {
        if (&other != this)
        {
            data = other.data;
        }
        return *this;
    }

    Enumerable& operator=(Enumerable&& other)
    {
        data = std::move(other.data);
        return *this;
    }

    void reserve(typename Container::size_type n) { data.reserve(n); }

    void push(const typename Container::value_type& value) { data.push_back(value); }

    /* for temp object*/
    void push(typename Container::value_type&& value) { data.push_back(std::move(value)); }

private:
    C<T> data;
};

template<typename T>
struct Enumerable<StlListTemplate<>::TPL1, T>
{

};

template <typename T, typename U>
struct A {};

struct X;
struct Y;

template <typename T = X, typename U = Y>
struct C
{
    template <typename T_>
    using TPLUsingU = A<T_, U>;

    template <typename U_>
    using TPLUsingT = A<T, U_>;
};

void f()
{
    C <>::TPLUsingU<Y> a;
    a;
}

template <typename T>
void b()
{
    C <T>::template TPLUsingT<Y> b;   // OK
    b;
}

template <template <typename> class TPL>
struct F;

template <>
struct F <C <>::TPLUsingU > {};  // OK

template <>
struct F <C <>::TPLUsingT > {};  // OK


template <>
struct F <C <Y>::TPLUsingT > {};  // OK

//template <typename T>
//struct F <C <typename T>::TPLUsingT > {};   // error C3200: “C<T,Y>::TPLUsingT”: 模板参数“TPL”的模板参数无效，应输入类模板

template <typename T_>
using TPLAliasA = C <T_>::template TPLUsingT;

template <typename T_>
using TPLAliasB = C <X, T_>::template TPLUsingT;

//using TPLAliasC = C <X, Y>::TPLUsingU; // error C2955: “C<X,Y>::TPLUsingU”: 使用 别名 模板 需要 模板 参数列表

template <>
struct F < TPLAliasA > {};

#endif