#pragma once
#include "msg.h"
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <cstdio>
#include<cstring>
#include <ctime>
#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT	6666 //�����˿�
#define MaxClient 10
#define ServerName "Server1"

struct Client {
	char ip[20];
	int port;
};

class Server
{
public:
	Server() {
		WORD wVersionRequested;
		WSADATA wsaData;
		wVersionRequested = MAKEWORD(2, 2);//ϣ��ʹ�õ�WinSock DLL�İ汾
		int ret = WSAStartup(wVersionRequested, &wsaData);
		if (ret != 0) {
			printf("WSAStartup() failed!\n");
			return;
		}

		//ȷ��WinSock DLL֧�ְ汾2.2��
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2){
			WSACleanup();
			printf("Invalid Winsock version!\n");
			return;
		}

		//����socket��ʹ��TCPЭ�飺
		sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sListen == INVALID_SOCKET) {
			WSACleanup();
			printf("socket() failed!\n");
			return;
		}

		//�������ص�ַ��Ϣ��
		struct sockaddr_in saServer;//��ַ��Ϣ
		saServer.sin_family = AF_INET;//��ַ����
		saServer.sin_port = htons(SERVER_PORT);//ע��ת��Ϊ�����ֽ���
		saServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//ʹ��INADDR_ANYָʾ�����ַ

		//�󶨣�
		ret = bind(sListen, (struct sockaddr *)&saServer, sizeof(saServer));
		if (ret == SOCKET_ERROR) {
			printf("bind() failed! code:%d\n", WSAGetLastError());
			closesocket(sListen);//�ر��׽���
			WSACleanup();
			return;
		}

		//������������
		ret = listen(sListen, 5);
		if (ret == SOCKET_ERROR) {
			printf("listen() failed! code:%d\n", WSAGetLastError());
			closesocket(sListen);//�ر��׽���
			WSACleanup();
			return;
		}
	}

	~Server() {
		for (int i = 0; i < clientNum; i++) {
			threads[i].join();
		}
		closesocket(sListen);//�ر��׽���
		closesocket(sServer);
		WSACleanup();
	}

	void run() {
		while (true) {
			//printf("Waiting for client connecting!\n");
			//�����ȴ����ܿͻ������ӣ�
			struct sockaddr_in saClient;
			int length = sizeof(saClient);
			sServer = accept(sListen, (struct sockaddr *)&saClient, &length);
			if (sServer == INVALID_SOCKET){
				printf("accept() failed! code:%d\n", WSAGetLastError());
				continue;
			}
			if (clientNum == MaxClient) {
				printf("�̳߳شﵽ��������");
			}
			else {
				Client client;
				strcpy(client.ip, inet_ntoa(saClient.sin_addr));
				client.port = ntohs(saClient.sin_port);
				clients[clientNum] = client;
				threads[clientNum] = std::thread(trans, sServer, saClient, clientNum);
				clientNum++;
				printf("Accepted client: %s:%d\n", inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port));
			}
		}
	}

	static void trans(SOCKET arg, SOCKADDR_IN client_addr, int pos){
		request req;
		int sen, ret;
		while (true) {
			ret = recv(sServer, (char*)&req, sizeof(request), 0);
			if (ret == SOCKET_ERROR) {
				printf("recv() failed! code:%d\n", WSAGetLastError());
				continue;
			}
			respond res;
			switch (req.type)
			{
			TIME:
				res.type = TIME;
				time_t t;    
				time(&t);  
				strcpy(res.data, ctime(&t));
				sen = send(sServer, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			NAME:
				res.type = TIME;
				strcpy(res.data, ServerName);
				sen = send(sServer, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			LIST:
				res.type = LIST;
				char list[300];
				for (int i = 0; i < clientNum; i++) {
					char clinfo[30] = { 0 };
					sprintf(clinfo, "%d/t%s/t%d", i, clients[i].ip, clients[i].port);
					strcpy(list + 30 * i, clinfo);
				}
				strcpy(res.data, list);
				sen = send(sServer, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			SEND:
				instruct ins;


			default:
				break;
			}
		}
	}

private:
	static SOCKET sListen, sServer; //�����׽��֣������׽���
	std::thread threads[MaxClient];
	static Client clients[MaxClient];
	static int clientNum;
};

