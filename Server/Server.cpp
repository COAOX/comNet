#include "stdafx.h"
#include "Server.h"

SOCKET Server::sListen = NULL, Server::sServer = NULL; //�����׽��֣������׽���
Client Server::clients[MaxClient] = { 0 };
int Server::clientNum = 0;