#pragma once

enum mType { TIME,NAME,LIST,SEND,SEND_RES,PRE_INS };

struct request {	//请求数据包
	mType type;	//类型
	int number;		//转发客户端编号
	char content[100];	//内容
};

struct respond {	//响应数据包
	mType type;	//类型
	char data[300];	//数据
};

struct instruct {	//指令数据包	
	char content[100];	//内容
	int number;		//消息来源客户端编号
	char ip[20], port[20];		//IP地址和端口
};