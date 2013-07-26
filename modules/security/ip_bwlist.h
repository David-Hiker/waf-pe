/*
 * $Id: ip_bwlist.h 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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

 
#ifndef _IP_BWLIST_H_
#define _IP_BWLIST_H_

#include "apr_global_mutex.h"
#include "apr_thread_mutex.h"
#include "apr_hash.h"
#include "apr_ring.h"
#include "apr_time.h"

#define IPADDR_LEN                   32
#define IP_DYN_BLIST_ENTRY_NUM       128               /* ��̬�������ɴ�ŵĽ����Ŀ */
#define IP_BWLIST_ENTRY_NUM          128               /* IP�ڰ������ɴ�ŵĽ����Ŀ */
#define IP_DYN_BLIST_FILENAME        "/tmp/ip_blist.txt"
#define IP_BWLIST_ADD                "Add"
#define IP_BWLIST_DEL                "Del"
#define IP_BLIST_ATTACK              1U

/* ��̬IP�ṹ */
typedef struct cli_ip_s {
    char *ip;                                         /* ip������ */
    apr_thread_mutex_t *mutex;                        /* �̻߳��޸�hitcount��last_logtime����Ҫ�� */
    apr_time_t  last_logtime;                         /* �����һ��log��¼ʱ�� */
    int  hitcount;                                    /* һ����֮�ڵ����д��� */
} cli_ip_t;

/* ��̬IP�ṹ */
typedef struct dyn_cli_ip_s {
    APR_RING_ENTRY(dyn_cli_ip_s) link;    
    char cli_ip[IPADDR_LEN];                           /* �ͻ��˶�̬ip */
    apr_time_t start_time;                             /* ip������ʱ�� */
    apr_time_t end_time;                               /* ip����ʱ�� */
    apr_time_t last_logtime;                           /* �����һ��log��¼ʱ�� */
    int hitcount;                                      /* һ����֮�ڵ����д��� */
} dyn_cli_ip_t;

/* �����ڴ������ */
typedef struct shm_info_mg_s {
    apr_global_mutex_t *shm_mutex;                         /* �����ڴ��� */
    int free_num;                                          /* ��ǰ���н����Ŀ */
    APR_RING_HEAD(ip_free_head, dyn_cli_ip_s) free_list;   /* ���н������ */
    APR_RING_HEAD(ip_used_head, dyn_cli_ip_s) used_list;   /* �ͻ��˶�̬IP������� */
} shm_info_mg_t;

enum ip_wlist_type_s {
    IP_WLIST_CLI_IP,
    IP_WLIST_SERV_IP,
    IP_WLIST_HOSTNAME,
    IP_WLIST_NUM
};

enum ip_blist_type_s {
    IP_BLIST_CLI_IP,
    IP_BLIST_EXCPT_IP,
    IP_BLIST_NUM
};

/**
 * ip_dyn_blist_add - ���һ��ip���ͻ��˶�̬ip������
 * @cli_ip: �ͻ���ip
 * 
 * ����-1��ʾ���ʧ�ܣ�����APR_SUCCESS��ʾ��ӳɹ���
 */
extern int ip_dyn_blist_add(const char *cli_ip);

/**
 * ip_bwlist_proccess - ip�ڰ���������
 * @msr: ������������
 *
 * ���������ѭ����������ԭ�� 
 *
 * ����ֵ: ������-1��ƥ�䵽����������HTTP_FORBDDEN,ƥ�䵽��������ʲô��ûƥ�䵽����OK
 */
extern int ip_bwlist_proccess(modsec_rec *msr);

/**
 * ip_bwlist_create - ����ip�ڰ�����
 * @pool: �ڴ��
 *
 * ����ֵ: ��ʼ���ɹ�����APR_SUCCESS,ʧ�ܷ���-1
 */
extern int ip_bwlist_create(apr_pool_t *pool);

/**
 * ip_dyn_blist_create - ������̬ip�������Ͷ�̬��������ʱ��
 * @pool: �ڴ��
 *
 * ����ֵ: ��ʼ���ɹ�����APR_SUCCESS,ʧ�ܷ���-1
 */
extern int ip_dyn_blist_create(apr_pool_t *pool);

/*
 * ip_dyn_blist_get - ��ȡ��̬ip����������
 * @result: ��������
 *
 * ����ֵ: �ɹ�����APR_SUCCESS,ʧ�ܷ���-1
 */
extern int ip_dyn_blist_get(apr_array_header_t **result);

/**
 * cmd_blist_cli_ip - �ͻ���ip�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @cli_ip: ip����
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_blist_cli_ip(cmd_parms *cmd, void *_dcfg, const char *action, 
    const char *cli_ip);

/**
 * cmd_blist_dyn_ip_timeout - �ͻ��˶�̬ip�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @strtimeout: ʱ���ַ���
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_blist_dyn_ip_timeout(cmd_parms *cmd, void *_dcfg, const char *strtimeout);

/**
 * cmd_blist_dyn_ip_except - �ͻ��˶�̬����ip�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @excpt_ip: ip����
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_blist_dyn_ip_except(cmd_parms *cmd, void *_dcfg, const char *action, 
    const char *excpt_ip);

/**
 * cmd_blist_dyn_cli_ip - �ͻ��˶�̬ip�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @cli_ip: ip����
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_blist_dyn_cli_ip(cmd_parms *cmd, void *_dcfg, const char *action, 
    const char *cli_ip);

/**
 * cmd_wlist_cli_ip - �ͻ���ip�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @cli_ip: ip����
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_wlist_cli_ip(cmd_parms *cmd, void *_dcfg, const char *action, 
    const char *cli_ip);

/**
 * cmd_wlist_serv_ip - ��������ip�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @serv_ip: ip����
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_wlist_serv_ip(cmd_parms *cmd, void *_dcfg, const char *action, 
    const char *serv_ip); 

/**
 * cmd_wlist_serv_host - �����������������������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @host: ��������
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_wlist_serv_host(cmd_parms *cmd, void *_dcfg, const char *action, 
    const char *host);

/**
 * cmd_wlist_serv_host - �����������������������
 * @cmd: �������йصĲ����ṹָ��
 * @p1: ip�ڰ��������أ�on/off
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_ip_bwlist(cmd_parms *cmd, void *_dcfg, const char *p1);
/**
 * ip_dyn_blist_timer_destroy - ֹͣ��ʱ���߳�
 * �޲���
 *
 * ����ֵ: ��ʼ���ɹ�����OK,ʧ�ܷ���DECLIEND
 */
extern int ip_dyn_blist_timer_destroy(void);   

#endif  /* _IP_BWLIST_H_ */

