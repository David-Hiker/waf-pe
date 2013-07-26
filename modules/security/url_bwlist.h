/*
 * $Id: url_bwlist.h 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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

 
#ifndef _URL_BWLIST_H_
#define _URL_BWLIST_H_

#include "apr_global_mutex.h"
#include "apr_thread_mutex.h"
#include "apr_hash.h"
#include "apr_ring.h"
#include "apr_time.h"

#define URL_STRING_LEN           64
#define URL_TYPE_PLAINTEXT       0
#define URL_TYPE_REGULEREXP      1
#define URL_TYPE_ALL             2
#define URL_BWLIST_ENTRY_NUM     128
#define URL_DYN_BLIST_ENTRY_NUM  128
#define URL_DYN_BLIST_FILENAME   "/tmp/url_blist.txt"
#define URL_BWLIST_ADD           "Add"
#define URL_BWLIST_DEL           "Del"
#define URL_BLIST_ATTACK         2U
   
/* ��̬URL�ṹ */
typedef struct cli_url_s {
    char *url;                            /* url�ַ��� */
    int type;                             /* url�����ͣ���ͨ�ַ�����������ʽ */
    msc_regex_t *url_regex;               /* ������������ʽ */
    apr_thread_mutex_t *mutex;            /* �̻߳��޸�hitcount��last_logtime����Ҫ�� */       
    apr_time_t  last_logtime;             /* �����һ��log��¼ʱ�� */
    int  hitcount;                        /* һ����֮�ڵ����д��� */                    
} cli_url_t;

typedef struct dyn_ref_url_s {
    APR_RING_ENTRY(dyn_ref_url_s) link;   
    char ref_url[URL_STRING_LEN];                           /* Refferrer URL */
    apr_time_t start_time;                                  /* ������ʼʱ�� */
    apr_time_t end_time;                                    /* ���ý���ʱ�� */
    apr_time_t last_logtime;                                /* �����һ��log��¼ʱ�� */
    int hitcount;                                           /* һ����֮�ڵ����д��� */                        
} dyn_ref_url_t;

/* ��̬ref url�����ڴ������ */
typedef struct dyn_url_blist_mg_s {
    apr_global_mutex_t *shm_mutex;                           /* �����ڴ��� */
    int free_num;                                            /* ��ǰ���н����Ŀ */
    APR_RING_HEAD(url_free_head, dyn_ref_url_s) free_list;   /* ���н������ */
    APR_RING_HEAD(url_used_head, dyn_ref_url_s) used_list;   /* ��̬Ref URL������� */
} dyn_url_blist_mg_t;

enum url_bwlist_type_e {
    URL_BLIST,                                               /* URL������ */
    URL_REF_BLIST_EXCPT,                                     /* Referrer URL ��̬��������� */
    URL_WLIST,                                               /* URL������ */
    URL_BWLIST_NUM
};

enum url_error_type_e {
    URL_LIST_IS_FULL = 1,
    URL_REF_IN_EXCPT,
    URL_HAS_IN_LIST,
    URL_TRUNCATE_INVALID
};

/**
 * url_bwlist_create - ����url�ڰ�����
 * @pool: �ڴ��
 *
 * ����ֵ: ��ʼ���ɹ�����APR_SUCCESS,ʧ�ܷ���-1
 */
extern int url_bwlist_create(apr_pool_t *pool);

/*
 * url_dyn_blist_get - ��ȡ��̬url����������
 * @result: ��������
 *
 * ����ֵ: ��
 */
extern int url_dyn_blist_get(apr_array_header_t **result);

/**
 * url_dyn_blist_add - ���һ��url����̬referrer url������
 * @ref_url: referrer url
 * 
 * ����-1��ʾ���ʧ�ܣ�����APR_SUCCESS��ʾ��ӳɹ���
 */
extern int url_dyn_blist_add(char *ref_url);

/**
 * url_dyn_blist_create - ������̬url������
 * @pool: �ڴ��
 *
 * ����ֵ: �ɹ�����APR_SUCCESS,ʧ�ܷ���-1
 */
extern int url_dyn_blist_create(apr_pool_t *pool);

/**
 * url_bwlist_process - url�ڰ�����ƥ�䴦��
 * @msr: �����������Ľṹ�����п��Ի��url��Ϣ�����ҿ��Լ��غڰ��������б��
 *
 * ����ֵ: �ɹ�����APR_SUCCESS,ʧ�ܷ���-1
 */
extern int url_bwlist_process(modsec_rec *msr);

/**
 * cmd_blist_url - URL�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @url_type: url���ͣ���ͨ�ַ�������������ʽ
 * @url: url
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_blist_url(cmd_parms *cmd, void *_dcfg, const char *action, 
                    const char *url_type, const char *url);

/**
 * cmd_blist_dyn_ref_url_timeout - ��̬ref url���������ʱ����
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @strtimeout: ʱ���ַ���
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_blist_dyn_ref_url_timeout(cmd_parms *cmd, void *_dcfg, 
                    const char *strtimeout);

/**
 * cmd_blist_dyn_ref_url - Referrer URL�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @ref_url: referrer url����
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_blist_dyn_ref_url(cmd_parms *cmd, void *_dcfg, const char *action, 
                    const char *ref_url);

/**
 * cmd_blist_ref_url_except - ����referrer url�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @excpt_url: url����
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_blist_ref_url_except(cmd_parms *cmd, void *_dcfg, const char *action, 
                    const char *excpt_url);

/**
 * cmd_wlist_url - url�����������
 * @cmd: �������йصĲ����ṹָ��
 * @dcfg: ���ýṹ
 * @action: �����
 * @url_type:url����
 * @url: url����
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_wlist_url(cmd_parms *cmd, void *_dcfg, const char *action, 
                    const char *url_type, const char *url);

/**
 * cmd_url_bwlist - url�ڰ�����������رպ���
 * @cmd: �������йصĲ����ṹָ��
 * @p1: url�ڰ��������أ�on/off
 *
 * ����ֵ: �ɹ�����NULL,ʧ�ܷ����ַ���
 */
extern const char *cmd_url_bwlist(cmd_parms *cmd, void *_dcfg, const char *p1);

/**
 * url_dyn_blist_timer_destroy - ֹͣ��ʱ���߳�
 * �޲���
 *
 * ����ֵ: ��ʼ���ɹ�����APR_SUCCESS,ʧ�ܷ���-1
 */
extern int url_dyn_blist_timer_destroy(void);

#endif  /* _URL_BWLIST_H_ */

