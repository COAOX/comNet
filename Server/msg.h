#pragma once

enum mType { TIME,NAME,LIST,SEND,ERR };

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
	mType type;
	char content[100];
	int number;
	char ip[20], port[20];
};