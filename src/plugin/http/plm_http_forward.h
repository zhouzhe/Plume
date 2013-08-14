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

#ifndef _PLM_HTTP_FORWARD_H
#define _PLM_HTTP_FORWARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct plm_http_forward {
	/* orign server */
	struct in_addr hf_addr;
	unsigned short hf_port;
	uint16_t hf_send_header : 1;
	uint16_t hf_send_body : 1;
	uint16_t hf_post_recv : 1;

	int hf_fd;
	void *hf_request;
	void (*hf_fn)(void *, int);
	void *hf_data;
};

void plm_http_forward_start(struct plm_http_request *request, void *data,
							void (*fn)(void *, int));

void plm_http_forward_body(struct plm_http_request *request, void *data,
						   void (*fn)(void *, int));	

#ifdef __cplusplus
}
#endif

#endif