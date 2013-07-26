/*
 * $Id: cli_common.c 2786 2013-07-09 16:42:55 FreeWAF Development Team $
 *
 * (C) 2013-2014 see FreeWAF Development Team
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <sys/ioctl.h>
#include <utmp.h>
#include "apr_network_io.h"
#include "cli_common.h"
#include "convert_rule.h"
#include "engine_config.h"
#include "http_config.h"
#include "cparser.h"

int cli_thread_may_exit = 0;
static apr_pool_t *pcli = NULL;                  /* cli�ڴ�� */
static apr_pool_t *pconf = NULL;                 /* conf�ڴ�� */
static apr_hash_t *secpolicy_list_hash = NULL;
static apr_hash_t *server_policy_hash = NULL;
server_rec *ap_main_server;
char g_cli_prompt[NAME_LEN_MAX];                 /* cli��ʾ�� */
ap_pod_t *pe_to_cli_pod;
ap_pod_t *cli_to_pe_pod;

/* ������������ʱ�����̺߳�cli�̵߳�ͬ�� */
static apr_status_t ap_sync_destroy(void *data)
{
    (void)ap_mpm_pod_close(pe_to_cli_pod);
    (void)ap_mpm_pod_close(cli_to_pe_pod);
    
    return APR_SUCCESS;
}

AP_DECLARE(int) ap_sync_init(apr_pool_t *pool)
{
    int rv;

    rv = ap_mpm_pod_open(pool, &pe_to_cli_pod);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "Create pe_to_cli_pod fail!");  
        return -1;
    }
    
    rv = ap_mpm_pod_open(pool, &cli_to_pe_pod);
    if (rv != APR_SUCCESS) {
        (void)ap_mpm_pod_close(pe_to_cli_pod);
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "Create cli_to_pe_pod fail!");  
        return -1;
    } 

    apr_pool_cleanup_register(pool, NULL, ap_sync_destroy, apr_pool_cleanup_null);
    
    return 0;
}

/**********************************************************
 * �ڰ�������غ���
 **********************************************************/

/**
 * ����ڰ�����.
 */
AP_DECLARE(int) ap_access_list_deploy(int lst)
{
    if ((lst < IP_BLACK) || (lst > ALL_LIST)) {
        return DECLINED;
    }

    return ap_config_update(NULL, 0, 0, GRACEFUL);
}

AP_DECLARE(int) ap_dyn_blacklist_clear(int lst, const char *addr)
{
    apr_status_t rv;
    apr_pool_t *ptemp; /* Pool for temporary config stuff, reset often */
    ap_directive_t *newdir;
    ap_directive_t *current;
    ap_directive_t *conftree;
    char cmd_name[NAME_LEN_MAX];

    if ((lst < IP_BLACK) || (lst > ALL_LIST)) {
        return DECLINED;
    }

    if (!addr) {
        return DECLINED;
    }

    rv = OK;

    switch (lst) {
    case IP_BLACK:
        strcpy(cmd_name, "SecBListDynCliIP");
        break;
    case URL_BLACK:
        strcpy(cmd_name, "SecBListDynRefURL");
        break;
    default:
        return DECLINED;
    }

    /* ���ô�������е���ʱ���ݷ�����ʱ�ڴ������ */
    apr_pool_create(&ptemp, pconf);
    apr_pool_tag(ptemp, "ptemp");

    /* ÿ�����������Խ���һ����ʱ�������� */
    conftree = NULL;
    current = NULL;

    newdir = (ap_directive_t *)apr_pcalloc(ptemp, sizeof(ap_directive_t));
    newdir->filename = "dynamic black list";
    newdir->line_num = (current == NULL) ? 1 : (current->line_num + 1);
    newdir->directive = cmd_name;
    newdir->args = apr_psprintf(ptemp, "Del %s", addr);
    current = ap_add_node(&conftree, current, newdir, 0);
    conftree = current;

    /* scan through all directives, print each one */
    for (current = conftree; current != NULL; current = current->next) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                     "%s: line[%d] directive[%s] args[%s]",
                     current->filename, current->line_num,
                     current->directive, current->args);
    }

    /* ����ڰ�������ָ�� */
    if (conftree) {
        rv = ap_process_config_tree(ap_main_server, conftree,
                                    pconf, ptemp);
    }

    apr_pool_destroy(ptemp);

    return rv;
}

/**********************************************************
 * ��ȫ������غ���
 **********************************************************/

static apr_status_t reload_secpolicy_list_hash(void *baton)
{
    secpolicy_list_hash = NULL;
    return APR_SUCCESS;
}

static void rebuild_secpolicy_list_hash(apr_pool_t *p)
{
    secpolicy_list_hash = apr_hash_make(p);

    apr_pool_cleanup_register(p, NULL, reload_secpolicy_list_hash,
                              apr_pool_cleanup_null);
}

AP_DECLARE(sec_policy_list_t *) ap_secpolicy_list_find(const char *name)
{
    sec_policy_list_t *scpl;
    char strbuf[STR_LEN_MAX];

    if (!name) {
        return NULL;
    }

    strcpy(strbuf, name);
    ap_str_tolower(strbuf);

    scpl = NULL;
    scpl = apr_hash_get(secpolicy_list_hash, strbuf, APR_HASH_KEY_STRING);

    return scpl;
}

AP_DECLARE(int) ap_secpolicy_list_add(const char *name)
{
    apr_pool_t *tpool;
    apr_pool_t *pscpl;
    sec_policy_list_t *scpl;
    char *key;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                 "add security policy list %s", name);

    if (!name) {
        return DECLINED;
    }

    if (!secpolicy_list_hash) {
        rebuild_secpolicy_list_hash(pcli);
    }

    scpl = ap_secpolicy_list_find(name);
    if (scpl != NULL) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                     "found security policy list %s", scpl->name);
        return OK;
    }

    /* ������ȫ���������ӳأ���ȫ��������洢�ڵ����ĳ����� */
    tpool = apr_hash_pool_get(secpolicy_list_hash);
    apr_pool_create(&pscpl, tpool);
    apr_pool_tag(pscpl, "pscpl");

    scpl = (sec_policy_list_t *)apr_pcalloc(pscpl, sizeof(sec_policy_list_t));
    scpl->pool = pscpl;
    strncpy(scpl->name, name, NAME_LEN_MAX);
    scpl->head.name[0] = '\0';
    scpl->head.ser_policy = NULL;
    scpl->head.prev = NULL;
    scpl->head.next = NULL;

    /* �����������ָ�뵽��ϣ�� */
    key = apr_pstrdup(pscpl, name);
    ap_str_tolower(key);
    apr_hash_set(secpolicy_list_hash, key, APR_HASH_KEY_STRING, scpl);

    return OK;
}

AP_DECLARE(int) ap_secpolicy_list_del(const char *name)
{
    sec_policy_list_t *scpl;
    char strbuf[STR_LEN_MAX];

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                 "delete security policy list %s", name);

    if (!name) {
        return DECLINED;
    }

    if (!secpolicy_list_hash) {
        return DECLINED;
    }

    strcpy(strbuf, name);
    ap_str_tolower(strbuf);
    scpl = apr_hash_get(secpolicy_list_hash, strbuf, APR_HASH_KEY_STRING);
    if (scpl != NULL) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                     "delete security policy list %s", scpl->name);
        if (scpl->head.next) {
            /* ��ȫ������������������Ե����ӽڵ㲻Ϊ��ʱ��������ɾ�� */
            return DECLINED;
        }
        else {
            apr_hash_set(secpolicy_list_hash, strbuf, APR_HASH_KEY_STRING, 0);
            apr_pool_destroy(scpl->pool);
        }
    }

    return OK;
}

static int secpolicy_server_link(const char *name, server_policy_t *sp)
{
    sec_policy_t *scp;
    sec_policy_list_t *scpl;
    char strbuf[STR_LEN_MAX];

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                 "add a security policy %s and server policy %s linking", name, sp->name);

    if (!name || !sp) {
        return DECLINED;
    }

    if (!secpolicy_list_hash) {
        return DECLINED;
    }

    strcpy(strbuf, name);
    ap_str_tolower(strbuf);
    scpl = ap_secpolicy_list_find(strbuf);
    if (!scpl) {
        return DECLINED;
    }

    /* �鿴��ǰ�Ƿ������� */
    for (scp = scpl->head.next; scp; scp = scp->next) {
        if ((sp == scp->ser_policy) && (sp->sec_policy == scp)) {
            return OK;
        }
    }

    /* �����µ����ӣ�����˫�������ͷ������ */
    scp = (sec_policy_t *)apr_pcalloc(scpl->pool, sizeof(sec_policy_t));
    strcpy(scp->name, name);
    scp->ser_policy = sp;
    sp->sec_policy = scp;
    scp->next = scpl->head.next;
    scpl->head.next = scp;
    scp->prev = &(scpl->head);
    if (scp->next) {
        scp->next->prev = scp;
    }

    return OK;
}

static int secpolicy_server_unlink(server_policy_t *sp)
{
    if (!sp || !(sp->sec_policy)) {
        return DECLINED;
    }

    sp->sec_policy->prev->next = sp->sec_policy->next;
    if (sp->sec_policy->next) {
        sp->sec_policy->next->prev = sp->sec_policy->prev;
    }
    sp->sec_policy = NULL;

    return OK;
}

AP_DECLARE(int) ap_secpolicy_deploy(const char *name)
{
    sec_policy_list_t *scpl;

    scpl = ap_secpolicy_list_find(name);
    if (scpl == NULL) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                     "secpolicy list %s is empty ", name);
        return DECLINED;
    }

    return ap_config_update(NULL, 0, 0, GRACEFUL);
}

/**********************************************************
 * ������������غ���
 **********************************************************/

static apr_status_t reload_server_policy_hash(void *baton)
{
    server_policy_hash = NULL;
    return APR_SUCCESS;
}

static void rebuild_server_policy_hash(apr_pool_t *p)
{
    server_policy_hash = apr_hash_make(p);

    apr_pool_cleanup_register(p, NULL, reload_server_policy_hash,
                              apr_pool_cleanup_null);
}

static void delete_directory(char *dir_path)
{
    char *argv[MAX_ARGV_NUM] = { NULL };
    apr_pool_t *ptemp;
    int i;

    /* �����ӳ� */
    apr_pool_create(&ptemp, pcli);
    apr_pool_tag(ptemp, "ptemp");

    i = 0;
    argv[i++] = apr_psprintf(ptemp, "%s", "/bin/rm");
    argv[i++] = apr_psprintf(ptemp, "-rf");
    argv[i++] = apr_psprintf(ptemp, "%s", dir_path);

    ap_exec_shell("/bin/rm", argv);
    
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                     "/bin/rm -rf %s", dir_path);
    
    apr_pool_destroy(ptemp);
}

/**
 * �����������Ե������Ƿ�����
 * ͨ������OK�����򷵻�DECLINED
 */
static int exam_server_policy(server_policy_t *sp)
{
    virt_host_t *vhost;
    sec_policy_list_t *scpl;

    /* ��ȫ���Բ�����Ϊ�� */
    if (!sp->sec_policy) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                     "security policy is NULL");
        return DECLINED;
    } else {
        /* ���Ұ�ȫ���ԣ��Ҳ����򷵻ش��� */
        scpl = ap_secpolicy_list_find(sp->sec_policy->name);
        if (scpl == NULL) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                         "can not find sec_policy %s", sp->sec_policy->name);
            return DECLINED;
        }
    }

    if (sp->is_default) {
        return OK;
    } else if (!sp->virt_host) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                     "virt server is NULL");
        return DECLINED;
    }

    for (vhost = sp->virt_host->next; vhost; vhost = vhost->next) {
        /* ����������ַ����Ϊ�գ������˿�Ҳ����ͬʱΪ�� */
        if (!vhost->ipaddr.s_addr || (!vhost->phttp && !vhost->phttps)) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                         "ipaddr %d, phttp %d and phttps %d",
                         vhost->ipaddr.s_addr,
                         vhost->phttp,
                         vhost->phttps);
            return DECLINED;
        }
    }

    /* û���κ�����������ַ */
    if (vhost == sp->virt_host->next) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                     "virt server address is NULL");
        return DECLINED;
    }

    if (sp->work_mode == WORK_REVERSE) {
        /* ���ԭʼ������ַ�Ͷ˿ڲ���Ϊ�� */
        if (!sp->orig_host->ipaddr.s_addr) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                         "orig_host address %d", sp->orig_host->ipaddr.s_addr);
            return DECLINED;
        }
    }

    return OK;
}

/**
 * �������������
 * ����ʱ�ķ��������Ա��浽pcli���ڴ�أ��������ϣ��
 */
static int save_server_policy(server_policy_t *sp)
{
    apr_pool_t *tpool;
    apr_pool_t *pserver;
    apr_pool_t *pvirt;
    apr_pool_t *porig;
    virt_host_t *vhost;
    virt_host_t *vaddr;
    server_policy_t *serplcy;
    char *key;
    char strbuf[STR_LEN_MAX];
    int rv;

    if (!server_policy_hash) {
        rebuild_server_policy_hash(pcli);
    }

    /* ��鵱ǰ�����������Ƿ��� */
    strcpy(strbuf, sp->name);
    ap_str_tolower(strbuf);
    serplcy = apr_hash_get(server_policy_hash, strbuf, APR_HASH_KEY_STRING);
    if (serplcy != NULL) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                     "found server-policy %s", serplcy->name);
        /* �����µİ�ȫ���Ժ���Ҫ��ԭ�ȵİ�ȫ���Ը���� */
        secpolicy_server_unlink(serplcy);
        apr_hash_set(server_policy_hash, strbuf, APR_HASH_KEY_STRING, 0);
        apr_pool_destroy(serplcy->pool);
    }

    /* ���������������ӳأ�ÿ�����������Դ洢�ڵ����ĳ����� */
    tpool = apr_hash_pool_get(server_policy_hash);
    apr_pool_create(&pserver, tpool);
    apr_pool_tag(pserver, "pserver");

    serplcy = (server_policy_t *)apr_pcalloc(pserver, sizeof(server_policy_t));
    serplcy->pool = pserver;
    serplcy->pvhost = NULL;
    strcpy(serplcy->name, sp->name);
    serplcy->server = NULL;
    serplcy->is_default = sp->is_default;
    serplcy->work_mode = sp->work_mode;
    serplcy->opt_flags = sp->opt_flags;
    /* ���������ļ��� */
    serplcy->cache_root = apr_pstrdup(pserver, sp->cache_root);
    if (access(sp->cache_root, F_OK)) {
        rv = mkdir(sp->cache_root, 0755);
        if (rv != 0) {
            apr_pool_destroy(pserver);
            return DECLINED;
        }
    }
    
    secpolicy_server_link(sp->sec_policy->name, serplcy);

    for (vhost = sp->virt_host; vhost; vhost = vhost->next) {
        apr_pool_create(&pvirt, serplcy->pool);
        apr_pool_tag(pvirt, "pvirt");
        vaddr = (virt_host_t *)apr_pcalloc(pvirt, sizeof(virt_host_t));
        vaddr->pool = pvirt;
        vaddr->ipaddr.s_addr = vhost->ipaddr.s_addr;
        strncpy(vaddr->server_name, vhost->server_name, DOMAIN_LEN_MAX);
        vaddr->phttp = vhost->phttp;
        vaddr->phttps = vhost->phttps;

        if (serplcy->virt_host) {
            vaddr->next = serplcy->virt_host->next;
            serplcy->virt_host->next = vaddr;
            vaddr->prev = serplcy->virt_host;
            if (vaddr->next) {
                vaddr->next->prev = vaddr;
            }
        } else {
            /* ��һ���ڵ�������ͷ */
            serplcy->virt_host = vaddr;
        }
    }

    if (sp->work_mode == WORK_REVERSE) {
        apr_pool_create(&porig, serplcy->pool);
        apr_pool_tag(porig, "porig");
        serplcy->orig_host = (orig_host_t *)apr_pcalloc(porig, sizeof(orig_host_t));
        serplcy->orig_host->pool = porig;
        serplcy->orig_host->ipaddr.s_addr = sp->orig_host->ipaddr.s_addr;
        serplcy->orig_host->proto = sp->orig_host->proto;
        serplcy->orig_host->port = sp->orig_host->port;
    }
    serplcy->engine = sp->engine;
    serplcy->audit_set = sp->audit_set;
    serplcy->audit_log = sp->audit_log;
    serplcy->atlog_lev = sp->atlog_lev;

    /* �������������ָ�뵽��ϣ�� */
    key = apr_pstrdup(pserver, sp->name);
    ap_str_tolower(key);
    apr_hash_set(server_policy_hash, key, APR_HASH_KEY_STRING, serplcy);

    return OK;
}

/**
 * �������������
 */
AP_DECLARE(int) ap_server_policy_deploy(server_policy_t *sp)
{
    if (!sp) {
        return DECLINED;
    }

    /* ���Ҫ����ķ��������Ե������� */
    if (exam_server_policy(sp)) {
        return DECLINED;
    }
    
    /* ��ʽ����µķ��������Ի��滻��ǰ�ķ��������� */
    if (save_server_policy(sp)) {
        return DECLINED;
    }

    return ap_config_update(NULL, 0, 0, GRACEFUL);
}

/**
 * ɾ������������
 * �ҵ����������Խڵ㣬ɾ����ϣ����ڲ��ͷ��ڴ�ء�
 */
AP_DECLARE(int) ap_server_policy_remove(const char *name)
{
    server_policy_t *sp;
    char strbuf[STR_LEN_MAX];

    if (!name) {
        return DECLINED;
    }

    if (!server_policy_hash) {
        return DECLINED;
    }

    strcpy(strbuf, name);
    ap_str_tolower(strbuf);
    sp = apr_hash_get(server_policy_hash, strbuf, APR_HASH_KEY_STRING);
    if (sp == NULL) {
        return DONE;
    }

    /* ɾ�������ļ��� */
    delete_directory(sp->cache_root);
    secpolicy_server_unlink(sp);
    
    apr_hash_set(server_policy_hash, strbuf, APR_HASH_KEY_STRING, 0);
    apr_pool_destroy(sp->pool);

    return ap_config_update(NULL, 0, 0, GRACEFUL);
}

/**
 * ������еķ���������
 */
AP_DECLARE(int) ap_server_policy_clear(void)
{
    char *key;
    apr_ssize_t klen;
    apr_hash_index_t *hi;
    server_policy_t *sp;

    if (!server_policy_hash) {
        return OK;
    }
    if (apr_hash_count(server_policy_hash) < 1) {
        return OK;
    }

    for (hi = apr_hash_first(NULL, server_policy_hash); hi; hi = apr_hash_next(hi)) {
        apr_hash_this(hi, (void *)&key, &klen, (void *)&sp);
               
        apr_hash_set(server_policy_hash, key, klen, NULL);
        if (sp) {
            /* ɾ�������ļ��� */
            delete_directory(sp->cache_root);
            secpolicy_server_unlink(sp);
            apr_pool_destroy(sp->pool);
        }
    }

    return OK;
}

/**
 * ����Ĭ�Ϸ���������
 */
AP_DECLARE(int) ap_create_default_serpolicy(void)
{
    apr_pool_t *tpool;
    apr_pool_t *pserver;
    server_policy_t *serplcy;
    char *key;
    int rv;

    if (!server_policy_hash) {
        rebuild_server_policy_hash(pcli);
    }

    /* ����Ĭ�Ϸ����������ӳأ����������Դ洢�ڵ����ĳ����� */
    tpool = apr_hash_pool_get(server_policy_hash);
    apr_pool_create(&pserver, tpool);
    apr_pool_tag(pserver, "pserver");

    serplcy = (server_policy_t *)apr_pcalloc(pserver, sizeof(server_policy_t));
    serplcy->pool = pserver;
    serplcy->pvhost = NULL;
    serplcy->server = NULL;
    serplcy->work_mode = ap_work_mode;
    serplcy->is_default = 1;
    serplcy->engine = BLOCK_ON;
    BitSet(serplcy->audit_log, ATTACK_LOG);
    BitSet(serplcy->audit_set, ATTACK_LOG);
    BitSet(serplcy->audit_log, ACCESS_LOG);
    BitSet(serplcy->audit_set, ACCESS_LOG);   
    serplcy->atlog_lev = DEFAULT_ATTACK_LEVEL; 
    strcpy(serplcy->name, "default");
    BitSet(serplcy->opt_flags, ADVANCE_FLAG);
    BitSet(serplcy->opt_flags, ENGINE_FLAG);
    
    rv = secpolicy_server_link("default", serplcy);
    if (rv != OK) {
        apr_pool_destroy(pserver);
        return DECLINED;
    }

    serplcy->cache_root = ap_server_root_relative(pserver, "cache/default");
    if (access(serplcy->cache_root, F_OK)) {
        rv = mkdir(serplcy->cache_root, 0755);
        if (rv != 0) {
            apr_pool_destroy(pserver);
            return DECLINED;   
        }
    }

    /* �������������ָ�뵽��ϣ�� */
    key = apr_pstrdup(pserver, "default");
    ap_str_tolower(key);
    apr_hash_set(server_policy_hash, key, APR_HASH_KEY_STRING, serplcy);

    return OK;
}

/**
 * �ı�Ĭ�Ϸ��������ԵĹ���ģʽ
 */
AP_DECLARE(int) ap_change_default_policy_workmode(ap_proxy_mode_t newmode)
{
    server_policy_t *serplcy;

    if (!server_policy_hash) {
        return DECLINED;
    }

    serplcy = apr_hash_get(server_policy_hash, "default", APR_HASH_KEY_STRING);
    if (serplcy == NULL) {
        return DECLINED;
    }
    
    serplcy->work_mode = newmode;

    return OK;
}

/**********************************************************
 * �����ĺ���
 **********************************************************/
AP_DECLARE(enum audit_flags) ap_get_log_type(char *type)
{
    if (!strcmp(type, "access_log")) {
        return ACCESS_LOG;
    } else if (!strcmp(type, "attack_log")) {
        return ATTACK_LOG;
    } else if (!strcmp(type, "admin_log")) {
        return ADMIN_LOG;
    } else if (!strcmp(type, "all")) {
        return ALL_LOG;
    } else {
        return UNKNOWN_LOG;
    }
}

static int engine_restart(ap_update_mode_t upmode)
{
    apr_status_t rv;
    pid_t curpid;
    int mpm_state;

    rv = ap_mpm_query(AP_MPMQ_MPM_STATE, &mpm_state);
    if (rv == APR_SUCCESS) {
        /* ��ǰ��MPMû������ */
        if (mpm_state == AP_MPMQ_STARTING) {
            return OK;
        }
    }

    curpid = getpid();
    if (upmode == SRV_TERM) {
        if (kill(curpid, SIGTERM) < 0) {
            ap_log_error(APLOG_MARK, APLOG_STARTUP, errno, NULL,
                         "Sending SIGTERM signal to server fail");
            return DECLINED;
        } 

        return DONE;
    } else {
        //if (kill(curpid, AP_SIG_GRACEFUL) < 0) {   
        if (kill(curpid, SIGHUP) < 0) { /* ���÷�����������ʽ */
            ap_log_error(APLOG_MARK, APLOG_STARTUP, errno, NULL,
                         "Sending graceful signal to server fail");
            return DECLINED;
        }

        /* �ȴ��ӽ����˳���� */
        rv = ap_mpm_pod_wait(pe_to_cli_pod);
        if (rv != AP_PE_MAIN_LOOP_OK) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "Wait message ap_pe_main_loop_ok fail!");
            return DECLINED;               
        }  
    }

    return OK;
}

/**
 * �������������
 */
AP_DECLARE(int) ap_config_update(cparser_context_t *context, ap_proxy_mode_t new_mode, 
                 int workmode_switching, ap_update_mode_t upmode)
{
    int rv;
    apr_time_t start_time;
    apr_time_t stop_time;

    /* ��ʼ��ʱ�������� */
    start_time = apr_time_now();

    /* ��������״̬���������� */
    rv = engine_restart(upmode);
    if (rv == DECLINED) {
        return DECLINED;
    } else if (rv == DONE) {
        return OK;
    }

    /* �л�����ģʽ */
    if (workmode_switching) {
        rv = workemode_change_prepare(context, new_mode);
        if (rv != OK) { 
            return DECLINED;
        }
    }
    
    /* ����ģʽ��ÿ��������Ҫ�رռ����˿� */
    if (ap_work_mode == WORK_OFFLINE) {
        ap_close_listeners();
    }
    
    rv = ap_mpm_pod_signal(cli_to_pe_pod, AP_CLI_CLEAR_OK);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "Send message ap_cli_clear_ok fail.");
        return DECLINED; 
    } 
   
    rv = ap_mpm_pod_wait(pe_to_cli_pod);
    if (rv != AP_PE_PRE_CONF_OK) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "Wait message ap_pe_pre_conf_ok fail.");
        return DECLINED;               
    }  

    /* ����ģʽ���� */
    if (ap_work_mode == WORK_OFFLINE) {
        rv = ap_offline_configure();
        if (rv != OK) {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, 
                "Offline configure fail.");
            (void)ap_mpm_pod_signal(cli_to_pe_pod, AP_CLI_CONF_FAIL); 
            if (rv == DECLINED) {
                return DECLINED;
            } else if (rv == DONE) {
                return OK;
            }
        }
    }

    /* ��������˿� */
    rv = ap_listen_ports_alloc();
    if (rv != OK) {
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, 
            "Alloc listen port fail.");
        (void)ap_mpm_pod_signal(cli_to_pe_pod, AP_CLI_CONF_FAIL);  
        if (rv == DECLINED) {
            return DECLINED;
        } else if (rv == DONE) {
            return OK;
        }     
    }
    
    /* �����Ŷ˿� */
    rv = ap_bridge_port_alloc();
    if (rv != OK) {
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, 
            "Alloc bridge port fail.");
        (void)ap_mpm_pod_signal(cli_to_pe_pod, AP_CLI_CONF_FAIL); 
        return DECLINED;
    }
    
    /* ����ڰ����� */
    rv = ap_access_list_handle(ALL_LIST);
    if (rv != OK) {
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, 
            "Process black white list fail.");
        (void)ap_mpm_pod_signal(cli_to_pe_pod, AP_CLI_CONF_FAIL); 
        return DECLINED;
    }

    /* ������������� */
    rv = ap_server_policy_walk(server_policy_hash);
    if (rv != OK) {
         ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, 
            "Process server policy fail.");
         (void)ap_mpm_pod_signal(cli_to_pe_pod, AP_CLI_CONF_FAIL);  
         if (rv == DECLINED) {
             return DECLINED;
         } else if (rv == DONE) {
             return OK;
         }     
    }

    /* CLI�����Ѿ�׼�����ˣ�֪ͨpe׼�������ӽ��� */
    rv = ap_mpm_pod_signal(cli_to_pe_pod, AP_CLI_CONF_OK);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "Send message(AP_CLI_CONF_OK) fail.");        
        return DECLINED; 
    }

    stop_time = apr_time_now();
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_main_server,
                 "cost %lld(us) to update config", (unsigned long long)(stop_time - start_time));

    /* �ȴ�pe�ӽ��̴������ */
    rv = ap_mpm_pod_wait(pe_to_cli_pod);
    if (rv != AP_PE_FORK_OK) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "Send message(AP_PE_FORK_OK) fail.");        
        return DECLINED;               
    }        

    return OK;
}

/* ��ѯ�ӿ��Ƿ���� */
AP_DECLARE(int) ap_query_interface_exist(char *device_name)
{
    int s;
    int err;
    struct ifreq ifr;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        return DECLINED;
    }
    
    /* IFNAMSIZ��СΪ16 */
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", device_name);
    err = ioctl(s, SIOCGIFFLAGS, &ifr);
    if(err){
        close(s);
        return DECLINED;
    } else {
        close(s);
        return OK;
    }
}

AP_DECLARE(int) ap_exec_shell(char *shell_name, char **argv) 
{
    pid_t pid;
    int rv;
    int status;
   
    pid = fork();
    if (pid == 0) {
        rv = execv(shell_name, argv);
        if (rv == -1) {
            return DECLINED;
        }
    } else if (pid < 0) {
        return DECLINED;
    }

    if (waitpid(pid, &status, 0) != pid) {
        return DECLINED;
    }

    /* ǰһ���жϱ�ʾexecv����״̬�룬��һ����ʾ�ű��ĳ�����ֵ */
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        return DECLINED;
    }

    return OK;
}

AP_DECLARE(int) ap_check_mask_validation(apr_int32_t mask)
{
    if (mask == 0 || ((~mask & (~mask + 1)) != 0)) {
        return DECLINED;
    }
    
    return OK;
}

AP_DECLARE(int) ap_cli_common_init(process_rec *process)
{
    int rv;

    if (!process) {
        return DECLINED;
    }

    apr_pool_create(&pcli, process->pool);
    apr_pool_tag(pcli, "pcli");
    pconf = process->pconf;

    rebuild_secpolicy_list_hash(pcli);
    rebuild_server_policy_hash(pcli);

    /* ��ʼ��cli������ʾ�� */
    snprintf(g_cli_prompt, NAME_LEN_MAX, "%s", DEFAULT_CLI_PROMPT);
    rv = ap_engine_config_init(pcli, pconf);
    if (rv != OK) {
        return DECLINED;
    }

    rv = convert_init(pcli, pconf);
    if (rv != OK) {
        return DECLINED;
    }

    rv = global_init(pcli);
    if (rv != OK) {
        return DECLINED;
    }
    
    rv = security_policy_init(pcli);
    if (rv != OK) {
        return DECLINED;
    }

    /* ����������Ҫ�ڰ�ȫ����֮���ʼ�� */
    rv = server_policy_init(pcli, pconf);
    if (rv != OK) {
        return DECLINED;
    }

    rv = show_accesslog_init(pcli);
    if (rv != OK) {
        return DECLINED;
    }

    rv = blackwhite_list_init(pcli);
    if (rv != OK) {
        return DECLINED;
    }

    rv = show_attacklog_init(pcli);
    if (rv != OK) {
        return DECLINED;
    }

    rv = show_adminlog_init(pcli);
    if (rv != OK) {
        return DECLINED;
    }

    rv = interface_init(pcli);
    if (rv != OK) {
        return DECLINED;
    }

    return OK;
}

static void admin_log_io_term(apr_pool_t *ptemp, admin_log_t *adminlog, char *action)
{
    struct utmp *u;
    char tname[32] = { 0 };
  
    strncpy(tname, ttyname(0), 32);
    adminlog->time = apr_time_now() / APR_USEC_PER_SEC;
    while ((u = getutent()) != NULL) {
        if(u->ut_type == USER_PROCESS) {
            if (!strcmp(u->ut_line, tname + 5)) { /* ȥ����ǰ���/dev/����Ϊu->ut_line�г�ȥ��/dev/ */
                adminlog->admin_name = apr_pstrdup(ptemp, u->ut_user);
                
                if (!strcmp(u->ut_host, "")) {
                    adminlog->ip = apr_pstrdup(ptemp, "localhost");
                } else {
                    adminlog->ip = apr_pstrdup(ptemp, u->ut_host);
                }
                
                adminlog->tty = apr_pstrdup(ptemp, tname);
                break;
            }
        }
    }

    endutent();
    setutent();    
    adminlog->action = apr_pstrdup(ptemp, action);
}

static void admin_log_io_socket_unix(cparser_context_t *context, admin_log_t *adminlog, char *action)
{
    char *usrname, *ip, *tty;

    /* ʱ�� */
    adminlog->time = apr_time_now() / APR_USEC_PER_SEC;
    /* �û��� */
    usrname = cparser_get_client_username(context->parser);
    adminlog->admin_name = usrname ? usrname : "";
    /* IP */
    ip = cparser_get_client_ip(context->parser);
    adminlog->ip = ip ? ip : "";
    /* �ն��� */
    tty = cparser_get_client_terminal(context->parser);
    adminlog->tty = tty ? tty : "";
    /* ���� */
    adminlog->action = action;   
}

AP_DECLARE(void) admin_log_process(cparser_context_t *context, char *action)
{
    admin_log_t adminlog;
    apr_pool_t *ptemp;
    char buf[1024] = { 0 };
    int rv;

    if (!context || !action) {
        return ;
    }

    memset(&adminlog, 0, sizeof(admin_log_t));    
    switch (context->parser->cfg.io_type) {
    case IO_SOCKET_UNIX: 
        if (!cparser_client_connecting(context->parser)) {
            /* �޿ͻ������ӵ�ʱ�򲻼�¼������־ */
            return;
        } 
        admin_log_io_socket_unix(context, &adminlog, action);
        snprintf(buf, 1024, "insert into admin_log_table values(%llu, '%s', '%s', '%s', '%s', '%s')", 
            (unsigned long long)adminlog.time, adminlog.ip, adminlog.admin_name, 
            adminlog.tty, adminlog.action, "pe");
        break;
     case IO_TERM:
        apr_pool_create(&ptemp, pcli);
        admin_log_io_term(ptemp, &adminlog, action);
        snprintf(buf, 1024, "insert into admin_log_table values(%llu, '%s', '%s', '%s', '%s', '%s')", 
            (unsigned long long)adminlog.time, adminlog.ip, adminlog.admin_name, 
            adminlog.tty, adminlog.action, "pe");
        apr_pool_destroy(ptemp);
        break;
     default:
        return;      
    }
  
    /* ���͵���־������ */
    rv = log_send(buf, 1);
    if (rv < 0) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "Send admin log to log server failed.%d", 
            rv);
    }
    
    return;
}

