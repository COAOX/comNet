#pragma once

enum mType { TIME,NAME,LIST,SEND,SEND_RES };

struct request {
	mType type;
	int number;
	char content[100];
};

struct respond {
	mType type;
	char data[300];
};

struct instruct {
	char content[100];
	int number;
	char ip[20], port[20];
};