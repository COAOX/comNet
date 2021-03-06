// ConsoleApplication1.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "msg.h"
#include <iostream>
#include <WinSock2.h>
#include <winsock.h>
#include <string>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <cstdio>
#include <process.h>
#include <windows.h>
#define SERVER_PORT	6666
#define MY_MSG WM_USER+100
#define TIME_MSG WM_USER+110
#define NAME_MSG TIME_MSG+10
#define LIST_MSG NAME_MSG+10
#define SENDRE_MSG LIST_MSG+10


using namespace std;



bool linkState;
DWORD cThread;

class client {
public:
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret;
	SOCKET sClient; //连接套接字
	struct sockaddr_in saServer;//地址信息
	

	BOOL fSuccess = TRUE;

	


	client() {
		wVersionRequested = MAKEWORD(2, 2);//希望使用的WinSock DLL的版本
		ret = WSAStartup(wVersionRequested, &wsaData);
		if (ret != 0)
		{
			printf("WSAStartup() failed!\n");
			return;
		}

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			WSACleanup();
			printf("Invalid Winsock version!\n");
			return;
		}

		sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sClient == INVALID_SOCKET)
		{
			WSACleanup();
			printf("socket() failed!\n");
			return;
		}
	}

	int connec(string addr) {
		char* taddr=(char*)addr.data();
		saServer.sin_family = AF_INET;//地址家族
		saServer.sin_port = htons(SERVER_PORT);//注意转化为网络字节序
		saServer.sin_addr.S_un.S_addr = inet_addr(taddr);
		ret = connect(sClient, (struct sockaddr *)&saServer, sizeof(saServer));
		if (ret == SOCKET_ERROR)
		{
			printf("connect() failed!\n");
			closesocket(sClient);//关闭套接字
			WSACleanup();
			return 0;
		}
		return 1;
	}

	

	int sen(struct request &mg) {
		ret = send(sClient, (char *)&mg, sizeof(mg), 0);
		if (ret == SOCKET_ERROR)
		{
			printf("send() failed!\n");
			return 1;
		}
		else
		{
			return 0;
		}
	}


	

	char ServerAddr[15];
};

client cli;
bool threadflag;


unsigned int __stdcall recMess(LPVOID pm)
{
	struct respond res;		//响应数据包
	struct instruct ins;		//指令数据包
	int Left = 0;		//已接受字节数
	char * pt = NULL;		//接受地址指针
	while(threadflag) {		//子线程循环
		int nLeft = sizeof(res);		//接受剩余字节更新
		char * ptr = (char *)&res;		//地址指针更新
		while (nLeft > 0) {		//接受循环
			
			cli.ret = recv(cli.sClient, ptr, nLeft, 0);		//socket接收消息
			if (cli.ret == SOCKET_ERROR)		//socket接受出现错误
			{
				printf("Recv() failed!\n");
				break;
			}

			if (cli.ret == 0) //客户端已经关闭连接
			{
				printf("\nServer has closed the connection!\n");
				threadflag = false;
				break;
			}
			nLeft -= cli.ret;
			ptr += cli.ret;
		}
		if (!nLeft) {//数据包分别处理
			switch (res.type) {
			case TIME:PostThreadMessage(cThread, TIME_MSG, (WPARAM)(&res), 0); break;
			case NAME:PostThreadMessage(cThread, NAME_MSG, (WPARAM)(&res), 0); break;
			case LIST:PostThreadMessage(cThread, LIST_MSG, (WPARAM)(&res), 0); break;
			case SEND_RES:PostThreadMessage(cThread, SENDRE_MSG, (WPARAM)(&res), 0); break;
			case PRE_INS:
				Left = sizeof(ins);
				pt = (char *)&ins;
				while (Left > 0) {

					cli.ret = recv(cli.sClient, pt, Left, 0);
					if (cli.ret == SOCKET_ERROR)
					{
						printf("Recv() failed!\n");
						threadflag = false;
						break;
					}

					if (cli.ret == 0) //客户端已经关闭连接
					{
						printf("Client has closed the connection!\n");
						threadflag = false;
						break;
					}
					Left -= cli.ret;
					pt += cli.ret;
				}
				
				printf("\nYou have a new message\n");
				printf("####################################\n");
				printf("Server number:%d\nFrom ip:%s\nport:%s\nMessage detail: %s\n", ins.number, ins.ip, ins.port, ins.content);
				printf("####################################\n>>>");
				break;
			default:break;
			}
		}
	}
	return 0;
}

int main()
{
	cli = client();	//初始化客户端
	linkState = false;		//连接状态

	string cmd;		//命令
	string ipaddr;		//IP地址
	MSG msg;		//线程通信消息
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);	//创建消息队列
	HANDLE hThread;		//子线程句柄
	bool flag = true;		//主线程循环标志
	printf("Client open!\n");
	printf("Please enter the commond(link,close,time,name,list,send,quit)\n");
	cThread = GetCurrentThreadId();	//主线程句柄
	while (flag) {		//主线程循环
		printf(">>>");	
		cin >> cmd;//输入命令

		unsigned nThreadID=0;
		/*命令处理*/
		switch (cmd.c_str()[0]) {
		case 'l'://连接
			if(cmd.c_str()[2]=='n'){//link
				printf("Please enter the ip address\n>>>");
				cin >> ipaddr;
				if (cli.connec(ipaddr) == 1) {
					linkState = true;	//连接
					printf("Connected\n");
				}
				else break;
				threadflag = true;
				hThread = (HANDLE)_beginthreadex(NULL, 0, recMess, NULL, 0, &nThreadID);//创建子线程
				break;//link
			}

			else {
				struct request re;
				re.type = LIST;
				cli.sen(re);


				for (;;) {
					GetMessage(&msg, 0, 0, 0);
					if (msg.message == LIST_MSG) {
						struct respond * pInfo = (struct respond *)msg.wParam;	//子线程消息
						char * ind = pInfo->data;
						printf("Number\tip\t\tport\t\n");
						for (int i = 0; i < 10 && *(ind) != '\n'; i++) {
							printf("%s\n", ind);
							ind += 30;
						}
						//delete pInfo;
						break;
					}
				}
				break;//Linked list
			}
		case 'q':flag = false; 
		case 'c':
			if (linkState) {
				threadflag = false;
				closesocket(cli.sClient);//关闭socket连接
				linkState = false;
				threadflag = 0;
				printf("Socket closed\n");
			}
			break;//close
		case 't':
			if (!linkState) {
				printf("No connection/n");
				break;
			}

			struct request req;
			req.type = TIME;

			cli.sen(req);


			for (;;) {
				GetMessage(&msg, 0, 0, 0);
				if (msg.message == TIME_MSG) {
					struct respond * pInfo = (struct respond *)msg.wParam;//接受子线程消息
					printf("%s", pInfo->data);
					//delete pInfo;
					break;
				}

			}


			break;//time
		case 'n':
			if (!linkState) {
				printf("no connection\n");
				break;
			}
			struct request r;
			r.type = NAME;

			cli.sen(r);
			for (;;) {
				GetMessage(&msg, 0, 0, 0);

				if (msg.message == NAME_MSG) {
					struct respond* pInfo = (struct respond *)msg.wParam;		//接受子线程消息
					printf("%s\n", pInfo->data);
					//delete pInfo;
					break;
				}
			}
			break;//name
			
		case 's'://发送数据
			struct request rr;
			rr.type = SEND;
			printf("Please enter the client number\n>>>");
			cin>>rr.number;		//客户端标号
			printf("Please enter the message\n>>>");
			cin >> rr.content;		//内容
			cli.sen(rr);			//发送消息
			
			for (;;) {
				GetMessage(&msg, 0, 0, 0);		//从消息队列获取消息
				if (msg.message == SENDRE_MSG) {
					struct respond * pInfo = (struct respond *)msg.wParam;

					printf("Post state:%s\n", pInfo->data);			//打印消息发送状态

					//delete pInfo;
					break;
				}
				break;//send msg
		//quit
			}
			
		}   ////
			
		
	}
	return 0;
}