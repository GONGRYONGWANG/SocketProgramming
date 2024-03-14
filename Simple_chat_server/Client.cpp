#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")

#include<stdio.h>
#include<WinSock2.h>
#include<process.h>
#include<string.h>

int client_init(char* ip, int port);
unsigned int WINAPI run_chat(void* param);

int main(int argc,  char* argv[]) {

	char ip_addr[256] = "";
	int port_number = 9999;
	char nickname[50] = "";
	unsigned int tid;
	int sock;
	char input[MAXBYTE];
	char message[MAXBYTE];
	char* pexit = NULL;
	HANDLE mainthread;

	if (argc < 3) {
		printf("\n명령인수가 충분하지 않습니다.\n");
		printf("다음과 같이 설정하세요. [IPADDRESS] [portnumber] [nickname]\n");
		exit(0);
	}

	if (argv[1] != NULL && argv[2] != NULL && argv[3] != NULL) {
		strcpy_s(ip_addr, argv[1]);
		port_number = atoi(argv[2]);
		strcpy_s(nickname, argv[3]);
	}

	sock = client_init(ip_addr, port_number);

	if (sock < 0) {
		printf("sock_init error.");
		exit(0);
	}

	printf("Enter \"/x\" to terminate.\n");

	mainthread = (HANDLE)_beginthreadex(NULL, 0, run_chat, (void*)sock, 0, &tid);
	if (mainthread) {
		while (1) {
			gets_s(input, MAXBYTE);
			sprintf_s(message, "[%s] %s", nickname, input);
			send(sock, message, sizeof(message), 0);
			pexit = strrchr(message, '/');
			if (pexit) {
				if (strcmp(pexit, "/x") == 0) break;
			}
		}
		closesocket(sock);
		WSACleanup();
		CloseHandle(mainthread);
	}

	return 0;
}

int client_init(char* ip, int port) {
	SOCKET server_socket;
	WSADATA wsadata;
	SOCKADDR_IN server_address = { 0 };
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
		printf("WSAStartup error.");
		return -1;
	}
	if ((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		puts("socket error.");
		return -1;
	}

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(ip);
	server_address.sin_port = htons(port);

	if ((connect(server_socket, (struct sockaddr*)&server_address, sizeof(server_address))) < 0) {
		puts("connection error.");
		return -1;
	}

	return server_socket;

}

unsigned int WINAPI run_chat(void* params) {
	SOCKET s = (SOCKET)params;
	char recv_message[MAXBYTE];
	int len = 0;
	int index = 0;
	WSANETWORKEVENTS ev;
	HANDLE event = WSACreateEvent();

	WSAEventSelect(s, event, FD_READ | FD_CLOSE);
	while (1) {
		index = WSAWaitForMultipleEvents(1, &event, false, INFINITE, false);
		if ((index != WSA_WAIT_FAILED) && (index != WSA_WAIT_TIMEOUT)) {
			WSAEnumNetworkEvents(s, event, &ev);
			if (ev.lNetworkEvents == FD_READ) {
				int len = recv(s, recv_message, MAXBYTE, 0);
				if (len > 0) {
					printf("%s\n", recv_message);
				}
			}
			else if (ev.lNetworkEvents == FD_CLOSE) {
				printf(" >> server closed.( Enter \"/x\" to terminate.)\n");
				closesocket(s);
				break;
			}
		}
	}
	WSACleanup();
	_endthreadex(0);

	return 0;
}