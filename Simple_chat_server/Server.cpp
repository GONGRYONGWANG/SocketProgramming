#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")

#include<stdio.h>
#include<WinSock2.h>
#include<process.h>
#include<string.h>

int server_init();
int server_close();

unsigned int WINAPI run_chat(void* param);
unsigned int WINAPI receiveNpropagte(void* param);

int add_client(int index);
int read_client(int index);
void remove_client(int index);
int notify_client(char* messange);
char* get_client_ip(int index);


typedef struct sock_info {
	SOCKET s;
	HANDLE ev;
	char nick[50];
	char ipaddr[50];
} SOCK_INFO;

int port_number = 9999;
const int client_count = 10;
SOCK_INFO sock_arr[client_count + 1];
int total_socket_count = 0;




int main(int argc, char* argv[]) {
	unsigned int tid;
	char message[MAXBYTE] = "";
	HANDLE mainthread;

	if (argv[1] != NULL) {
		port_number = atoi(argv[1]);
	}
	mainthread = (HANDLE)_beginthreadex(NULL, 0, run_chat, (void*)0, 0, &tid);
	if (mainthread) {
		while (1) {
			gets_s(message, MAXBYTE);
			if (strcmp(message, "/x") == 0) break;
			notify_client(message);
		}
		server_close();
		WSACleanup();
		CloseHandle(mainthread);
	}
	return 0;
}

int server_init() {

	WSADATA wsadata;
	SOCKET s;
	SOCKADDR_IN server_address;

	memset(&sock_arr, 0, sizeof(sock_arr));
	total_socket_count = 0;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
		puts("WSAStartup error.");
		return -1;
	}
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		puts("socket error.");
		return -1;
	}

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port_number);

	if (bind(s, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
		puts("bind error.");
		return -2;
	}

	if (listen(s, SOMAXCONN) < 0) {
		puts("listen error.");
		return -3;
	}

	return s;

}

int server_close() {
	for (int i = 1; i < total_socket_count; i++) {
		closesocket(sock_arr[i].s);
		WSACloseEvent(sock_arr[i].ev);
	}
	return 0;
}

unsigned int WINAPI run_chat(void* param) {
	SOCKET server_socket = server_init();
	WSANETWORKEVENTS ev;
	int index;
	WSAEVENT handle_array[client_count + 1];

	if (server_socket < 0) {
		printf("initialization error\n");
		exit(0);
	}
	else {
		printf(" >> server intialization success.(portnumber:%d)\n", port_number);
		HANDLE event = WSACreateEvent();
		sock_arr[total_socket_count].ev = event;
		sock_arr[total_socket_count].s = server_socket;
		strcpy_s(sock_arr[total_socket_count].nick, "svr");
		strcpy_s(sock_arr[total_socket_count].ipaddr, "0.0.0.0");
		WSAEventSelect(server_socket, event, FD_ACCEPT);
		total_socket_count++;

		while (true) {
			memset(&handle_array, 0, sizeof(handle_array));
			for (int i = 0; i < total_socket_count; i++) {
				handle_array[i] = sock_arr[i].ev;
			}
			index = WSAWaitForMultipleEvents(total_socket_count, handle_array, false, INFINITE, false);
			if ((index != WSA_WAIT_FAILED) && (index != WSA_WAIT_TIMEOUT)) {
				WSAEnumNetworkEvents(sock_arr[index].s, sock_arr[index].ev, &ev);
				if (ev.lNetworkEvents == FD_ACCEPT) add_client(index);
				else if (ev.lNetworkEvents == FD_READ) read_client(index);
				else if (ev.lNetworkEvents == FD_CLOSE) remove_client(index);
			}
		}
		closesocket(server_socket);
	}
	WSACleanup();
	_endthreadex(0);

	return 0;

}

int add_client(int index) {

	SOCKADDR_IN addr;
	int len = 0;
	SOCKET accept_sock;

	if (total_socket_count == FD_SETSIZE) {
		return 1;
	}
	else {
		len = sizeof(addr);
		memset(&addr, 0, sizeof(addr));
		accept_sock = accept(sock_arr[0].s, (SOCKADDR*)&addr, &len);


		HANDLE event = WSACreateEvent();
		sock_arr[total_socket_count].ev = event;
		sock_arr[total_socket_count].s = accept_sock;
		strcpy_s(sock_arr[total_socket_count].ipaddr, inet_ntoa(addr.sin_addr));

		WSAEventSelect(accept_sock, event, FD_READ | FD_CLOSE);

		total_socket_count++;

		printf(" >> new client connection(IP : %s)\n", inet_ntoa(addr.sin_addr));

		char msg[256];
		sprintf_s(msg, ">> new client connection(IP : %s)\n", inet_ntoa(addr.sin_addr));
		notify_client(msg);
	}

	return 0;
}

int read_client(int index) {
	char read_ip[256];
	strcpy_s(read_ip, get_client_ip(index));
	printf("new message (INDEX: %d, IP: %s, Nick: %s)\n", index, read_ip, sock_arr[index].nick);
	unsigned int tid;
	HANDLE mainthread = (HANDLE)_beginthreadex(NULL, 0, receiveNpropagte, (void*)index, 0, &tid);
	WaitForSingleObject(mainthread, INFINITE);
	CloseHandle(mainthread);

	return 0;
}

unsigned int WINAPI receiveNpropagte(void* param) {

	int index = (int)param;
	char message[MAXBYTE], share_message[MAXBYTE];
	SOCKADDR_IN client_address;
	int recv_len = 0, addr_len = 0;
	char* token1 = NULL;
	char* next_token = NULL;

	memset(&client_address, 0, sizeof(client_address));

	if ((recv_len = recv(sock_arr[index].s, message, MAXBYTE, 0)) > 0) {
		addr_len = sizeof(client_address);
		getpeername(sock_arr[index].s, (SOCKADDR*)&client_address, &addr_len);
		strcpy_s(share_message, message);

		if (strlen(sock_arr[index].nick) <= 0) {
			token1 = strtok_s(message, "]", &next_token);
			strcpy_s(sock_arr[index].nick, token1 + 1);
		}
		for (int i = 1; i < total_socket_count; i++) {
			send(sock_arr[i].s, share_message, MAXBYTE, 0);
		}
	}

	_endthreadex(0);
	return 0;
}

void remove_client(int index) {
	char remove_ip[256];
	char message[MAXBYTE];

	strcpy_s(remove_ip, get_client_ip(index));
	printf(" >> client disconnected(INDEX: %d, IP: %s, Nick: %s)\n", index, remove_ip, sock_arr[index].nick);
	sprintf_s(message, " >> client disconnected(IP: %s, Nick: %s)\n", remove_ip, sock_arr[index].nick);

	closesocket(sock_arr[index].s);
	WSACloseEvent(sock_arr[index].ev);

	total_socket_count--;
	sock_arr[index].s = sock_arr[total_socket_count].s;
	sock_arr[index].ev = sock_arr[total_socket_count].ev;
	strcpy_s(sock_arr[index].ipaddr, sock_arr[total_socket_count].ipaddr);
	strcpy_s(sock_arr[index].nick, sock_arr[total_socket_count].nick);

	notify_client(message);
}

char* get_client_ip(int index) {
	static char ipaddress[256];
	int addr_len;
	struct sockaddr_in sock;
	addr_len = sizeof(sock);

	if (getpeername(sock_arr[index].s, (struct sockaddr*)&sock, &addr_len) < 0){
		return NULL;
	}
	strcpy_s(ipaddress, inet_ntoa(sock.sin_addr));
	return ipaddress;
}

int notify_client(char* message) {
	for (int i = 1; i < total_socket_count; i++) {
		send(sock_arr[i].s, message, MAXBYTE, 0);
	}
	return 0;
}