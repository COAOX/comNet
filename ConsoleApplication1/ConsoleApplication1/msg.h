#pragma once

enum mType { TIME,NAME,LIST,SEND,SEND_RES,PRE_INS };

struct request {	//�������ݰ�
	mType type;	//����
	int number;		//ת���ͻ��˱��
	char content[100];	//����
};

struct respond {	//��Ӧ���ݰ�
	mType type;	//����
	char data[300];	//����
};

struct instruct {	//ָ�����ݰ�	
	char content[100];	//����
	int number;		//��Ϣ��Դ�ͻ��˱��
	char ip[20], port[20];		//IP��ַ�Ͷ˿�
};