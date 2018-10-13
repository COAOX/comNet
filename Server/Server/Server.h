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
#define MaxClient 8
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

		//设置socket为非阻塞模式
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

		//初始化完成
		printf("The server has been initialized.\n"); 
		return 0;
	}

	void run() {
		while (true) {
			//接受连接请求
			struct sockaddr_in saClient;
			int length = sizeof(saClient);
			sServer = accept(sListen, (struct sockaddr *)&saClient, &length);
			if (sServer == INVALID_SOCKET) {
				if (WSAGetLastError() == WSAEWOULDBLOCK)
					continue;//资源暂时不可用，继续尝试接受连接
				else {//出现其他错误
					printf("accept() failed! code:%d\n", WSAGetLastError());
					return;
				}
			}
			else //若成功接受连接请求，则针对该客户端建立连接服务
				openService(saClient);
		}
	}

	void openService(struct sockaddr_in saClient) {
		Client client;
		strcpy(client.ip, inet_ntoa(saClient.sin_addr));
		client.port = ntohs(saClient.sin_port);

		//寻找未被占用的最小客户端编号，用这个编号建立服务
		int i;
		for (i = 0; i < MaxClient; i++) {
			if (mapClient.count(i) == 0) {
				mapClient[i] = client; //记录客户端信息
				mapThread[i] = std::thread(runService, sServer, saClient, i, this); //创建线程并记录信息
				mapThread[i].detach();
				break;
			}
		}
		if (i == MaxClient) //若客户端数量超过服务器容量，则报错
			printf("ERROR: The server can't deal with more the %d clients.", MaxClient);
		else //成功建立服务，则提示客户端已建立连接
			printf("Client %d (%s:%d) connected.\n", i, inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port));
	}

	static void runService(SOCKET arg, SOCKADDR_IN client_addr, int pos, Server * pthis){	
		int sen, ret;
		while (true) {
			//首先处理服务器内消息队列中的请求
			if (pthis->queueMessege.size() > 0) { 
				//如果该线程被其他线程要求发送指令数据包
				if (pthis->queueMessege.front().to == pos) {
					Message &msg = pthis->queueMessege.front();
					sen = send(arg, (char*)&(msg.ins), sizeof(msg.ins), 0); //向指定客户端发送信息
					msg.processed = true; //标记该请求已被处理
					if (sen == SOCKET_ERROR) 
						printf("send() failed! code:%d\n", WSAGetLastError());
					else if (sen == 0) { //已经断开连接，则关闭线程
						std::thread t = std::thread(closeService, pos, client_addr, pthis);
						t.detach();
						return;
					}
					else { //发送成功，标记请求信息为“成功发送”
						msg.sent = true;
					}
				}
				//如果该线程要求发送的指令数据包已经被处理
				else if (pthis->queueMessege.front().from == pos&& pthis->queueMessege.front().processed == true) { 
					respond res;
					res.type = SEND_RES;
					if (pthis->queueMessege.front().sent) //成功送达
						strcpy(res.data, "SUCCESS: The message has been sent!");
					else //发送失败
						strcpy(res.data, "ERROR: The client is not in the list.");
					pthis->queueMessege.pop(); //将该消息从消息队列中删除
					sen = send(arg, (char*)&res, sizeof(res), 0); //向客户端发送反馈信息
					if (sen == SOCKET_ERROR)
						printf("send() failed! code:%d\n", WSAGetLastError());
					else if (sen == 0) { //已经断开连接，则关闭线程
						std::thread t = std::thread(closeService, pos, client_addr, pthis);
						t.detach();
						return;
					}
				}
			}

			//其次处理客户端的请求
			request req;
			ret = recv(arg, (char*)&req, sizeof(request), 0); //接受客户端消息
			if (ret == SOCKET_ERROR) { 
				if (WSAGetLastError() == WSAEWOULDBLOCK)//资源暂时不可用，继续尝试接受消息
					continue;
				else if (WSAGetLastError() == WSAECONNRESET) { //若连接异常，则关闭线程
					std::thread t = std::thread(closeService, pos, client_addr, pthis);
					t.detach();
					return;
				}
				else
					printf("recv() failed! code:%d\n", WSAGetLastError());
				continue;
			}
			else if (ret == 0) { //已经断开连接，则关闭线程
				std::thread t = std::thread(closeService, pos, client_addr, pthis);
				t.detach();
				return;
			}
			respond res;
			switch (req.type)
			{
			case TIME: //请求类型为获取时间
				res.type = TIME;
				time_t t;    
				time(&t);  //获取系统时间
				strcpy(res.data, ctime(&t));
				sen = send(arg, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			case NAME: //请求类型为获取名字
				res.type = TIME;
				strcpy(res.data, ServerName);
				sen = send(arg, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			case LIST: //请求类型为获取客户端列表
				res.type = LIST;
				char list[300];
				char* plist;
				plist=list;
				//遍历客户端列表，将客户端信息存至字符串数组中
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
			case SEND: //请求类型为发送消息
				res.type = SEND_RES;
				//若不存在该客户端编号，则报错
				if (pthis->mapClient.count(req.number)==0) {
					strcpy(res.data, "ERROR: The number of the client is not in the list.");
				}
				else {
					//组装指示数据包
					instruct ins;
					strcpy(ins.content, req.content);
					strcpy(ins.ip, pthis->mapClient[pos].ip);
					sprintf(ins.port, "%d", pthis->mapClient[pos].port);
					//组装消息队列内请求信息
					Message msg;
					msg.ins = ins;
					msg.from = pos;
					msg.to = req.number;
					msg.processed = msg.sent = false;
					//将该消息加入消息队列中
					pthis->queueMessege.push(msg);
				}
			default:
				printf("ERROR: The request from the client %d can't be recognized!\n", pos);
				break;
			}
		}
	}

	static void closeService(int number, struct sockaddr_in saClient, Server * pthis) {
		pthis->mapClient.erase(number);
		//TerminateThread(pthis->mapThread[number].native_handle(), 0);
		pthis->mapThread.erase(number);
		printf("Client %d (%s:%d) misconnected.\n",number, inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port));
	}

private:
	SOCKET sListen, sServer; //侦听套接字，连接套接字
	std::map <int, std::thread> mapThread; //存储线程信息
	std::map <int, Client> mapClient; //存储客户端信息
	std::queue <Message> queueMessege; //用于转发数据的消息队列
};