/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * TCP JSON 服务器实现。由 STM32 LwIP netconn + CMSIS-RTOS 版本移植到 Zephyr:
 *   - netconn_new/bind/listen/accept/recv -> BSD socket API(zsock_*)
 *   - osThreadNew -> K_THREAD_STACK + k_thread_create
 *   - printf -> printk
 * 每行一条 JSON 命令,经 FaultCommand_ExecuteLine 分发;行缓冲逻辑与原实现一致。
 */
#include "app/tcp_json_server.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

#include "app/fault_command.h"

#define TCP_JSON_SERVER_STACK_SIZE 2048U
#define TCP_JSON_SERVER_LINE_SIZE  256U
#define TCP_JSON_SERVER_BACKLOG    1

static K_THREAD_STACK_DEFINE(tcp_json_stack, TCP_JSON_SERVER_STACK_SIZE);
static struct k_thread tcp_json_thread;
static k_tid_t tcp_json_tid;

typedef struct {
	int sock;
} TcpJsonServer_OutputContext;

static void TcpJsonServer_Write(const char *text, void *context)
{
	TcpJsonServer_OutputContext *output = (TcpJsonServer_OutputContext *)context;

	if ((output == NULL) || (output->sock < 0) || (text == NULL)) {
		return;
	}

	(void)zsock_send(output->sock, text, strlen(text), 0);
}

static void TcpJsonServer_HandleLine(int sock, const char *line)
{
	TcpJsonServer_OutputContext output = {sock};

	FaultCommand_ExecuteLine(line, TcpJsonServer_Write, &output);
}

static void TcpJsonServer_HandleClient(int sock)
{
	char rx[128];
	char line[TCP_JSON_SERVER_LINE_SIZE];
	uint16_t lineLength = 0U;
	int received;

	const char *hello =
		"{\"ok\":true,\"service\":\"bms-fault-simulator\",\"hint\":\"send one JSON command per line\"}\r\n";

	(void)zsock_send(sock, hello, strlen(hello), 0);

	while ((received = zsock_recv(sock, rx, sizeof(rx), 0)) > 0) {
		for (int index = 0; index < received; index++) {
			char ch = rx[index];

			if ((ch == '\r') || (ch == '\n')) {
				if (lineLength > 0U) {
					line[lineLength] = '\0';
					TcpJsonServer_HandleLine(sock, line);
					lineLength = 0U;
				}
				continue;
			}

			if (lineLength < (TCP_JSON_SERVER_LINE_SIZE - 1U)) {
				line[lineLength++] = ch;
			} else {
				lineLength = 0U;
				(void)zsock_send(sock,
						 "{\"ok\":false,\"error\":\"line_too_long\"}\r\n",
						 38U, 0);
			}
		}
	}
}

static void TcpJsonServer_Task(void *p1, void *p2, void *p3)
{
	struct sockaddr_in bind_addr;
	int server;

	(void)p1;
	(void)p2;
	(void)p3;

	server = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server < 0) {
		printk("[tcp] socket create failed\r\n");
		return;
	}

	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind_addr.sin_port = htons(TCP_JSON_SERVER_PORT);

	if (zsock_bind(server, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		printk("[tcp] bind %u failed\r\n", TCP_JSON_SERVER_PORT);
		(void)zsock_close(server);
		return;
	}

	if (zsock_listen(server, TCP_JSON_SERVER_BACKLOG) < 0) {
		printk("[tcp] listen failed\r\n");
		(void)zsock_close(server);
		return;
	}

	printk("[tcp] JSON server listening on port %u\r\n", TCP_JSON_SERVER_PORT);

	for (;;) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client = zsock_accept(server, (struct sockaddr *)&client_addr,
					  &client_len);

		if (client >= 0) {
			printk("[tcp] client connected\r\n");
			TcpJsonServer_HandleClient(client);
			(void)zsock_close(client);
			printk("[tcp] client disconnected\r\n");
		} else {
			k_msleep(100);
		}
	}
}

void TcpJsonServer_Start(void)
{
	if (tcp_json_tid == NULL) {
		tcp_json_tid = k_thread_create(&tcp_json_thread, tcp_json_stack,
					       K_THREAD_STACK_SIZEOF(tcp_json_stack),
					       TcpJsonServer_Task, NULL, NULL, NULL,
					       K_PRIO_PREEMPT(10), 0, K_NO_WAIT);
		(void)k_thread_name_set(tcp_json_tid, "TcpJson");
	}
}
