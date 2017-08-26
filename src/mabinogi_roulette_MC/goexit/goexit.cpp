﻿/*! \file goexit.h
    \brief プログラムを終了するときの関数の実装

    Copyright ©  2015 @dc1394 All Rights Reserved.
    This software is released under the BSD 2-Clause License.
*/

#include "goexit.h"
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
    #include <conio.h>                  // for _getch
#endif

namespace goexit {
    void goexit()
    {
#if defined(_WIN32) || defined(_WIN64)
        std::cout << "終了するには何かキーを押してください..." << std::endl;
        ::_getch();
#endif
    }
}
