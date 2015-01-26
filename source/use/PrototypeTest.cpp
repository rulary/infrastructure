
#include "datatype.h"
#include "Test.h"
#include "ObjectHandle.h"
#include "Cpp11TemplateAlias.h"
#include <conio.h>
#include <stdio.h>

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

    printf("press enter to exit program \r\n");
    _getch();
    printf("program exit \r\n");
    return 0;
}