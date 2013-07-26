/*
 * $Id: mod_offline.c 2786 2013-07-09 16:42:55 FreeWAF Development Team $
 *
 * (C) 2013-2014 FreeWAF Development Team
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file LICENSE.GPLv2.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/licenses/gpl-2.0.html
 *
 */
#define CORE_PRIVATE
#include "mod_offline.h"
#include "apr_strings.h"
#include "apr_tables.h"
#include "apr_lib.h"
#include "apr_network_io.h"
#include "apr_buckets.h"
#include "http_core.h"
#include "http_connection.h"
#include "http_config.h"
#include "util_filter.h"
#include "mod_core.h"

#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>

#define MIPS_WAF_DEVICE 0

#if MIPS_WAF_DEVICE
#include "efmp_libpcap_api.h"
#endif

/* �����ļٵ�conn */
#define OFFLINE_FAKE_CONN "offline_fake_conn"

/* Ĭ�ϵ�IO_BUFFER_SIZE */
#define OFFLINE_IO_BUFFER_SIZE 8192

/* �������INTERIM  ����*/
#define OFFLINE_MAX_INTERIM_RESPONSES 10

/* �������� */
struct offline_net_device {
    char ethname[256];
};

/* ���ýṹ�� */
typedef struct offline_server_conf {
    /* �ڴ��ָ�� */
    apr_pool_t * pool;
    /* �������� */
    apr_array_header_t * devices;
    /* ������б����������� */
    int pkt_num;
    /* �����ڴ���������λKB */
    int mem_limit;
    /* �������ӵ����������������ƣ���λKB */
    int max_recv_len;    
    /* ��־���� */
    int log_level;
    /* ��־��С���ƣ���λKB */
    int log_limit;
    /* ��ȡ��Ӧ�ĳ�ʱֵ����λ�� */
    apr_interval_time_t recv_timeout;
    /* ���ڼ�¼�򿪵ľ�� */
    int fd;
    /* ��־���� */
    char log_name[512];
    /* ģ���Ƿ�򿪵Ŀ��� */
    int is_opened;
} offline_server_conf;

/* ������ĳ�� */
module AP_MODULE_DECLARE_DATA offline_module;

/* ���Ӷ��� */
APR_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(offline, OFFLINE, int, create_req,
                                   (request_rec *r, request_rec *pr), (r, pr),
                                   OK, DECLINED)
                                   
APR_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(offline, OFFLINE, int, pre_connection,
                                   (conn_rec *c, void *csd), (c, csd),
                                   OK, DECLINED)


static void * create_offline_config(apr_pool_t *p, server_rec *s)
{
    offline_server_conf *ps = apr_pcalloc(p, sizeof(offline_server_conf));
    char *path;
            
    if ( !ps ) {
        return 0;
    }
    /* ��ʼ��Ĭ��ֵ */
    ps->pool = p;
    /* Ĭ��fatal */
    ps->log_level = 0;
    /* Ĭ��1024MB */
    ps->log_limit = 1024 * 1024;
    /* Ĭ��1MB */
    ps->max_recv_len = 1 * 1024;
    /* Ĭ��512MB */
    ps->mem_limit = 512 * 1024;
    /* Ĭ�� 1000000��*/
    ps->pkt_num = 1000000;
    /* Ĭ��60 �� */
    ps->recv_timeout = 60000000;
    /* ��������*/
    ps->devices = apr_array_make(p, 1,  sizeof(struct offline_net_device *));
    if ( !ps->devices ) {
        return 0;
    }
    /* ��ʼ����� */
    ps->fd = 0;
    /* ��־���� */
    path = ap_server_root_relative(p, "logs/pe_offline.log");
    snprintf(ps->log_name, 512, "%s", path);
    /* �������� */
    ps->is_opened = 0;

    return ps;
}

static void * merge_offline_config(apr_pool_t *p, void *basev, void *overridesv)
{
    offline_server_conf *ps, *base, *overrides;
    
    ps = apr_pcalloc(p, sizeof(offline_server_conf));
    if ( !ps ) {
        return 0;
    }
    base = (offline_server_conf *) basev;
    overrides = (offline_server_conf *) overridesv;
    if ( !base || !overrides ) {
        return 0;
    } else {
        ps->pool = p;

        ps->log_level = overrides->log_level;
        ps->log_limit = overrides->log_limit;
        ps->max_recv_len = overrides->max_recv_len;
        ps->mem_limit = overrides->mem_limit;
        ps->pkt_num = base->pkt_num > overrides->pkt_num ? base->pkt_num : overrides->pkt_num;

        ps->recv_timeout = base->recv_timeout > overrides->recv_timeout ? base->recv_timeout : overrides->recv_timeout;
        ps->devices = apr_array_append(p, base->devices, overrides->devices);

        memset(ps->log_name, 0, sizeof(ps->log_name));
        strcpy(ps->log_name, overrides->log_name);

        ps->is_opened = overrides->is_opened;
    }
    
    return ps;
}

/* ��ȡ�������е�Client Socket */
static apr_socket_t * offline_get_client_socket(request_rec *r)
{
    core_net_rec * net;
    struct ap_filter_t * current;

    if ( !r ) {
        return NULL;
    }
    if ( !r->input_filters ) {
        return NULL;
    }

    /* �������������� */
    for ( current = r->input_filters; current != 0;  current = current->next ) {
        if ( ap_core_input_filter_handle == current->frec ) {
            net = current->ctx;
            if ( net != 0 && net->client_socket != 0 ) {
                return net->client_socket;
            }
        }
    }

    return NULL;
}

/* ����α������ */
static request_rec * offline_make_fake_request(conn_rec *c, request_rec *r)
{
    request_rec * rp = apr_pcalloc(r->pool, sizeof(*r));
    if ( !rp ) {
        return 0;
    }

    rp->pool = r->pool;
    rp->status = HTTP_OK;

    rp->headers_in = apr_table_make(r->pool, 50);
    rp->subprocess_env = apr_table_make(r->pool, 50);
    rp->headers_out = apr_table_make(r->pool, 50);
    rp->err_headers_out = apr_table_make(r->pool, 10);
    rp->notes = apr_table_make(r->pool, 10);

    rp->server = r->server;
    rp->request_time = r->request_time;
    rp->connection = c;
    rp->output_filters = c->output_filters;
    rp->input_filters = c->input_filters;
    rp->proto_output_filters = c->output_filters;
    rp->proto_input_filters = c->input_filters;

    rp->request_config = (ap_conf_vector_t *)ap_create_request_config(r->pool);
    offline_run_create_req(r, rp);

    return rp;
}

/* ��ȡһ�� */
static apr_status_t offline_getline(apr_bucket_brigade * bb, char * s, int n, request_rec * r, int fold, int * writen)
{
    char *tmp_s = s;
    apr_status_t rv;
    apr_size_t len;

    rv = ap_rgetline(&tmp_s, n, &len, r, fold, bb);
    apr_brigade_cleanup(bb);

    if (rv == APR_SUCCESS) {
        *writen = (int) len;
    } else if (rv == APR_ENOSPC) {
        *writen = n;
    } else {
        *writen = -1;
    }

    return rv;
}

/* ���ӹرյĴ��� */
static void offline_backend_broke(request_rec *r, apr_bucket_brigade *brigade)
{
    apr_bucket *e;
    conn_rec *c = r->connection;

    r->no_cache = 1;
    if (r->main)
        r->main->no_cache = 1;

    /* �������洢�κͽ����洢�Σ���ʾ���� */
    e = ap_bucket_error_create(HTTP_BAD_GATEWAY, NULL, c->pool, c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(brigade, e);
    e = apr_bucket_eos_create(c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(brigade, e);
}

/* �������� */
static int offline_addit_dammit(void *v, const char *key, const char *val)
{
    apr_table_addn(v, key, val);
    return 1;
}

/* ��ȡHTTP ͷ */
static void offline_read_headers(request_rec *r, request_rec *rr
                                 , char *buffer, int size, conn_rec *c, int *pread_len)
{
    int len;
    char *value, *end;
    char field[MAX_STRING_LEN];

    /* ��ʼ��50 ��ͷ�Ŀռ� */
    r->headers_out = apr_table_make(r->pool, 50);
    *pread_len = 0;

    /* 
    * ��ѭ������������
    * 1. ����һ������
    * 2. ��ȡ��ʱ
    * 3. ��ȡ�������Ѿ��ر�
    * 4. ��ȡ����������
    */
    while ( (len = ap_getline(buffer, size, rr, 1)) > 0 ) {
        if ( !(value = strchr(buffer, ':')) ) {
            /* �Ҳ���ð�ŵ���ֱ�Ӻ��� */
            continue;
        }

        /* �ҵ�ð�ź�Ľ������� */
        *value = '\0';
        ++value;
        while (apr_isspace(*value)) {
            ++value; /* �ҵ��ǿ��ַ�Ϊֹ */
        }
        /* ȥ���ַ���ĩβ�Ŀո� */
        for ( end = &value[strlen(value) - 1]; end > value && apr_isspace(*end); --end ) {
            *end = '\0';
        }

        /* ������������key-value�Է���headers-out */
        apr_table_add(r->headers_out, buffer, value) ;
        
        /* ����ͷ����ʱ��Ĵ��� */
        if (len >= size - 1) {
            while ((len = ap_getline(field, MAX_STRING_LEN, rr, 1)) >= MAX_STRING_LEN - 1) {
                ;
            }
            if (len == 0) /* time to exit the larger loop as well */
                break;
        }
    }
}

static apr_status_t offline_buckets_lifetime_transform(request_rec *r, apr_bucket_brigade *from,
                                    apr_bucket_brigade *to)
{
    apr_bucket *e;
    apr_bucket *new;
    const char *data;
    apr_size_t bytes;
    apr_status_t rv = APR_SUCCESS;

    apr_brigade_cleanup(to);
    for (e = APR_BRIGADE_FIRST(from);
         e != APR_BRIGADE_SENTINEL(from);
         e = APR_BUCKET_NEXT(e)) {
        if (!APR_BUCKET_IS_METADATA(e)) {
            apr_bucket_read(e, &data, &bytes, APR_BLOCK_READ);
            new = apr_bucket_transient_create(data, bytes, r->connection->bucket_alloc);
            APR_BRIGADE_INSERT_TAIL(to, new);
        }
        else if (APR_BUCKET_IS_FLUSH(e)) {
            new = apr_bucket_flush_create(r->connection->bucket_alloc);
            APR_BRIGADE_INSERT_TAIL(to, new);
        }
        else if (APR_BUCKET_IS_EOS(e)) {
            new = apr_bucket_eos_create(r->connection->bucket_alloc);
            APR_BRIGADE_INSERT_TAIL(to, new);
        }
        else {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                          "offline: Unhandled bucket type of type %s in"
                          " offline_buckets_lifetime_transform", e->type->name);
            rv = APR_EGENERAL;
        }
    }
    return rv;
}

static apr_status_t offline_http_process_response(apr_pool_t * p, request_rec *r, conn_rec * origin, offline_server_conf * psf) 
{
    conn_rec * c = r->connection;
    char buffer[HUGE_STRING_LEN];
    const char * buf;
    char keepchar;
    request_rec * rp;
    apr_bucket * e;
    apr_bucket_brigade *bb, *tmp_bb;
    apr_bucket_brigade *pass_bb;
    int len, backasswards;
    apr_socket_t * client_socket;

    /* �Ƿ������1xx ���Ƶ���Ӧ״̬�� */
    int interim_response = 0; 

    int pread_len = 0;
    apr_table_t * save_table;
    int backend_broke = 0;
    const char * te = NULL;
    int offline_status = OK;
    const char * offline_status_line = NULL;

    pass_bb = apr_brigade_create(p, c->bucket_alloc);
    tmp_bb = apr_brigade_create(p, c->bucket_alloc);

    /* ����α������ */
    rp = offline_make_fake_request(origin, r);
    if ( !rp ) {
        return APR_EGENERAL;
    }

    /* ������������ */
    rp->offlinereq = OFFLINEREQ_RESPONSE;

    /* ��ȡ��Ӧ���ҽ���Ӧ�������������洫 */
    bb = apr_brigade_create(p, c->bucket_alloc);
    do {
        apr_status_t rc;
        apr_brigade_cleanup(bb);

        rc = offline_getline(tmp_bb, buffer, sizeof(buffer), rp, 0, &len);

        if (r->proxy_response_time == 0) {
            r->proxy_response_time = apr_time_now();
        }
        
        if ( len == 0 ) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rc, r, "offline: error reading double CRLF, rc(%d), len(%d)", rc, len);
            /* ������ܴ��ڵĶ���� CRLF */
            rc = offline_getline(tmp_bb, buffer, sizeof(buffer), rp, 0, &len);
        }
        if ( len <= 0 ) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rc, r, "offline: error reading status line from remote server, rc(%d), len(%d)", rc, len);
            if ( APR_STATUS_IS_TIMEUP(rc) ) {
                /* ��ȡ��ʱ*/
                ap_log_rerror(APLOG_MARK, APLOG_NOTICE, rc, r, "read timeout");
            } else {
                /* δ��ʱ����ȡ��� */
                apr_bucket *eos;
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rc, r, "offline_backend_broke");
                offline_backend_broke(r, bb);

                e = (apr_bucket *)ap_bucket_eoc_create(c->bucket_alloc);
                eos = APR_BRIGADE_LAST(bb);
                while ((APR_BRIGADE_SENTINEL(bb) != eos) && !APR_BUCKET_IS_EOS(eos)) {
                    eos = APR_BUCKET_PREV(eos);
                }
                if (eos == APR_BRIGADE_SENTINEL(bb)) {
                    APR_BRIGADE_INSERT_TAIL(bb, e);
                } else {
                    APR_BUCKET_INSERT_BEFORE(eos, e);
                }
                ap_pass_brigade(r->output_filters, bb);
                return OK;
            }
            return HTTP_BAD_GATEWAY;
        }

        /* ��Ӧ״̬�еĴ���*/
        if (apr_date_checkmask(buffer, "HTTP/#.# ###*")) {
            int major, minor;

            major = buffer[5] - '0';
            minor = buffer[7] - '0';

            if ( (major != 1) || (len >= sizeof(buffer) - 1) ) {
                /* HTTP Э�鲻Ϊ1�����汾�����߳��ȳ���8192 */
                return HTTP_BAD_GATEWAY;
            }
            backasswards = 0;

            keepchar = buffer[12];
            buffer[12] = '\0';
            offline_status = atoi(&buffer[9]);

            if (keepchar != '\0') {
                buffer[12] = keepchar;
            } else {
                /* 
                 * RFC 2616 Ҫ��״̬���Կո������
                 * ���� ap_rgetline_core ���ܽ���ɾ����
                 * �����ﲹ�ϡ�
                 */
                buffer[12] = ' ';
                buffer[13] = '\0';
            }
            offline_status_line = apr_pstrdup(p, &buffer[9]);

            /* ״̬�еĴ��� */
            r->status = offline_status;
            r->status_line = offline_status_line;

            /* ����ͷ����r->headers_out��������Set-Cookie �Ĵ��� */
            save_table = apr_table_make(r->pool, 1);
            apr_table_do(offline_addit_dammit, save_table, r->headers_out, "Set-Cookie", NULL);
            offline_read_headers(r, rp, buffer, sizeof(buffer), origin, &pread_len);
            if (r->headers_out == NULL) {
                r->headers_out = apr_table_make(r->pool, 1);
                r->status = HTTP_BAD_GATEWAY;
                r->status_line = "bad gateway";
                return r->status;
            }
            apr_table_do(offline_addit_dammit, save_table, r->headers_out, "Set-Cookie", NULL);
            if (!apr_is_empty_table(save_table)) {
                apr_table_unset(r->headers_out, "Set-Cookie");
                r->headers_out = apr_table_overlay(r->pool, r->headers_out, save_table);
            }

            /* ͷ��ͬʱ����"Transfer-Encoding" �� "Content-Length"����Ϊ��������� */
            if (apr_table_get(r->headers_out, "Transfer-Encoding")
                && apr_table_get(r->headers_out, "Content-Length")) {
                    /* �����������ͬʱ���ڣ�����"Content-Length" */
                    apr_table_unset(r->headers_out, "Content-Length");
            }

            /* ��ȡ TE */
            te = apr_table_get(r->headers_out, "Transfer-Encoding");

            /* ����"Content-Type" */
            if ((buf = apr_table_get(r->headers_out, "Content-Type"))) {
                ap_set_content_type(r, apr_pstrdup(p, buf));
            }
            /* ��α����������HTTP_IN ��������� */
            if (!ap_is_HTTP_INFO(offline_status)) {
                ap_add_input_filter("HTTP_IN", NULL, rp, origin);
            }

            /* ���HTTP�汾С��1.1����ֱ��ȡ��keepalive���� */
            if ((major < 1) || (minor < 1)) {
                origin->keepalive = AP_CONN_CLOSE;
            }
        } else {
            /* 0.9 �汾����Ӧ */
            backasswards = 1;
            r->status = 200;
            r->status_line = "200 OK";
        }

        if ( ap_is_HTTP_INFO(offline_status) ) {
            interim_response++;
        } else {
            interim_response = 0;
        }

        if ( interim_response ) {
            const char *policy = apr_table_get(r->subprocess_env, "offline-interim-response");
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "offline: HTTP: received interim %d response", r->status);
            if (!policy || !strcasecmp(policy, "RFC")) {
                ap_send_interim_response(r, 1);
            } else if (strcasecmp(policy, "Suppress")) {
                ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, "undefined offline interim response policy");
            }
        }
        r->sent_bodyct = 1;

        /* ��Ҫ�����ʱ��ŷ����� */
        if ( !r->header_only && !interim_response && offline_status != HTTP_NO_CONTENT && offline_status != HTTP_NOT_MODIFIED ) {
            /* αװ������ͷ */
            rp->headers_in = apr_table_copy(r->pool, r->headers_out);
            /* ��"Transfer-Encoding" �����⴦�� */
            if (te && !apr_table_get(rp->headers_in, "Transfer-Encoding")) {
                apr_table_add(rp->headers_in, "Transfer-Encoding", te);
            }
            apr_table_unset(r->headers_out,"Transfer-Encoding");

            /* ��ʼ���壬���Ҵ������������ */
            if ( !ap_is_HTTP_ERROR(offline_status) || offline_status == HTTP_NOT_FOUND ) {
                
                /* ע��:   �˴���������ģʽ��ȡ */
                apr_read_type_e mode = APR_BLOCK_READ;
                int finish = FALSE;

                /* ��ȡ�ͻ������Ӷ��� */
                client_socket = offline_get_client_socket(r);
                if ( !client_socket ) {
                    return HTTP_INTERNAL_SERVER_ERROR;
                }

                do {
                    apr_status_t rv;
                    /* �����ɳ�ʱֵ*/
                    apr_interval_time_t old_inter, new_inter = -1;
                    
                    apr_socket_timeout_get(client_socket, &old_inter);
                    if ( APR_BLOCK_READ == mode ) {
                        apr_socket_timeout_set(client_socket, new_inter);
                    }

                    rv = ap_get_brigade(rp->input_filters, bb, AP_MODE_READBYTES, mode, OFFLINE_IO_BUFFER_SIZE);
                    if ( APR_STATUS_IS_EAGAIN(rv) || (rv == APR_SUCCESS && APR_BRIGADE_EMPTY(bb))) {
                        /* ����Ӧˢ�¸��ͻ��ˣ������л�������ģʽ */
                        e = apr_bucket_flush_create(c->bucket_alloc);
                        APR_BRIGADE_INSERT_TAIL(bb, e);
                        
                        if (ap_pass_brigade(r->output_filters, bb) || c->aborted) {
                            break;
                        }
                        apr_brigade_cleanup(bb);
                        mode = APR_BLOCK_READ;

                        /* �ָ�ԭ��ʱֵ */
                        apr_socket_timeout_set(client_socket, old_inter);
                        continue;
                    }  else if (rv == APR_EOF) {
                    
                        /* �ָ�ԭ��ʱֵ */
                        apr_socket_timeout_set(client_socket, old_inter);
                        break;
                    } else if (rv != APR_SUCCESS) {
                        ap_log_cerror(APLOG_MARK, APLOG_WARNING, rv, c, "offline: error reading response");
                        offline_backend_broke(r, bb);
                        ap_pass_brigade(r->output_filters, bb);
                        backend_broke = 1;

                        /* �ָ�ԭ��ʱֵ */
                        apr_socket_timeout_set(client_socket, old_inter);
                        break;
                    }

                    /* ע��:   �˴���������ģʽ��ȡ */
                    mode = APR_BLOCK_READ;
                    if (APR_BRIGADE_EMPTY(bb)) {
                        apr_brigade_cleanup(bb);
                        break;
                    }

                    /* Switch the allocator lifetime of the buckets */
                    offline_buckets_lifetime_transform(r, bb, pass_bb);

                    if (APR_BUCKET_IS_EOS(APR_BRIGADE_LAST(bb))) {
                        finish = TRUE;
                    }
                    
                    if (ap_pass_brigade(r->output_filters, pass_bb) != APR_SUCCESS || c->aborted) {
                        finish = TRUE;
                    }

                    apr_brigade_cleanup(bb);
                    apr_brigade_cleanup(pass_bb);

                } while (!finish);
            }
        } else if ( !interim_response ) {
            /* ����Ҫ�� */
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "offline: header only interim_response(%d), offline_status(%d)", interim_response, offline_status);

            /* ֱ�Ӵ��������� */
            e = apr_bucket_eos_create(c->bucket_alloc);
            APR_BRIGADE_INSERT_TAIL(bb, e);

            ap_pass_brigade(r->output_filters, bb);
            apr_brigade_cleanup(bb);
        }
    } while ( interim_response && (interim_response < OFFLINE_MAX_INTERIM_RESPONSES) );

    /* If our connection with the client is to be aborted, return DONE. */
    if (c->aborted || backend_broke) {
        return DONE;
    }

    return OK;
}

/* ���߿��ʼ�� */
static int offline_open_logs(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
    int ret, i = 0;
#if  MIPS_WAF_DEVICE
    struct interface_bind_info_s bind_info;
#endif

    /* ��ֹ���� */
    apr_socket_terminate();

    offline_server_conf *psf = ap_get_module_config(s->module_config, &offline_module);
    if ( !psf || !psf->is_opened ) {
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, "offline_open_logs DECLINED");
        return DECLINED;
    }

#if MIPS_WAF_DEVICE
    /* ��ʼ��mips-libpcap */
    psf->fd = open("/dev/wafdev", O_RDONLY);
    if ( psf->fd < 0 ) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, 0, s, "open pf_packet device failed!");
        return DECLINED;
    }
#endif
   
    /* ��ʼ���� */
    ret = apr_socket_initialize(psf->pkt_num, psf->mem_limit, psf->max_recv_len, psf->log_level, psf->log_limit, psf->log_name);
    if ( ret == APR_EGENERAL ) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, 0, s, "apr_socket_initialize failed!");
        return DECLINED;
    } else if ( ret > 0 ) {
        //apr_socket_start_listen();
    }
    
    /* ���ñ�־ */
    apr_socket_set_offline_mode(1/* open it */);
    apr_pollset_set_offline_mode(1/* open it */); 
    
#if MIPS_WAF_DEVICE
    for ( i = 0; i < psf->devices->nelts; ++i ) {
        memset(&bind_info, 0, sizeof(bind_info));
        bind_info.ifx = atoi(APR_ARRAY_IDX(psf->devices, i, struct offline_net_device *)->ethname);
        ret = ioctl(psf->fd, LIBPCAP_BIND_INTERFACE, &bind_info);
        if (ret < 0) {
            close(psf->fd);
            return DECLINED;
        }
        apr_socket_add_device(bind_info.net_device_name);
    }
#else
    for ( i = 0; i < psf->devices->nelts; ++i ) {
        apr_socket_add_device(APR_ARRAY_IDX(psf->devices, i, struct offline_net_device *)->ethname);
    }
#endif
    
    return OK;
}

/* ���ߴ����� */
static int offline_handler(request_rec *r)
{
    int rc;
    apr_status_t rv;
    struct ap_filter_t * f;
    apr_interval_time_t current_timeout;
    apr_bucket_alloc_t *bucket_alloc;
    apr_socket_t * client_socket;
    conn_rec * fake_conn;
    offline_server_conf * psf;
    conn_rec * real_conn = r->connection;

    /* ��ȡ������Ϣ */
    psf = ap_get_module_config(r->server->module_config, &offline_module);
    if ( !psf  || !psf->is_opened ) {
        ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "offline_handler DECLINED");
        return DECLINED;
    }

    if (r->proxy_request_time == 0) {
        r->proxy_request_time = apr_time_now();
    }
    
    /* ��ȡ�ͻ������Ӷ��� */
    client_socket = offline_get_client_socket(r);
    if ( !client_socket ) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }
        
    /* �����洢�����ڴ������ */
    if ( !real_conn->offline_conn ) {
        bucket_alloc = apr_bucket_alloc_create(real_conn->pool);

        /* �����µ����Ӷ��� */
        fake_conn = (conn_rec *)ap_run_create_connection(real_conn->pool, r->server, client_socket, 0, NULL, bucket_alloc);
        if (!fake_conn) {
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        real_conn->offline_conn = fake_conn;

        /* ����Ԥ����*/
        apr_socket_timeout_get(client_socket, &current_timeout);
        rc = offline_run_pre_connection(fake_conn, client_socket);
        if (rc != OK && rc != DONE) {
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        apr_socket_timeout_set(client_socket, current_timeout);
    } else {
        fake_conn = real_conn->offline_conn;
    }

    /* ��ʼ��α��������д��� */
    rv = offline_http_process_response(r->pool, r, fake_conn, psf);

    /* ժ��HTTP_IN ������ */
    if ( fake_conn->input_filters ) {
        for ( f = fake_conn->input_filters; f != 0;  f = f->next ) {
            if ( ap_http_input_filter_handle == f->frec ) {
                ap_remove_input_filter(f);
                break ;
            }
        }
    }

    return rv;
}


/* ʵ����ָ�� */
static const char * offline_set_ptk_num(cmd_parms *parms, void *dummy, const char *arg)
{
    int rv;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "offline_set_ptk_num");
    
    offline_server_conf *psf = ap_get_module_config(parms->server->module_config, &offline_module);
    if ( !psf ) {
        return "offline_set_ptk_num psf is null!";
    }

    rv = atoi(arg);
    if ( rv > psf->pkt_num ) {
        psf->pkt_num = rv;
    }
    return NULL;
}

static const char * offline_set_mem_limit(cmd_parms *parms, void *dummy, const char *arg)
{
    int rv;
    
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "offline_set_mem_limit");
    
    offline_server_conf *psf = ap_get_module_config(parms->server->module_config, &offline_module);
    if ( !psf ) {
        return "offline_set_mem_limit psf is null!";
    }

    rv = atoi(arg);
    if ( rv > psf->mem_limit ) {
        psf->mem_limit = rv;
    }
    return NULL;
}

static const char * offline_set_max_recv_len(cmd_parms *parms, void *dummy, const char *arg)
{
    int rv;
    
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "offline_set_max_recv_len");
    
    offline_server_conf *psf = ap_get_module_config(parms->server->module_config, &offline_module);
    if ( !psf ) {
        return "offline_set_max_recv_len psf is null!";
    }

    rv = atoi(arg);
    if ( rv > psf->max_recv_len ) {
        psf->max_recv_len = rv;
    }
    return NULL;
}

static const char * offline_set_log_level(cmd_parms *parms, void *dummy, const char *arg)
{
    int rv = 0;
    
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "offline_set_log_level");
    
    offline_server_conf *psf = ap_get_module_config(parms->server->module_config, &offline_module);
    if ( !psf ) {
        return "offline_set_log_level psf is null!";
    }

    if ( !strcmp(arg, "debug") ) {
        rv = 0;
    } else if ( !strcmp(arg, "info") ) {
        rv = 1;
    } else if ( !strcmp(arg, "warn") ) {
        rv = 2;
    } else if ( !strcmp(arg, "fatal") ) {
        rv = 3;
    } else if ( !strcmp(arg, "stop") ) {
        rv = 4;
    }
    
    if ( rv > psf->log_level ) {
        psf->log_level = rv;
    }
    return NULL;
}

static const char * offline_set_log_limit(cmd_parms *parms, void *dummy, const char *arg)
{
    int rv;
    
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "offline_set_log_limit");
    
    offline_server_conf *psf = ap_get_module_config(parms->server->module_config, &offline_module);
    if ( !psf ) {
        return "offline_set_log_limit psf is null!";
    }

    rv = atoi(arg);
    if ( rv > psf->log_limit ) {
        psf->log_limit = rv;
    }
    return NULL;
}

static const char * offline_set_recv_timeout(cmd_parms *parms, void *dummy, const char *arg)
{
    int rv;
    
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "offline_set_recv_timeout");
    
    offline_server_conf *psf = ap_get_module_config(parms->server->module_config, &offline_module);
    if ( !psf ) {
        return "offline_set_recv_timeout psf is null!";
    }

    rv = atoi(arg);
    if ( rv > psf->recv_timeout ) {
        psf->recv_timeout = rv * 1000000;
    }
    return NULL;
}

static const char * offline_set_ethname(cmd_parms *parms, void *dummy, const char *arg)
{
    int i, to_add;
    struct offline_net_device * device, *current;
    
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "offline_set_ethname");
    
    offline_server_conf *psf = ap_get_module_config(parms->server->module_config, &offline_module);
    if ( !psf ) {
        return "offline_set_ethname psf is null!";
    }

    if ( !psf->devices ) {
        psf->devices = apr_array_make(psf->pool, 1,  sizeof(struct offline_net_device *));
        if ( !psf->devices ) {
            return "offline_set_ethname make new devices failed!";
        }
    }

    to_add = 1;
    for ( i = 0; i < psf->devices->nelts; ++i ) {
        current = (struct offline_net_device *)(psf->devices->elts) + i;
        if ( 0 == strcmp(arg,  current->ethname) ) {
            to_add = 0; break; 
        }
    }
    if ( to_add ) {
        device = apr_pcalloc(psf->pool, sizeof(struct offline_net_device));
        if ( !device ) {
            return "apr_pcalloc failed!";
        }
        if ( !strncpy(device->ethname, arg, sizeof(device->ethname) - 1) ) {
            return "ethname must be set!";
        }
        *((struct offline_net_device **)apr_array_push(psf->devices)) = device;
    }
    
    return NULL;
}

static const char * offline_set_log_name(cmd_parms *parms, void *dummy, const char *arg)
{    
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "offline_set_log_name");
    
    offline_server_conf *psf = ap_get_module_config(parms->server->module_config, &offline_module);
    if ( !psf ) {
        return "offline_set_log_name psf is null!";
    }

   memset(psf->log_name, 0, sizeof(psf->log_name));
    if ( !strncpy(psf->log_name, arg, sizeof(psf->log_name) - 1) ) {
        return "log_name must be set!";
    }
    
    return NULL;
}

static const char * offline_set_engine(cmd_parms *parms, void *dummy, const char *arg)
{    
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "offline_set_engine");
    
    offline_server_conf *psf = ap_get_module_config(parms->server->module_config, &offline_module);
    if ( !psf ) {
        return "offline_set_engine psf is null!";
    }

    if ( !strcmp("on", arg) || !strcmp("On", arg)  || !strcmp("ON", arg) ) {
        psf->is_opened = 1;
    } else {
        psf->is_opened = 0;
    }
    
    return NULL;
}

/* ʵ����ָ�� */
static const command_rec offline_cmds[] =
{
    AP_INIT_TAKE1("OfflinePktNum", offline_set_ptk_num, NULL, RSRC_CONF, "pkt num ceiling in a message queue"),
    AP_INIT_TAKE1("OfflineMemLimit", offline_set_mem_limit, NULL, RSRC_CONF, "self mem pool ceiling"),
    AP_INIT_TAKE1("OfflineMaxRecvLen", offline_set_max_recv_len, NULL, RSRC_CONF, "sum of the pkts size in 1 direction"),
    AP_INIT_TAKE1("OfflineLogLevel", offline_set_log_level, NULL, RSRC_CONF, "log level"),
    AP_INIT_TAKE1("OfflineLogLimit", offline_set_log_limit, NULL, RSRC_CONF, "log file size ceiling"),
    AP_INIT_TAKE1("OfflineRecvTimeOut", offline_set_recv_timeout, NULL, RSRC_CONF, "time to wait for recv response very time"),
    AP_INIT_TAKE1("OfflineEthName", offline_set_ethname, NULL, RSRC_CONF, "which eth should be monitored"),
    AP_INIT_TAKE1("OfflineLogName", offline_set_log_name, NULL, RSRC_CONF, "where log should be putted"),
    AP_INIT_TAKE1("OfflineEngine", offline_set_engine, NULL, RSRC_CONF, "engine on or off"),
    {NULL}
};

static void offline_register_hooks(apr_pool_t *p)
{
    ap_hook_open_logs(offline_open_logs, NULL, NULL, APR_HOOK_BLOODY_FIRST);
    ap_hook_handler(offline_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA offline_module = {
    STANDARD20_MODULE_STUFF, 
    NULL,
    NULL,
    create_offline_config,
    merge_offline_config,
    offline_cmds,
    offline_register_hooks
};

