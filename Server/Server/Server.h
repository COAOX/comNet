#pragma once
#include "msg.h"
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <map>
#include <queue>
#include <cstdio>
#include<cstring>
#include <ctime>
#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT	6666 //侦听端口
#define MaxClient 10
#define ServerName "Server1"

struct Client {
	char ip[20];
	int port;
};

struct Message {
	int from, to;
	instruct ins;
	bool processed, sent;
};

class Server
{
public:
	Server() {

	}

	~Server() {
		closesocket(sListen);//关闭套接字
		closesocket(sServer);
		WSACleanup();
	}

	int init() {
		WORD wVersionRequested;
		WSADATA wsaData;
		wVersionRequested = MAKEWORD(2, 2);//希望使用的WinSock DLL的版本
		int ret = WSAStartup(wVersionRequested, &wsaData);
		if (ret != 0) {
			printf("WSAStartup() failed!\n");
			return -1;
		}

		//确认WinSock DLL支持版本2.2：
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
			WSACleanup();
			printf("Invalid Winsock version!\n");
			return -1;
		}

		//创建socket，使用TCP协议：
		sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sListen == INVALID_SOCKET) {
			printf("socket() failed!\n");
			return -1;
		}

		//构建本地地址信息：
		struct sockaddr_in saServer;//地址信息
		saServer.sin_family = AF_INET;//地址家族
		saServer.sin_port = htons(SERVER_PORT);//注意转化为网络字节序
		saServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//使用INADDR_ANY指示任意地址

		//绑定：
		ret = bind(sListen, (struct sockaddr *)&saServer, sizeof(saServer));
		if (ret == SOCKET_ERROR) {
			printf("bind() failed! code:%d\n", WSAGetLastError());
			return -1;
		}

		//设置为非阻塞
		unsigned long ul = 1;
		ret = ioctlsocket(sListen, FIONBIO, (unsigned long *)&ul);//设置成非阻塞模式。
		if (ret == SOCKET_ERROR) {//设置失败。
			printf("ioctlsocket() failed! code:%d\n", WSAGetLastError());
			return -1;
		}

		//侦听连接请求：
		ret = listen(sListen, 5);
		if (ret == SOCKET_ERROR) {
			printf("listen() failed! code:%d\n", WSAGetLastError());
			return -1;
		}

		printf("The server has been initialized.\n");
		return 0;
	}

	void run() {
		while (true) {
			//printf("Waiting for client connecting!\n");
			//阻塞等待接受客户端连接：
			struct sockaddr_in saClient;
			int length = sizeof(saClient);
			sServer = accept(sListen, (struct sockaddr *)&saClient, &length);
			if (sServer == INVALID_SOCKET) {
				if (WSAGetLastError() == WSAEWOULDBLOCK)
					continue;
				else {
					printf("accept() failed! code:%d\n", WSAGetLastError());
					return;
				}
			}
			else
				openClient(saClient);
		}
	}

	void openClient(struct sockaddr_in saClient) {
		Client client;
		strcpy(client.ip, inet_ntoa(saClient.sin_addr));
		client.port = ntohs(saClient.sin_port);
		int i;
		for (i = 0; i < MaxClient; i++) {
			if (mapClient.count(i) == 0) {
				mapClient[i] = client;
				mapThread[i] = std::thread(runClient, sServer, saClient, i, this);
				mapThread[i].detach();
				break;
			}
		}
		printf("Client %d (%s:%d) connected.\n", i, inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port));
	}

	static void runClient(SOCKET arg, SOCKADDR_IN client_addr, int pos, Server * pthis){	
		int sen, ret;
		while (true) {
			//如果该线程被其他线程要求发送指令数据包
			if (pthis->queueMessege.size() > 0) { 
				if (pthis->queueMessege.front().to == pos) {
					Message &msg = pthis->queueMessege.front();
					sen = send(arg, (char*)&(msg.ins), sizeof(msg.ins), 0);
					msg.processed = true;
					if (sen == SOCKET_ERROR)
						printf("send() failed! code:%d\n", WSAGetLastError());
					else if (sen == 0) { //已经断开连接
						std::thread t = std::thread(closeClient, pos, client_addr, pthis);
						t.detach();
						return;
					}
					else {
						msg.sent = true;
					}
				}
				//如果该线程要求发送的指令数据包已经被处理
				else if (pthis->queueMessege.front().from == pos&& pthis->queueMessege.front().processed == true) { 
					respond res;
					res.type = SEND_RES;
					if (pthis->queueMessege.front().sent) //成功送达
						strcpy(res.data, "SUCCESS: The message has been sent!");
					else
						strcpy(res.data, "ERROR: The number of the client is not in the list.");
					pthis->queueMessege.pop(); 
					sen = send(arg, (char*)&res, sizeof(res), 0);
					if (sen == SOCKET_ERROR)
						printf("send() failed! code:%d\n", WSAGetLastError());
					else if (sen == 0) { //已经断开连接
						std::thread t = std::thread(closeClient, pos, client_addr, pthis);
						t.detach();
						return;
					}
				}
			}

			request req;
			ret = recv(arg, (char*)&req, sizeof(request), 0);
			if (ret == SOCKET_ERROR) {
				if (WSAGetLastError() == WSAEWOULDBLOCK)
					;
				else
					printf("recv() failed! code:%d\n", WSAGetLastError());
				continue;
			}
			else if (ret == 0) { //已经断开连接
				std::thread t= std::thread(closeClient, pos, client_addr,pthis);
				t.detach();
				return;
			}
			respond res;
			switch (req.type)
			{
			case TIME:
				res.type = TIME;
				time_t t;    
				time(&t);  
				strcpy(res.data, ctime(&t));
				sen = send(arg, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			case NAME:
				res.type = TIME;
				strcpy(res.data, ServerName);
				sen = send(arg, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			case LIST:
				res.type = LIST;
				char list[300];
				char* plist;
				plist=list;
				for (auto iter = pthis->mapClient.begin(); iter != pthis->mapClient.end(); iter++) {
					char clinfo[30] = { 0 };
					sprintf(clinfo, "%d/t%s/t%d", iter->first, iter->second.ip, iter->second.port);
					strcpy(plist, clinfo);
					plist += 30;
				}
				strcpy(res.data, list);
				sen = send(arg, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			case SEND:
				res.type = SEND_RES;
				if (pthis->mapClient.count(req.number)==0) {
					strcpy(res.data, "ERROR: The number of the client is not in the list.");
				}
				else {
					instruct ins;
					strcpy(ins.content, req.content);
					strcpy(ins.ip, pthis->mapClient[pos].ip);
					sprintf(ins.port, "%d", pthis->mapClient[pos].port);
					Message msg;
					msg.ins = ins;
					msg.from = pos;
					msg.to = req.number;
					msg.processed = msg.sent = false;
				}
				sen = send(arg, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			default:
				printf("ERROR: The request from the client %d can't be recognized!\n", pos);
				break;
			}
		}
	}

	static void closeClient(int number, struct sockaddr_in saClient, Server * pthis) {
		pthis->mapClient.erase(number);
		//TerminateThread(pthis->mapThread[number].native_handle(), 0);
		pthis->mapThread.erase(number);
		printf("Client %d (%s:%d) misconnected.\n",number, inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port));
	}

private:
	SOCKET sListen, sServer; //侦听套接字，连接套接字
	std::map <int, std::thread> mapThread;
	std::map <int, Client> mapClient;
	std::queue <Message> queueMessege;
};