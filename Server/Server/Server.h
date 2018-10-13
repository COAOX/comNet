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

#define SERVER_PORT	6666 //�����˿�
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
		closesocket(sListen);//�ر��׽���
		closesocket(sServer);
		WSACleanup();
	}

	int init() {
		WORD wVersionRequested;
		WSADATA wsaData;
		wVersionRequested = MAKEWORD(2, 2);//ϣ��ʹ�õ�WinSock DLL�İ汾
		int ret = WSAStartup(wVersionRequested, &wsaData);
		if (ret != 0) {
			printf("WSAStartup() failed!\n");
			return -1;
		}

		//ȷ��WinSock DLL֧�ְ汾2.2��
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
			WSACleanup();
			printf("Invalid Winsock version!\n");
			return -1;
		}

		//����socket��ʹ��TCPЭ�飺
		sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sListen == INVALID_SOCKET) {
			printf("socket() failed!\n");
			return -1;
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
			return -1;
		}

		//����socketΪ������ģʽ
		unsigned long ul = 1;
		ret = ioctlsocket(sListen, FIONBIO, (unsigned long *)&ul);//���óɷ�����ģʽ��
		if (ret == SOCKET_ERROR) {//����ʧ�ܡ�
			printf("ioctlsocket() failed! code:%d\n", WSAGetLastError());
			return -1;
		}

		//������������
		ret = listen(sListen, 5);
		if (ret == SOCKET_ERROR) {
			printf("listen() failed! code:%d\n", WSAGetLastError());
			return -1;
		}

		//��ʼ�����
		printf("The server has been initialized.\n"); 
		return 0;
	}

	void run() {
		while (true) {
			//������������
			struct sockaddr_in saClient;
			int length = sizeof(saClient);
			sServer = accept(sListen, (struct sockaddr *)&saClient, &length);
			if (sServer == INVALID_SOCKET) {
				if (WSAGetLastError() == WSAEWOULDBLOCK)
					continue;//��Դ��ʱ�����ã��������Խ�������
				else {//������������
					printf("accept() failed! code:%d\n", WSAGetLastError());
					return;
				}
			}
			else //���ɹ�����������������Ըÿͻ��˽������ӷ���
				openService(saClient);
		}
	}

	void openService(struct sockaddr_in saClient) {
		Client client;
		strcpy(client.ip, inet_ntoa(saClient.sin_addr));
		client.port = ntohs(saClient.sin_port);

		//Ѱ��δ��ռ�õ���С�ͻ��˱�ţ��������Ž�������
		int i;
		for (i = 0; i < MaxClient; i++) {
			if (mapClient.count(i) == 0) {
				mapClient[i] = client; //��¼�ͻ�����Ϣ
				mapThread[i] = std::thread(runService, sServer, saClient, i, this); //�����̲߳���¼��Ϣ
				mapThread[i].detach();
				break;
			}
		}
		if (i == MaxClient) //���ͻ������������������������򱨴�
			printf("ERROR: The server can't deal with more the %d clients.", MaxClient);
		else //�ɹ�������������ʾ�ͻ����ѽ�������
			printf("Client %d (%s:%d) connected.\n", i, inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port));
	}

	static void runService(SOCKET arg, SOCKADDR_IN client_addr, int pos, Server * pthis){	
		int sen, ret;
		while (true) {
			//���ȴ������������Ϣ�����е�����
			if (pthis->queueMessege.size() > 0) { 
				//������̱߳������߳�Ҫ����ָ�����ݰ�
				if (pthis->queueMessege.front().to == pos) {
					Message &msg = pthis->queueMessege.front();
					sen = send(arg, (char*)&(msg.ins), sizeof(msg.ins), 0); //��ָ���ͻ��˷�����Ϣ
					msg.processed = true; //��Ǹ������ѱ�����
					if (sen == SOCKET_ERROR) 
						printf("send() failed! code:%d\n", WSAGetLastError());
					else if (sen == 0) { //�Ѿ��Ͽ����ӣ���ر��߳�
						std::thread t = std::thread(closeService, pos, client_addr, pthis);
						t.detach();
						return;
					}
					else { //���ͳɹ������������ϢΪ���ɹ����͡�
						msg.sent = true;
					}
				}
				//������߳�Ҫ���͵�ָ�����ݰ��Ѿ�������
				else if (pthis->queueMessege.front().from == pos&& pthis->queueMessege.front().processed == true) { 
					respond res;
					res.type = SEND_RES;
					if (pthis->queueMessege.front().sent) //�ɹ��ʹ�
						strcpy(res.data, "SUCCESS: The message has been sent!");
					else //����ʧ��
						strcpy(res.data, "ERROR: The client is not in the list.");
					pthis->queueMessege.pop(); //������Ϣ����Ϣ������ɾ��
					sen = send(arg, (char*)&res, sizeof(res), 0); //��ͻ��˷��ͷ�����Ϣ
					if (sen == SOCKET_ERROR)
						printf("send() failed! code:%d\n", WSAGetLastError());
					else if (sen == 0) { //�Ѿ��Ͽ����ӣ���ر��߳�
						std::thread t = std::thread(closeService, pos, client_addr, pthis);
						t.detach();
						return;
					}
				}
			}

			//��δ���ͻ��˵�����
			request req;
			ret = recv(arg, (char*)&req, sizeof(request), 0); //���ܿͻ�����Ϣ
			if (ret == SOCKET_ERROR) { 
				if (WSAGetLastError() == WSAEWOULDBLOCK)//��Դ��ʱ�����ã��������Խ�����Ϣ
					continue;
				else if (WSAGetLastError() == WSAECONNRESET) { //�������쳣����ر��߳�
					std::thread t = std::thread(closeService, pos, client_addr, pthis);
					t.detach();
					return;
				}
				else
					printf("recv() failed! code:%d\n", WSAGetLastError());
				continue;
			}
			else if (ret == 0) { //�Ѿ��Ͽ����ӣ���ر��߳�
				std::thread t = std::thread(closeService, pos, client_addr, pthis);
				t.detach();
				return;
			}
			respond res;
			switch (req.type)
			{
			case TIME: //��������Ϊ��ȡʱ��
				res.type = TIME;
				time_t t;    
				time(&t);  //��ȡϵͳʱ��
				strcpy(res.data, ctime(&t));
				sen = send(arg, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			case NAME: //��������Ϊ��ȡ����
				res.type = TIME;
				strcpy(res.data, ServerName);
				sen = send(arg, (char*)&res, sizeof(res), 0);
				if (sen == SOCKET_ERROR)
					printf("send() failed! code:%d\n", WSAGetLastError());
				break;
			case LIST: //��������Ϊ��ȡ�ͻ����б�
				res.type = LIST;
				char list[300];
				char* plist;
				plist=list;
				//�����ͻ����б����ͻ�����Ϣ�����ַ���������
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
			case SEND: //��������Ϊ������Ϣ
				res.type = SEND_RES;
				//�������ڸÿͻ��˱�ţ��򱨴�
				if (pthis->mapClient.count(req.number)==0) {
					strcpy(res.data, "ERROR: The number of the client is not in the list.");
				}
				else {
					//��װָʾ���ݰ�
					instruct ins;
					strcpy(ins.content, req.content);
					strcpy(ins.ip, pthis->mapClient[pos].ip);
					sprintf(ins.port, "%d", pthis->mapClient[pos].port);
					//��װ��Ϣ������������Ϣ
					Message msg;
					msg.ins = ins;
					msg.from = pos;
					msg.to = req.number;
					msg.processed = msg.sent = false;
					//������Ϣ������Ϣ������
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
	SOCKET sListen, sServer; //�����׽��֣������׽���
	std::map <int, std::thread> mapThread; //�洢�߳���Ϣ
	std::map <int, Client> mapClient; //�洢�ͻ�����Ϣ
	std::queue <Message> queueMessege; //����ת�����ݵ���Ϣ����
};