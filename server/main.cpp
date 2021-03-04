#pragma comment(lib, "Ws2_32.lib")
#include<iostream>
#include<WinSock2.h>
#include<string>
#include<string.h>
#include<fstream>
using namespace std;
SOCKET server;
SOCKADDR_IN serverAddr, clientAddr;
#define CONNECTSUCCESS true
#define CONNECTFAIL false
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
#define ACKMsg '%'
#define NAK '^'
string filename;
char message[200000000];
static char pindex[2];		// 用于标志已经收到了哪个包
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
// 等待连接
void WaitConnection()
{
	while (1)
	{
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(server, recv, 2, 0, (sockaddr *)&clientAddr, &clientlen) == SOCKET_ERROR);
		if (recv[0] != SEQ1)
			continue;
		cout << "Server received first handshake." << endl;
		bool flag = CONNECTSUCCESS;
		while (1)
		{
			memset(recv, 0, 2);
			char send[2];
			send[0] = SEQ2;
			send[1] = ACK2;
			sendto(server, send, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
			cout << "Server sent second handshake." << endl;
			while (recvfrom(server, recv, 2, 0, (sockaddr *)&clientAddr, &clientlen) == SOCKET_ERROR);
			if (recv[0] == SEQ1)
				continue;
			if (recv[0] != SEQ3 || recv[1] != ACK3)
			{
				cout << "Connection failed.\nPlease restart your client." << endl;
				flag = CONNECTFAIL;
			}
			break;
		}
		if (!flag)
			continue;
		break;
	}
}
// 开启服务器
int StartServer()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartup Error:" << WSAGetLastError() << endl;
		return -1;
	}
	server = socket(AF_INET, SOCK_DGRAM, 0);

	if (server == SOCKET_ERROR)
	{
		cout << "Socket Error:" << WSAGetLastError() << endl;
		return -1;
	}
	int Port = 1439;
	serverAddr.sin_addr.s_addr = htons(INADDR_ANY);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(Port);

	if (bind(server, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		cout << "Bind Error:" << WSAGetLastError() << endl;
		return -1;
	}
	cout << "Start Server Success!" << endl;
	return 1;
}
// 等待断开连接
void WaitDisconnection()
{
	while (1)
	{
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(server, recv, 2, 0, (sockaddr *)&clientAddr, &clientlen) == SOCKET_ERROR);
		if (recv[0] != WAVE1)
			continue;
		cout << "Server received first hand." << endl;
		char send[2];
		send[0] = WAVE2;
		send[1] = ACKW2;
		sendto(server, send, 2, 0, (sockaddr *)&clientAddr, sizeof(clientAddr));
		break;
	}
	cout << "Client Disconnected." << endl;
}
// 接收消息
void Recvmessage()
{
	pindex[0] = 0;
	pindex[1] = -1;
	int len = 0;
	while (1)
	{
		char recv[LENGTH + CheckSum];
		memset(recv, '\0', LENGTH + CheckSum);
		int length;
		while (1)
		{
			int clientlen = sizeof(clientAddr);
			while (recvfrom(server, recv, LENGTH + CheckSum, 0, (sockaddr *)&clientAddr, &clientlen) == SOCKET_ERROR);
			length = recv[4] * 128 + recv[5];
			char send[3];
			memset(send, '\0', 3);
			// 检查ACK
			if (recv[6] == ACKMsg)
			{
				send[0] = ACKMsg;
				// 这一个包是不是顺序的下一个包
				if (((pindex[0] == recv[2] && pindex[1] + 1 == recv[3]) || (pindex[0] + 1 == recv[2] && recv[3] == 0 && pindex[1] == 127)) && PkgCheck(recv, length + CheckSum) == 0)
				{
					pindex[0] = recv[2];
					pindex[1] = recv[3];
					send[1] = recv[2];
					send[2] = recv[3];
				}
				else
				{
					send[1] = -1;
					send[2] = -1;
					sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
					continue;
				}
				sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
				break;
			}
			// 出错了 发送一个NAK
			else
			{
				cout << "Something Wrong." << endl;
				send[0] = NAK;
				send[1] = recv[2];
				send[2] = recv[3];
				sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
				continue;
			}
		}
		for (int i = CheckSum; i < length + CheckSum; i++)
		{
			message[len] = recv[i];
			len++;
		}
		if (recv[1] == LAST)
			break;
	}
	ofstream fout(filename.c_str(), ofstream::binary);
	for (int i = 0; i < len; i++)
		fout << message[i];
	fout.close();
}
// 接收文件名
void RecvName()
{
	char name[100];
	int clientlen = sizeof(clientAddr);
	while (recvfrom(server, name, 100, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR);
	for (int i = 0; name[i] != '$'; i++)
	{
		filename += name[i];
	}
}
int main()
{
	StartServer();
	cout << "Waiting..." << endl;
	WaitConnection();
	cout << "Connect to Client!" << endl;
	RecvName();
	Recvmessage();
	cout << "ReceiveMessage Over!" << endl;
	WaitDisconnection();
	closesocket(server);
	WSACleanup();
	return 0;
}