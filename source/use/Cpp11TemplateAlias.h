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
template <typename T, template<typename> class allocator = std::allocator >
struct StlVectorTemplate
{
    template <typename _T>
    using TPL1 = std::vector< _T, allocator<_T> >;

    template <template<typename> class _allocator>
    using TPL2 = std::vector< T, _allocator<T> >;
};

template <typename T, template<typename> class allocator = std::allocator >
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

#endif