
#include "datatype.h"
#include "Test.h"

#include <conio.h>

int main(int argc,char** argv)
{
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

    _getch();
    
    return 0;
}