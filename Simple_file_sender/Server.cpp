#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")

#include<stdio.h>
#include<WinSock2.h>
#include<process.h>
#include<string.h>

#define PACKET_SIZE 1024
#define PACKET_TYPE 0x11
#define FILE_DELIMITER '&'

typedef struct {
	char type;
	int len;
} PACKET_HEADER;

char* save_folder = NULL;
int server_port = 9999;

int write_file(char* filename, void* data, int data_len) {
	FILE* fp;
	int len = 0;
	if ((data != NULL) && (data_len > 0)) {
		if ((fp = fopen(filename, "wb")) == NULL) {
			puts("fopen error.");
			return -1;
		}

		if ((fwrite(data, sizeof(char), data_len, fp)) < data_len) {
			puts("fwrite error.");
			fclose(fp);
		}
		fclose(fp);
	}
	return 0;
}

int save_file(char* file_buf, int filelen) {
	char* split_pos = NULL;
	char fullpath[MAX_PATH] = "", filename[MAX_PATH] = "";
	int filename_len = 0, filecontent_len = 0;

	split_pos = strchr(file_buf, FILE_DELIMITER);
	if (save_folder[strlen(save_folder) - 1] == '\\') {
		strcpy(fullpath, save_folder);
	}
	else sprintf(fullpath, "%s\\", save_folder);

	strncpy(filename, file_buf, split_pos - file_buf);
	strcat(fullpath, filename);

	filename_len = strlen(filename);
	filecontent_len = filelen - (filename_len + sizeof(FILE_DELIMITER));

	if (write_file(fullpath, file_buf + strlen(filename) + sizeof(FILE_DELIMITER), filecontent_len) != 0) {
		puts("write_file error.");
		return -1;
	}

	printf(" >> received file name : %s, size : %dbytes\n", fullpath, filecontent_len);

	return 0;
}

int recv_socket(int sock, char* recv_buf, int recv_len) {
	fd_set rset;
	struct timeval tvalue;
	int signal_flag, len;
	while (1) {
		tvalue.tv_sec = 3600;
		tvalue.tv_usec = 0;
		FD_ZERO(&rset);
		FD_SET(sock, &rset);

		if ((signal_flag = select(0, &rset, NULL, NULL, &tvalue)) <= 0) {
			if (signal_flag == 0) {
				if (send(sock, (char*)&signal_flag, 4, 0) < 0) {
					return -1;
				}
			}
			break;
		}
		if (FD_ISSET(sock, &rset)) {
			if ((len = recv(sock, recv_buf, recv_len, 0)) > 0) {
				return len;
			}
			else break;
		}
		else break;
	}
	return -1;

}

int recv_file(int socket) {
	PACKET_HEADER trans_header;
	char* header = (char*)&trans_header;
	char* file_buf = NULL, * data_buf = NULL;
	int header_len = 5, filelen = 0, read_len = 0, data_len = 0;

	memset(&trans_header, 0, sizeof(PACKET_HEADER));
	while (read_len = recv_socket(socket, (char*)header, header_len)) {
		if (read_len == -1) {
			return -1;
		}
		else if (read_len == header_len) break;

		header_len -= read_len;
		header += read_len;
	}
	memcpy(&trans_header.len, &header[1], 4);
	filelen = ntohl(trans_header.len);

	if ((filelen <= 0) || (trans_header.type != PACKET_TYPE)) {
		return -1;
	}
	if ((file_buf = (char*)calloc(1, filelen + 1)) == NULL) {
		return -1;
	}

	read_len = 0;
	data_buf = file_buf;
	data_len = filelen;
	while (1) {
		read_len = recv_socket(socket, (char*)data_buf, data_len);
		if (read_len == -1) {
			puts("recv_socket error.");
			return -1;
		}
		else if (read_len == data_len) break;

		data_len -= read_len;
		data_buf += read_len;
	}
	save_file(file_buf, filelen);

	free(file_buf);
	return 0;

}

unsigned int WINAPI fileserver_main(void* arg) {
	int sock = (int)arg;
	int rv = 0;

	if (recv_file(sock) < 0) {
		puts("fileserver_recv error.");
	}
	closesocket(sock);
	_endthreadex(0);
	return NULL;
}

int get_client_ip(int socket) {
	struct sockaddr_in sock;
	int len = sizeof(sock);

	if (getpeername(socket, (struct sockaddr*)&sock, &len) < 0) {
		return -1;
	}
	printf(" >> connected client IP : [%s]\n", inet_ntoa(sock.sin_addr));
	return 0;
}

int init_fileserver(int port_num) {
	struct sockaddr_in server_addr, client_addr;
	int server_socket, client_socket;
	int len, reuseflag = TRUE;
	unsigned int thread_id;
	WSADATA wsadata;

	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
		puts("WSAStartup error.");
		return -1;
	}

	if ((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		puts("socket error.");
		return -1;
	}

	memset((char*)&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons((unsigned short)port_num);

	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseflag, sizeof(reuseflag));

	while (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		puts("bind error.");
		Sleep(120 * 1000);
	}

	if (listen(server_socket, SOMAXCONN) < 0) {
		puts("listen error.");
		return -1;
	}

	while (1) {
		len = sizeof(client_addr);
		if ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &len)) < 0){
			continue;
		}
		HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, fileserver_main, (void*)client_socket, 0, &thread_id);
		if (thread == NULL) {
			puts(" >> _beginthreadex error.");
			closesocket(client_socket);
		}
		printf(" >> thread created(socket=%d, tid=%d)\n", client_socket, thread_id);
		CloseHandle(thread);

		get_client_ip(client_socket);
	}
	return 0;

}

unsigned int WINAPI do_file_service(void* params) {
	init_fileserver(server_port);

	_endthreadex(0);
	return NULL;
}

int main(int argc, char* argv[]) {
	unsigned int tid;
	HANDLE mainthread;
	if (argc != 3) {
		printf("명령인수가 충분하지 않습니다.\n");
		printf("다음과 같이 설정하세요. [portnumber] [directory_path]\n");
		exit(0);
	}

	server_port = atoi(argv[1]);
	save_folder = argv[2];

	mainthread = (HANDLE)_beginthreadex(NULL, 0, do_file_service, (void*)0, 0, &tid);
	if (mainthread) {
		printf(" >> serever initialization success\n");
		WaitForSingleObject(mainthread, INFINITE);
		CloseHandle(mainthread);
	}
	return 0;
}