// Server.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "msg.h"
#include "Server.h"
#include <iostream>
#include <thread>

int main()
{
	Server server;
	server.run();
    return 0;
}

