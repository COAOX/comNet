#include "stdafx.h"
#include "Server.h"

SOCKET Server::sListen = NULL, Server::sServer = NULL; //侦听套接字，连接套接字
Client Server::clients[MaxClient] = { 0 };
int Server::clientNum = 0;