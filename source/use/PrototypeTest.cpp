
#include "datatype.h"
#include "Test.h"
#include "ObjectHandle.h"
#include "Cpp11TemplateAlias.h"
#include <conio.h>
#include <stdio.h>

namespace TPLUsing{
template <typename T>
struct _M;

template <typename T>
using M = _M<T>;

template <typename T>
struct N;

template<typename T>
struct TPLUsing
{
    template<typename U>
    using M = _M<T>;
};

struct Atom {};

template <typename T>
struct AUse;

template <typename T>
struct AUse<N<T>>
{};

template <typename T>
struct AUse<M<T>>
{};

//template <typename T>
//struct AUse<TPLUsing<T>::template M<T>>
//{};

template <>
struct AUse< typename TPLUsing<Atom>::template M<Atom> >
{};

}

int main(int argc,char** argv)
{
    f();
    b<Y>();
    _getch();

    StartAsyncTest();
    /**
    for (int i = 0; i < 10; i++)
    {
        StartSPTest(50);
        _getch();
        ShowCount();
        _getch();
    }
    _getch();
    ShowCount();
    /**/
    _getch();

    StopAsyncNet();

    printf("press enter to exit program \r\n");
    _getch();
    printf("program exit \r\n");
    return 0;
}