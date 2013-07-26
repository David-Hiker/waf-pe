/*
 * $Id: bwlist_common.h 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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

#ifndef _BWLIST_COMMON_H_
#define _BWLIST_COMMON_H_

#include <semaphore.h>
#include "apr_thread_cond.h"
#include "apr_thread_proc.h"

#define BLIST_MIN_TIMEOUT              1
#define BLIST_MAX_TIMEOUT              7200
#define BLIST_ONE_MINUTE               1
#define DYN_BLIST_DEFAULT_TIMEOUT      60    /* ��̬������Ĭ�ϳ�ʱʱ��60���� */
#define TIME_STRING_LEN                30
#define BLIST_SEC_PER_MINUTE           60
#define BLIST_USEC_PER_SEC             1000000
#define BLIST_TIME_TO_USEC(minutes)    \
           ((minutes) * ((apr_time_t)BLIST_SEC_PER_MINUTE) * ((apr_time_t)BLIST_USEC_PER_SEC))

/* ��̬��������ʱ����ɾ������ */
typedef void(*fn_dyn_blist_t)(void);

/* ��������̬���show�ṹ */
typedef struct dyn_blist_show_str_s {
    char *str;
    char *start_time;
    char *end_time;
} dyn_blist_show_str_t;

/* ��������ʱ���ṹ */
typedef struct blist_timer_s {
    apr_pool_t *pool;
    fn_dyn_blist_t fn;
    int terminated;
    sem_t sem_timeout;
    apr_thread_t *thread;
} blist_timer_t;

/* ���ٶ�ʱ����Դ */
extern apr_status_t dyn_blist_timer_destroy(void *data);

/* ��ʼһ���̶߳�ʱ�� */
extern apr_status_t dyn_blist_timer_start(apr_pool_t *pool, blist_timer_t *timer);

/* ֹͣһ���̶߳�ʱ�� */
extern apr_status_t dyn_blist_timer_stop(blist_timer_t *timer);

/* ������ʱ���߳� */
extern blist_timer_t *dyn_blist_timer_create(apr_pool_t *pool, fn_dyn_blist_t fn);

#endif  /* _BWLIST_COMMON_H_ */

