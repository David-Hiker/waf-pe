/*
 * $Id: convert_modify_check_value.c 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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

/* ����ֻ�����ò��Կ��еļ��ֵ�����޸ĵĹؼ��� */
int modified_keyword_add(keyword_t *k, const char *flag, apr_pool_t *ptemp)
{
    int                     rv, nrows;
    char                    *state, **kw;
    const char              *kw_id, *sec_name, *keyword;
    apr_dbd_results_t       *res = NULL;
    apr_dbd_row_t           *row = NULL;
    apr_dbd_transaction_t   *trans = NULL;

    if ((k == NULL) || (flag == NULL) || (ptemp == NULL)) {
        return CONV_FAIL;
    }

    /* �ؼ�����Ŀ����һ������� */
    rv = k->keyword->nelts;
    if (rv != 1) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "WAF_CONVERT: too much keyword!");
        return CONV_FAIL;
    }
    kw = apr_array_pop(k->keyword);

    sec_name = apr_dbd_escape(driver, ptemp, k->sec_policy, tmp_db);
    keyword = apr_dbd_escape(driver, ptemp, *kw, tmp_db);
    state = apr_psprintf(ptemp, "SELECT keyword_id FROM %s where sec_policy = '%s' and"\
                         " sec_subpolicy = %d and type = %d", KEYWORD_ID_TAB,
                         sec_name, k->sec_subpolicy, k->type);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {
        return rv;
    } else if (rv > 0) {    /* ���ڴ˹ؼ��֣���ֻ����±��� */
#if APU_HAVE_SQLITE3
        rv = apr_dbd_get_row(driver, ptemp, res, &row, -1);
#elif APU_HAVE_MYSQL
        rv = apr_dbd_get_row(driver, ptemp, res, &row, 1);
#endif
        if (rv == -1) {
            return CONV_FAIL;
        }

        kw_id = apr_dbd_get_entry(driver, row, 0);
        if (kw_id == NULL) {
            return CONV_FAIL;
        }

        state = apr_psprintf(ptemp, "UPDATE %s SET value = '%s' WHERE keyword_id = %s",
                             KEYWORD_TAB, keyword, kw_id);
        rv = apr_dbd_query(driver, tmp_db, &nrows, state);
        if (rv > 0) {
            return CONV_FAIL;
        } else {
            return CONV_OK;
        }
    } else {    /* �����ڹؼ��֣�����Ҫ��� */
        kw_id_flag++;

        rv = apr_dbd_transaction_start(driver, ptemp, tmp_db, &trans);
        if (rv > 0) {
            kw_id_flag--;
            return CONV_FAIL;
        }
        state = apr_psprintf(ptemp, "INSERT INTO %s VALUES (%ld, '%s', %d, %d)",
                             KEYWORD_ID_TAB, kw_id_flag, sec_name, k->sec_subpolicy, k->type);
        rv = apr_dbd_query(driver, tmp_db, &nrows, state);
        if (rv > 0) {
            kw_id_flag--;
            apr_dbd_transaction_end(driver, ptemp, trans);
            return CONV_FAIL;
        }

        state = apr_psprintf(ptemp, "INSERT INTO %s VALUES (%ld, %d, '%s')",
                             KEYWORD_TAB, kw_id_flag, KEYWORD_FIRST, keyword);
        rv = apr_dbd_query(driver, tmp_db, &nrows, state);
        if (rv > 0) {
            kw_id_flag--;
            apr_dbd_transaction_end(driver, ptemp, trans);
            return CONV_FAIL;
        }

        rv = apr_dbd_transaction_end(driver, ptemp, trans);
        if (rv > 0) {
            kw_id_flag--;
            return CONV_FAIL;
        }
        return CONV_OK;
    }
}

/* ɾ������ʱֻ�����ò��Կ��еļ��ֵ�����޸ĵĹؼ��� */
int modified_keyword_del(keyword_t *k, apr_pool_t *ptemp)
{
    int                     rv, nrows;
    char                    *state;
    const char              *kw_id, *sec_name;
    apr_dbd_results_t       *res = NULL;
    apr_dbd_row_t           *row = NULL;
    apr_dbd_transaction_t   *trans = NULL;

    if ((k == NULL) || (ptemp == NULL)) {
        return CONV_FAIL;
    }

    sec_name = apr_dbd_escape(driver, ptemp, k->sec_policy, tmp_db);
    state = apr_psprintf(ptemp, "SELECT keyword_id FROM %s where sec_policy = '%s' and"\
                         " sec_subpolicy = %d and type = %d", KEYWORD_ID_TAB,
                         sec_name, k->sec_subpolicy, k->type);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {
        return CONV_FAIL;
    } else if (rv == 0) {    /* �����ڹؼ��֣��򷵻سɹ� */
        return CONV_OK;
    }

#if APU_HAVE_SQLITE3
    rv = apr_dbd_get_row(driver, ptemp, res, &row, -1);
#elif APU_HAVE_MYSQL
    rv = apr_dbd_get_row(driver, ptemp, res, &row, 1);
#endif
    if (rv == -1) {
        return CONV_FAIL;
    }

    kw_id = apr_dbd_get_entry(driver, row, 0);
    if (kw_id == NULL) {
        return CONV_FAIL;
    }

    /* ɾ�����ݿ������� */
    rv = apr_dbd_transaction_start(driver, ptemp, tmp_db, &trans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s where keyword_id = '%s'",
                         KEYWORD_TAB, kw_id);
    rv = apr_dbd_query(driver, tmp_db, &nrows, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s where keyword_id = '%s'",
                         KEYWORD_ID_TAB, kw_id);
    rv = apr_dbd_query(driver, tmp_db, &nrows, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    rv = apr_dbd_transaction_end(driver, ptemp, trans);

    if (rv > 0) {
        return CONV_FAIL;
    } else {
        return CONV_OK;
    }
}

/* ������Ҫ�޸ļ��ֵ�Ĺ��� */
int modified_keyword_query(const char *name, int sec_subpolicy, apr_dbd_row_t *row, int type,
                           apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 rv;
    const char          *check_val, *sec_name, *keyword;
    char                *state;
    apr_dbd_results_t   *mres = NULL;
    apr_dbd_row_t       *mrow = NULL;

    if ((name == NULL) || (sec_subpolicy < SQL_INJECTION) || (sec_subpolicy >= MAX_SUBPOLICY)
            || (row == NULL) || (type < 0) || (result == NULL)
            || (*result == NULL) || (ptemp == NULL)) {
        return CONV_FAIL;
    }

    /* ����ʱ������ȡ���ؼ��� */
    sec_name = apr_dbd_escape(driver, ptemp, name, tmp_db);
    state = apr_psprintf(ptemp, "SELECT value FROM %s WHERE keyword_id in (select %s from"\
                         " keyword_id where sec_policy = '%s' and sec_subpolicy = %d "\
                         "and type = %d)", KEYWORD_TAB,
                         KEYWORD_ID_TAB, sec_name, sec_subpolicy, type);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &mres, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, mres);
    if (rv == -1) {
        return CONV_FAIL;
    } else if (rv > 0) {
#if APU_HAVE_SQLITE3
        rv = apr_dbd_get_row(driver, ptemp, mres, &mrow, -1);
#elif APU_HAVE_MYSQL
        rv = apr_dbd_get_row(driver, ptemp, mres, &mrow, 1);
#endif
        if (rv == -1) {
            return CONV_FAIL;
        }
        check_val = apr_dbd_get_entry(driver, mrow, 0);
    } else {
        check_val = NULL;
    }

    keyword = escape_keyword(check_val, ptemp);
    combined_rule(row, keyword, NULL, result, ptemp);
    return CONV_OK;
}

/* ��ѯ����ʱ��Ҫ�����ù�����ֵ�����޸ĵģ�������ʾ�Ĺؼ��� */
int modified_keyword_list(const char *sec_policy, int sec_subpolicy, int type,
                          const char *mod_flag, apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 rv, mark;
    char                **new, *state;
    const char          *sec_name, *entry, *modified;
    apr_dbd_results_t   *res = NULL;
    apr_dbd_row_t       *row = NULL;
    apr_pool_t          *ptab;
    keyword_t           *keyword, **new_keyword;

    if ((sec_policy == NULL) || (sec_subpolicy < SQL_INJECTION) || (sec_subpolicy >= MAX_SUBPOLICY)
            || (type < 0) || (mod_flag == NULL) || (result == NULL)
            || (*result == NULL) || (ptemp == NULL)) {
        return CONV_FAIL;
    }

    ptab = (*result)->pool;
    sec_name = apr_dbd_escape(driver, ptemp, sec_policy, tmp_db);
    modified = apr_dbd_escape(driver, ptemp, mod_flag, tmp_db);

    /* ��ѯ�Ƿ����øð�ȫ�����µİ�ȫ�Ӳ��� */
    sec_name = apr_dbd_escape(driver, ptemp, sec_policy, tmp_db);
    state = apr_psprintf(ptemp, "select * from %s where sec_policy = '%s' and sub_policy = '%s'",
                         SEC_POLICY_TAB, sec_name, policy_type[sec_subpolicy]);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }
    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {
        return CONV_FAIL;
    }

    mark = OUTER_KEYWORD;
    state = apr_psprintf(ptemp, "SELECT value from %s WHERE keyword_id IN (select"\
                         " keyword_id from %s where sec_policy = '%s' and sec_subpolicy"\
                         " = %d and type = %d)", KEYWORD_TAB, KEYWORD_ID_TAB,
                         sec_name, sec_subpolicy, type);

    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {
        return CONV_FAIL;
    } else if (rv == 0){            /* ��ѯ���ÿ���ֵ */
        mark = INNER_KEYWORD;
        state = apr_psprintf(ptemp, "select check_value from %s where flag = '%s'",
                             BASIC_TAB, modified);
        rv = apr_dbd_select(driver, ptemp, default_db, &res, state, 1);
        if (rv > 0) {
            return CONV_FAIL;
        }

        rv = apr_dbd_num_tuples(driver, res);
        if (rv == -1) {             /* ��ѯ���� */
            return CONV_FAIL;
        } else if (rv == 0) {       /* ���ÿ��в����ڼ��ֵ */
            return CONV_FAIL;
        }
    }

#if APU_HAVE_SQLITE3
    rv = apr_dbd_get_row(driver, ptemp, res, &row, -1);
#elif APU_HAVE_MYSQL
    rv = apr_dbd_get_row(driver, ptemp, res, &row, 1);
#endif
    if (rv == -1) {
        return CONV_FAIL;
    }

    entry = apr_dbd_get_entry(driver, row, 0);
    if (entry == NULL) {
        return CONV_FAIL;
    }

    keyword = apr_pcalloc(ptab, sizeof(keyword_t));
    keyword->type = type;
    keyword->flag = mark;

    keyword->keyword = apr_array_make(ptab, 1, sizeof(char *));
    new = (char **)apr_array_push(keyword->keyword);
    *new = apr_pstrdup(ptab, entry);

    new_keyword = (keyword_t **)apr_array_push(* result);
    *new_keyword = keyword;

    return CONV_OK;
}
