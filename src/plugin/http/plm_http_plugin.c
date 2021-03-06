/* Copyright (c) 2013 Xingxing Ke <yykxx@hotmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "plm_plugin.h"
#include "plm_http.h"
#include "plm_http_request.h"
#include "plm_http_plugin.h"

#define DEF_BACKLOG 5

static void *plm_http_ctx_create(void *);
static void plm_http_ctx_destroy(void *);
static int plm_http_listen_set(void *, plm_dlist_t *);
static int plm_http_backend_set(void *, plm_dlist_t *);

/* shared from plume main context */
static struct plm_share_param sp;

struct plm_plugin http_plugin;
static struct plm_cmd http_cmds[] = {
	{
		&http_plugin,
		plm_string("http"),
		PLM_BLOCK,
		NULL,
		plm_http_ctx_create,
		plm_http_ctx_destroy
	},
	{
		&http_plugin,
		plm_string("http_listen"),
		PLM_INSTRUCTION,
		plm_http_listen_set,
		NULL,
		NULL
	},
	{
		&http_plugin,
		plm_string("http_backend"),
		PLM_INSTRUCTION,
		plm_http_backend_set,
		NULL,
		NULL
	},
	{0}
};

static void plm_http_set_main_conf(struct plm_share_param *);
static int plm_http_on_work_proc_start(struct plm_ctx_list *);
static void plm_http_on_work_proc_exit(struct plm_ctx_list *);
static int plm_http_on_work_thrd_start(struct plm_ctx_list *);
static void plm_http_on_work_thrd_exit(struct plm_ctx_list *);

struct plm_plugin http_plugin = {
	plm_http_set_main_conf,
	plm_http_on_work_proc_start,
	plm_http_on_work_proc_exit,
	plm_http_on_work_thrd_start,
	plm_http_on_work_thrd_exit,
	http_cmds
};

void *plm_http_ctx_create(void *parent)
{
	struct plm_http_ctx *ctx;

	ctx = (struct plm_http_ctx *)malloc(sizeof(struct plm_http_ctx));
	if (ctx) {
		memset(ctx, 0, sizeof(struct plm_http_ctx));
		ctx->hc_backlog = DEF_BACKLOG;

		PLM_LIST_INIT(&ctx->hc_backends);
	}
	return (ctx);
}

void plm_http_ctx_destroy(void *data)
{
	struct plm_http_ctx *ctx;
	struct plm_http_backend *bd;

	ctx = (struct plm_http_ctx *)data;
	if (ctx->hc_addr.s_str)
		plm_strclear(&ctx->hc_addr);

	/* free all backends */
	do {
		if (PLM_LIST_LEN(&ctx->hc_backends) == 0)
			break;

		bd = (struct plm_http_backend *)PLM_LIST_FRONT(&ctx->hc_backends);
		PLM_LIST_DEL_FRONT(&ctx->hc_backends);
		
		free(bd);
	} while (1);
	
	free(ctx);
}

/* http_listen 192.168.1.101 80 5 */
int plm_http_listen_set(void *ctx, plm_dlist_t *param_list)
{
	int n;
	struct plm_http_ctx *http_ctx;
	struct plm_cmd_param *param;

	n = PLM_DLIST_LEN(param_list);
	http_ctx = (struct plm_http_ctx *)ctx;
	if (n != 2 && n != 3) {
		plm_log_syslog("the number of http_listen's param is wrong");
		return (-1);
	}

	param = (struct plm_cmd_param *)PLM_DLIST_FRONT(param_list);
	
	plm_strzdup(&http_ctx->hc_addr, &param->cp_data);
	if (!http_ctx->hc_addr.s_str) {
		plm_log_syslog("strdup failed, memory emergent");
		return (-1);
	}

	if (-1 == inet_addr(http_ctx->hc_addr.s_str)) {
		plm_strclear(&http_ctx->hc_addr);
		plm_log_syslog("invalid ip address with http_listen");
		return (-1);
	}	

	param = (struct plm_cmd_param *)PLM_DLIST_NEXT(&param->cp_node);
	http_ctx->hc_port = plm_str2s(&param->cp_data);

	if (n == 3) {
		param = (struct plm_cmd_param *)PLM_DLIST_NEXT(&param->cp_node);
		http_ctx->hc_backlog = plm_str2i(&param->cp_data);
	}

	return (0);
}

/* http_backend 192.168.1.102 80 */
int plm_http_backend_set(void *ctx, plm_dlist_t *param_list)
{
	int n;
	unsigned short port;
	struct plm_http_ctx *http_ctx;
	struct plm_cmd_param *param;
	struct plm_http_backend *backend;
	struct sockaddr_in tmp;
	plm_string_t ip;

	n = PLM_DLIST_LEN(param_list);
	http_ctx = (struct plm_http_ctx *)ctx;
	if (n != 3) {
		plm_log_syslog("the number of http_backend's param is wrong");
		return (-1);
	}

	tmp.sin_family = AF_INET;
	
	param = (struct plm_cmd_param *)PLM_DLIST_FRONT(param_list);
	plm_strzdup(&ip, &param->cp_data);
	
	if (inet_addr(ip.s_str) == -1) {
		plm_strclear(&ip);
		plm_log_syslog("invalid backend address");
		return (-1);
	}

	tmp.sin_addr.s_addr = inet_addr(ip.s_str);
	plm_strclear(&ip);

	param = (struct plm_cmd_param *)PLM_DLIST_NEXT(&param->cp_node);
	port = plm_str2s(&param->cp_data);
	tmp.sin_port = htons(port);

	backend = (struct plm_http_backend *)malloc(sizeof(*backend));
	if (!backend) {
		plm_log_syslog("memory allocate failed: %s %d",
					   __FUNCTION__, __LINE__);
		return (-1);
	}

	memset(backend, 0, sizeof(*backend));
	backend->hb_addr.sin_family = tmp.sin_family;
	backend->hb_addr.sin_addr = tmp.sin_addr;
	backend->hb_addr.sin_port = tmp.sin_port;

	PLM_LIST_ADD_FRONT(&http_ctx->hc_backends, &backend->hb_node);
	return (0);
}

void plm_http_set_main_conf(struct plm_share_param *param)
{
	memcpy(&sp, param, sizeof(sp));
}

int plm_http_on_work_proc_start(struct plm_ctx_list *cl)
{
	struct plm_http_ctx *ctx;	

	ctx = (struct plm_http_ctx *)PLM_CTX_LIST_GET_POINTER(cl);

	plm_lookaside_list_init(&ctx->hc_conn_pool, sp.sp_maxfd,
							sizeof(struct plm_http_conn), sp.sp_tag,
							malloc, free);
	
	plm_lookaside_list_enable(&ctx->hc_conn_pool,
							  sp.sp_zeromem, sp.sp_tagcheck, sp.sp_thrdn > 1);
	
	return plm_http_open_server(ctx);
}

void plm_http_on_work_proc_exit(struct plm_ctx_list *cl)
{
	plm_http_close_server();
}

int plm_http_on_work_thrd_start(struct plm_ctx_list *cl)
{
	return (0);
}

void plm_http_on_work_thrd_exit(struct plm_ctx_list *cl)
{	
}
