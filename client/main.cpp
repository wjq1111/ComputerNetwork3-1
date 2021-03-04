#pragma comment(lib, "Ws2_32.lib")
#include<iostream>
#include<WinSock2.h>
#include<string>
#include<string.h>
#include<time.h>
#include<fstream>
#include<stdio.h>
#include<vector>
using namespace std;
SOCKET client;
SOCKADDR_IN serverAddr, clientAddr;
#define TIMEOUT 500
#define SENDSUCCESS true
#define SENDFAIL false
#define SEQ1 '1'
#define ACK1 '#'
#define SEQ2 '3'
#define ACK2 SEQ1 + 1
#define SEQ3 '5'
#define ACK3 SEQ2 + 1
#define WAVE1 '7'
#define ACKW1 '#'
#define WAVE2 '9'
#define ACKW2 WAVE1 + 1
#define LENGTH 16377
#define CheckSum 16384 - LENGTH
#define LAST '$'
#define NOTLAST '@'
#define TEST '%'
#define ACKMsg '%'
bool No1 = 1;
char message[200000000];
// checksum检查包
unsigned char PkgCheck(char *arr, int len)
{
	if (len == 0)
		return ~(0);
	unsigned char ret = arr[0];
	for (int i = 1; i < len; i++)
	{
		unsigned int tmp = ret + (unsigned char)arr[i];
		tmp = tmp / (1 << 8) + tmp % (1 << 8);
		tmp = tmp / (1 << 8) + tmp % (1 << 8);
		ret = tmp;
	}
	return ~ret;
}
// 发包
void Sendpackage(char* msg, int len, int index, int last)
{
	char *buffer = new char[len + CheckSum];
	if (last)
	{
		buffer[1] = LAST;
	}
	else
	{
		buffer[1] = NOTLAST;
	}
	buffer[2] = index / 128;
	buffer[3] = index % 128;
	buffer[4] = len / 128;
	buffer[5] = len % 128;
	if (!No1)
		buffer[6] = ACKMsg;
	else
		buffer[6] = '$';
	
	for (int i = 0; i < len; i++)
	{
		buffer[i + CheckSum] = msg[i];
	}
	while (1)
	{
		buffer[0] = PkgCheck(buffer + 1, len + CheckSum - 1);
		sendto(client, buffer, LENGTH + CheckSum, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
		int begin_time = clock();
		char recv[3];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 3, 0, (sockaddr *)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int over_time = clock();
			if (over_time - begin_time > TIMEOUT)
			{
				break;
			}
		}
		if (recv[0] == ACKMsg)
		{
			if (index != recv[1] * 128 + recv[2])
				continue;
			break;
		}
		else
		{
			cout << "Error!Send again!" << endl;
			buffer[6] = ACKMsg;
			No1 = 0;
		}
	}
	delete buffer;
}
// 发送消息
void Sendmessage(string filename, int size)
{
	int len = 0;
	if (filename == "quit")
	{
		return;
	}
	else
	{
		ifstream fin(filename.c_str(), ifstream::binary);
		if (!fin)
		{
			cout << "This file disappeared!" << endl;
			return;
		}
		unsigned char ch = fin.get();
		while (fin)
		{
			message[len] = ch;
			len++;
			ch = fin.get();
		}
		fin.close();
	}
	int package_num = len / LENGTH + (len % LENGTH != 0);
	static int index = 0;
	for (int i = 0; i < package_num; i++)
	{
		Sendpackage(message + i * LENGTH
			, i == package_num - 1 ? len - (package_num - 1)*LENGTH : LENGTH
			, index++, i == package_num - 1);
		if (i % 10 == 0)
		{
			printf("Finished:%.2f%%\n", (float)i / package_num * 100);
		}
	}
}
// 连接到服务器
void ConnectToServer()
{
	while (1)
	{
		char send[2];
		send[0] = SEQ1;
		send[1] = ACK1;
		sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
		cout << "Client sent first handshake." << endl;
		int begin_time = clock();
		bool flag = SENDSUCCESS;
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 2, 0, (sockaddr *)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int over_time = clock();
			if (over_time - begin_time > TIMEOUT)
			{
				flag = SENDFAIL;
				break;
			}
		}
		if (flag && recv[0] == SEQ2 && recv[1] == ACK2)
		{
			cout << "Client received second handshake." << endl;
			send[0] = SEQ3;
			send[1] = ACK3;
			sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
			cout << "Client sent third handshake." << endl;
			break;
		}
	}
}
// 发送文件名
void SendName(string filename, int size)
{
	char *name = new char[size + 1];
	for (int i = 0; i < size; i++)
	{
		name[i] = filename[i];
	}
	name[size] = '$';
	sendto(client, name, size + 1, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
	delete name;
}
// 开始客户端
int StartClient()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartup Error:" << WSAGetLastError() << endl;
		return -1;
	}
	client = socket(AF_INET, SOCK_DGRAM, 0);

	if (client == SOCKET_ERROR)
	{
		cout << "Socket Error:" << WSAGetLastError() << endl;
		return -1;
	}
	int Port = 1439;
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(Port);
	cout << "Start Client Success!" << endl;
	return 1;
}
// 关闭客户端
void CloseClient()
{
	while (1)
	{
		char send[2];
		send[0] = WAVE1;
		send[1] = ACKW1;
		sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
		cout << "Client waved first hand." << endl;
		int begin_time = clock();
		bool flag = SENDSUCCESS;
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 2, 0, (sockaddr *)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int over_time = clock();
			if (over_time - begin_time > TIMEOUT)
			{
				flag = SENDFAIL;
				break;
			}
		}
		if (flag && recv[0] == WAVE2 && recv[1] == ACKW2)
		{
			break;
		}
	}
}
int main()
{
	StartClient();
	cout << "Connecting to Server..." << endl;
	ConnectToServer();
	cout << "Connect Successfully" << endl;
	int time_out = 1;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));
	string filename;
	cin >> filename;
	SendName(filename, filename.length());
	Sendmessage(filename, filename.length());
	cout << "Send Sucessfully!" << endl;
	CloseClient();
	WSACleanup();
	return 0;
}