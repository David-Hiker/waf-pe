/*
 * $Id: convert_black_white.c 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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
#include "convert_private.h"

static int combined_ip_black(apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 rv, type, i, kw_num;
    char                *state, *rule, **new;
    const char          *entry;
    apr_pool_t          *tpool;
    apr_dbd_results_t   *res = NULL;
    apr_dbd_row_t       *row = NULL;

    if ((*result)->pool == NULL) {
        return CONV_FAIL;
    }
    tpool = (*result)->pool;

    state = apr_psprintf(ptemp, "SELECT * FROM %s WHERE lst = %d",
                         BLACK_WHITE, IP_BLACK);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {               /* ��ȡ����ʧ�� */
        return CONV_FAIL;
    } if (rv == 0) {              /* ������IP�������򷵻سɹ� */
        return CONV_OK;
    } else {
        kw_num = rv;
    }
#if APU_HAVE_SQLITE3
    for (i = kw_num; i > 0; i--) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, -1);
#elif APU_HAVE_MYSQL
    for (i = 1; i <= kw_num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, i);
#endif
        if (rv == -1) {
            return CONV_FAIL;
        }

        entry = apr_dbd_get_entry(driver, row, 1);
        if (entry == NULL) {
            return CONV_FAIL;
        }

        type = atoi(entry);
        entry = apr_dbd_get_entry(driver, row, 2);
        switch (type) {
        case IB_CLIENT_IP:
            rule = apr_psprintf(ptemp, "SecBListCliIP Add %s", entry);

            break;
        case IB_DYN_EXCEPT:
            rule = apr_psprintf(ptemp, "SecBListDynCliIPExcept Add %s", entry);
            break;
        case IB_DYN_TIMEOUT:
            rule = apr_psprintf(ptemp, "SecBListDynCliIPTimeout %s", entry);
            break;
        default:
            return CONV_FAIL;
        }
        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, rule);
    }

    return CONV_OK;
}

static int combined_ip_white(apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 rv, type, i, kw_num;
    char                *state, *rule, **new;
    const char          *entry;
    apr_pool_t          *tpool;
    apr_dbd_results_t   *res = NULL;
    apr_dbd_row_t       *row = NULL;

    if ((*result)->pool == NULL) {
        return CONV_FAIL;
    }
     tpool = (*result)->pool;

    state = apr_psprintf(ptemp, "SELECT * FROM %s WHERE lst = %d",
                         BLACK_WHITE, IP_WHITE);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {               /* ��ȡ����ʧ�� */
        return CONV_FAIL;
    } else if (rv == 0) {          /* ������IP�������򷵻سɹ� */
        return CONV_OK;
    } else {
        kw_num = rv;
    }

#if APU_HAVE_SQLITE3
    for (i = kw_num; i > 0; i--) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, -1);
#elif APU_HAVE_MYSQL
    for (i = 1; i <= kw_num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, i);
#endif
        if (rv == -1) {
            return CONV_FAIL;
        }

        entry = apr_dbd_get_entry(driver, row, 1);
        if (entry == NULL) {
            return CONV_FAIL;
        }

        type = atoi(entry);
        entry = apr_dbd_get_entry(driver, row, 2);
        switch (type) {
            case IW_CLIENT_IP:
                rule = apr_psprintf(ptemp, "SecWListCliIP Add %s", entry);
                break;
            case IW_SERVER_IP:
                rule = apr_psprintf(ptemp, "SecWListServIP Add %s", entry);
                break;
            case IW_SERVER_HOST:
                rule = apr_psprintf(ptemp, "SecWListServHost Add %s", entry);
                break;
            default:
                return CONV_FAIL;
        }

        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, rule);
    }
    return CONV_OK;
}

static int combined_url_black(apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 rv = 0, type, i, kw_num;
    char                *state, *rule, **new;
    const char          *entry;
    apr_pool_t          *tpool;
    apr_dbd_results_t   *res = NULL;
    apr_dbd_row_t       *row = NULL;

    if ((*result)->pool == NULL) {
        return CONV_FAIL;
    }
     tpool = (*result)->pool;

    state = apr_psprintf(ptemp, "SELECT * FROM %s WHERE lst = %d",
                         BLACK_WHITE, URL_BLACK);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {               /* ��ȡ����ʧ�� */
        return CONV_FAIL;
    } else if (rv == 0) {          /* ������URL�������򷵻سɹ� */
        return CONV_OK;
    } else {
        kw_num = rv;
    }

#if APU_HAVE_SQLITE3
    for (i = kw_num; i > 0; i--) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, -1);
#elif APU_HAVE_MYSQL
    for (i = 1; i <= kw_num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, i);
#endif
        if (rv == -1) {
            return CONV_FAIL;
        }

        entry = apr_dbd_get_entry(driver, row, 1);
        if (entry == NULL) {
            return CONV_FAIL;
        }

        type = atoi(entry);
        entry = apr_dbd_get_entry(driver, row, 2);
        if (entry == NULL) {
            return CONV_FAIL;
        }
        switch (type) {
            case UB_URL_PLAIN:
				rule = apr_psprintf(ptemp, "SecBListURL Add PlainText %s", entry);
                break;
            case UB_URL_REGEX:
                rule = apr_psprintf(ptemp, "SecBListURL Add RegularExp %s", entry);
                break;
            case UB_DYN_EXCEPT:
                rule = apr_psprintf(ptemp, "SecBListDynRefURLExcept Add %s", entry);
                break;
            case UB_DYN_TIMEOUT:
                rule = apr_psprintf(ptemp, "SecBListDynRefURLTimeout %s", entry);
                break;
            default:
                return CONV_FAIL;
        }

        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, rule);
    }

    return CONV_OK;
}

static int combined_url_white(apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 rv = 0, i, kw_num, type;
    char                *state, *rule, **new;
    const char          *entry;
    apr_pool_t          *tpool;
    apr_dbd_results_t   *res = NULL;
    apr_dbd_row_t       *row = NULL;

    if ((*result)->pool == NULL) {
        return CONV_FAIL;
    }
     tpool = (*result)->pool;

    state = apr_psprintf(ptemp, "SELECT * FROM %s WHERE lst = %d",
                         BLACK_WHITE, URL_WHITE);

    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {               /* ��ȡ����ʧ�� */
        return CONV_FAIL;
    } else if (rv == 0) {          /* ������URL�������򷵻سɹ� */
        return CONV_OK;
    } else {
        kw_num = rv;
    }

#if APU_HAVE_SQLITE3
    for (i = kw_num; i > 0; i--) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, -1);
#elif APU_HAVE_MYSQL
    for (i = 1; i <= kw_num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, i);
#endif
        if (rv == -1) {
            return CONV_FAIL;
        }

        entry = apr_dbd_get_entry(driver, row, 1);
        if (entry == NULL) {
            return CONV_FAIL;
        }

        type = atoi(entry);
        entry = apr_dbd_get_entry(driver, row, 2);
        if (entry == NULL) {
            return CONV_FAIL;
        }

        switch (type) {
            case UW_URL_PLAIN:
			    rule = apr_psprintf(ptemp, "SecWListURL Add PlainText %s", entry);
                break;
            case UW_URL_REGEX:
                rule = apr_psprintf(ptemp, "SecWListURL Add RegularExp %s", entry);
                break;
            default:
                return CONV_FAIL;
        }

        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, rule);
    }

    return CONV_OK;
}

static int check_data(int lst, int type, char *data)
{
    if (data == NULL) {
        return CONV_FAIL;
    }

    switch (lst) {
    case IP_BLACK:
        if (type < IB_CLIENT_IP || type > IB_DYN_CLEAR) {
            return CONV_FAIL;
        }
        break;
    case IP_WHITE:
        if (type < IW_CLIENT_IP || type > IW_SERVER_HOST) {
            return CONV_FAIL;
        }
        break;
    case URL_BLACK:
        if (type < UB_URL_PLAIN || type > UB_DYN_CLEAR) {
            return CONV_FAIL;
        }
        break;
    case URL_WHITE:
        if (type < UW_URL_PLAIN || type > UW_URL_REGEX) {
            return CONV_FAIL;
        }
        break;
    default:
        return CONV_FAIL;
    }

    return CONV_OK;
}

/* ���IP��URL����/������ */
int convert_access_list_set(int lst, int type, char *data)
{
    int                     rv, nrows;
    char                    *state;
    const char              *keyword;
    apr_pool_t              *ptemp;
    apr_dbd_results_t       *res = NULL;

    rv = check_data(lst, type, data);
    if (rv != CONV_OK) {
        return CONV_FAIL;
    }

    rv = apr_pool_create(&ptemp, ptrans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    keyword = apr_dbd_escape(driver, ptemp, data, tmp_db);

    /* ����timeout�����������Ҫ�������ݿ� */
    if ((lst == IP_BLACK && type == IB_DYN_TIMEOUT)
            || (lst == URL_BLACK && type == UB_DYN_TIMEOUT)) {
        state = apr_psprintf(ptemp, "SELECT * from %s WHERE lst = %d and type = %d",
                             BLACK_WHITE, lst, type);
        rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
        if (rv > 0) {
            apr_pool_destroy(ptemp);
            return CONV_FAIL;
        }

        /* ����ֵʼ�մ��ڵ���0 */
        rv = apr_dbd_num_tuples(driver, res);
        if (rv == -1) {             /* ��ȡ����ʧ�� */
            apr_pool_destroy(ptemp);
            return CONV_FAIL;
        } else if (rv > 0) {        /* �������timeout��������ݿ� */
            state = apr_psprintf(ptemp, "UPDATE %s SET keyword = '%s' WHERE lst = %d and"\
                                 " type = %d", BLACK_WHITE, keyword, lst, type);
            rv = apr_dbd_query(driver, tmp_db, &nrows, state);
            apr_pool_destroy(ptemp);
            if (rv > 0) {
                return CONV_FAIL;
            } else {
                return CONV_OK;
            }
        } else if (rv == 0) {       /* ������timeout��������� */
            state = apr_psprintf(ptemp, "INSERT INTO %s VALUES (%d, %d, '%s')",
                                 BLACK_WHITE, lst, type, keyword);
            rv = apr_dbd_query(driver, tmp_db, &nrows, state);
            apr_pool_destroy(ptemp);
            if (rv > 0) {
                return CONV_FAIL;
            } else {
                return CONV_OK;
            }
        }
    }

    /* ��ѯ�����Ƿ��� */
    state = apr_psprintf(ptemp, "SELECT * from %s where lst = %d and type = %d",
                        BLACK_WHITE, lst, type);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        apr_pool_destroy(ptemp);
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {                         /* ��ȡ����ʧ�� */
        apr_pool_destroy(ptemp);
        return CONV_FAIL;
    } else if (rv >= IP_BWLIST_ENTRY_NUM) {  /* �������� */
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                     "WAF_CONVERT: too much item in the list");
        apr_pool_destroy(ptemp);
        return CONV_OVERFLOW;
    }

    /* ��ѯ�Ƿ������ͬԪ�� */
    state = apr_psprintf(ptemp, "SELECT * from %s WHERE lst = %d and type = %d and" \
                         " keyword = '%s'", BLACK_WHITE, lst, type, keyword);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        apr_pool_destroy(ptemp);
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {                 /* ��ȡ����ʧ�� */
        return CONV_FAIL;
    } else if (rv > 0) {            /* ������ͬԪ�أ����س�ͻ */
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                     "WAF_CONVERT: item conflict in the list");
        apr_pool_destroy(ptemp);
        return CONV_CONFLICT;
    }

    /* �������� */
    keyword = apr_dbd_escape(driver, ptemp, data, tmp_db);
    state = apr_psprintf(ptemp, "INSERT INTO %s VALUES (%d, %d, '%s')",
                        BLACK_WHITE, lst, type, keyword);
    rv = apr_dbd_query(driver, tmp_db, &nrows, state);
    apr_pool_destroy(ptemp);
    if (rv > 0) {
        return CONV_FAIL;
    } else {
        return CONV_OK;
    }
}

/* ���úڰ�������ȱʡֵ */
int convert_access_default_set(void)
{
    int rv;

    rv = convert_access_list_set(IP_BLACK, IB_DYN_TIMEOUT, IP_BLACK_TIMEOUT);
    if (rv != CONV_OK) {
        return CONV_FAIL;
    }

    rv = convert_access_list_set(URL_BLACK, UB_DYN_TIMEOUT, URL_BLACK_TIMEOUT);
    if (rv != CONV_OK) {
        return CONV_FAIL;
    }

    return CONV_OK;
}


/* ɾ����/�������е�IP��URL */
int convert_access_list_clear(int lst, int type, char *data)
{
    int                     rv, nrows;
    char                    *state;
    apr_pool_t              *ptemp;
    const char              *keyword;
    apr_dbd_results_t       *res = NULL;

    rv = check_data(lst, type, data);
    if (rv != CONV_OK) {
        return CONV_FAIL;
    }

    rv = apr_pool_create(&ptemp, ptrans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    if (strcmp(data, KEYWORD_ALL)) {    /* ɾ��ָ����IP��URL */
        keyword = apr_dbd_escape(driver, ptemp, data, tmp_db);

        /* ����Ԫ���Ƿ���� */
        state = apr_psprintf(ptemp, "SELECT * FROM %s where lst = %d and type = %d and "\
                             "keyword = '%s'", BLACK_WHITE, lst, type, keyword);
        rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
        if (rv > 0) {
            apr_pool_destroy(ptemp);
            return CONV_FAIL;
        }
        rv = apr_dbd_num_tuples(driver, res);
        if (rv == -1) {                         /* ��ȡ����ʧ�� */
            apr_pool_destroy(ptemp);
            return CONV_FAIL;
        } else if (rv == 0) {                   /* �����ڶ�ӦԪ�� */
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                         "WAF_CONVERT: too much item in the list");
            apr_pool_destroy(ptemp);
            return CONV_NOTEXIST;
        }

        /* ɾ����ӦԪ�� */
        state = apr_psprintf(ptemp, "DELETE FROM %s where lst = %d and type = %d and "\
                             "keyword = '%s'", BLACK_WHITE, lst, type, keyword);
        rv = apr_dbd_query(driver, tmp_db, &nrows, state);
    } else {                            /* ɾ�����е�IP��URL */
        state = apr_psprintf(ptemp, "DELETE FROM %s where lst = %d and type = %d",
                             BLACK_WHITE, lst, type);
        rv = apr_dbd_query(driver, tmp_db, &nrows, state);
    }

    apr_pool_destroy(ptemp);
    if (rv > 0) {
        return CONV_FAIL;
    } else {
        return CONV_OK;
    }
}

/* ת�����ڲ���ĺ�/������ */
int convert_access_list_query(int lst, apr_array_header_t **result)
{
    int rv;
    apr_pool_t *ptemp;

    if ((lst < IP_BLACK) || (lst > ALL_LIST) || (result == NULL) || (*result == NULL)) {
        return CONV_FAIL;
    }

    rv = apr_pool_create(&ptemp, ptrans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    switch (lst) {
    case IP_BLACK:      /* ת��IP������ */
        rv = combined_ip_black(result, ptemp);
        break;
    case IP_WHITE:      /* ת��IP������ */
        rv = combined_ip_white(result, ptemp);
        break;
    case URL_BLACK:     /* ת��URL������ */
        rv = combined_url_black(result, ptemp);
        break;
    case URL_WHITE:     /* ת��URL������ */
        rv = combined_url_white(result, ptemp);
        break;
    case ALL_LIST:      /* ת�����к�/������ */
        rv = combined_ip_black(result, ptemp);
        if (rv < 0) {
            break;
        }
        rv = combined_ip_white(result, ptemp);
        if (rv < 0) {
            break;
        }
        rv = combined_url_black(result, ptemp);
        if (rv < 0) {
            break;
        }
        rv = combined_url_white(result, ptemp);
        if (rv < 0) {
            break;
        }
        break;
    default:
        rv = CONV_FAIL;
        break;
    }

    apr_pool_destroy(ptemp);
    if (rv < 0) {
        return CONV_FAIL;
    } else {
        return CONV_OK;
    }

}

/* ��ѯ������ʾ�ĺ�/������ */
int convert_access_list_show(int lst, int type, apr_array_header_t **result)
{
    int                     rv, i, kw_num;
    char                    *state, **new;
    apr_dbd_results_t       *res = NULL;
    apr_dbd_row_t           *row = NULL;
    const char              *keyword;
    apr_pool_t              *ptemp, *tpool;

    switch (lst) {
    case IP_BLACK:
        if (type < IB_CLIENT_IP || type > IB_DYN_CLEAR) {
            return CONV_FAIL;
        }
        break;
    case IP_WHITE:
        if (type < IW_CLIENT_IP || type > IW_SERVER_HOST) {
            return CONV_FAIL;
        }
        break;
    case URL_BLACK:
        if (type < UB_URL_PLAIN || type > UB_DYN_CLEAR) {
            return CONV_FAIL;
        }
        break;
    case URL_WHITE:
        if (type < UW_URL_PLAIN || type > UW_URL_REGEX) {
            return CONV_FAIL;
        }
        break;
    default:
        return CONV_FAIL;
    }

    rv = apr_pool_create(&ptemp, ptrans);
    if (rv) {
        return CONV_FAIL;
    }

    /* ��ѯ�ڰ����� */
    state = apr_psprintf(ptemp, "SELECT keyword from %s where lst = %d and type = %d",
                        BLACK_WHITE, lst, type);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {                           /* ��ѯ�ڰ�����ʧ�� */
        apr_pool_destroy(ptemp);
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {                         /* ��ȡ����ʧ�� */
        apr_pool_destroy(ptemp);
        return CONV_FAIL;
    } else if (rv == 0) {                   /* �����ںڰ����� */
        apr_pool_destroy(ptemp);
        return CONV_OK;
    } else {
        kw_num = rv;
    }

    if ((*result)->pool == NULL) {
        return CONV_FAIL;
    }
    tpool = (*result)->pool;
    /* �������ݿ��ѯ����� */
#if APU_HAVE_SQLITE3
    for (i = kw_num; i > 0; i--) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, -1);
#elif APU_HAVE_MYSQL
    for (i = 1; i <= kw_num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, i);
#endif
        if (rv == -1) {
            apr_pool_destroy(ptemp);
            return CONV_FAIL;
        }

        keyword = apr_dbd_get_entry(driver, row, 0);
        if (keyword == NULL) {
            apr_pool_destroy(ptemp);
            return CONV_FAIL;
        }

        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, keyword);
    }

    apr_pool_destroy(ptemp);
    return CONV_OK;
}
