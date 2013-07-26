/*
 * $Id: convert_csrf.c 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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

static char *cookie_name = "csrf-cookie-name";
static char *url_flag = "csrf-url";
static char *csrf_check = "((?i:%s)=([^\\s]+)\\;\\s?)";

/* �ж��Ƿ������ָ���Ĺؼ��֣�����Ѿ������򷵻ض�Ӧ��id */
static int check_new_keyword(const char *keyword, const char *sec_name, const int type, 
                            const int sec_subpolicy, const char **keyword_id, apr_pool_t *ptemp)
{
    int                 rv;
    char                *state;
    const char          *kw_id;
    apr_dbd_results_t   *res = NULL;
    apr_dbd_row_t       *row = NULL;


    state = apr_psprintf(ptemp, "SELECT keyword_id FROM %s WHERE value = '%s' AND"\
                         " keyword_id IN (SELECT keyword_id FROM %s WHERE sec_policy"\
                         " = '%s' AND sec_subpolicy = %d AND type = %d)",
                         KEYWORD_TAB, keyword, KEYWORD_ID_TAB, sec_name, sec_subpolicy, type);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {
        return DECLINED;
    } else if (rv == 0) {        /* �����ڶ�Ӧ�ؼ��� */
        return CONV_NOTEXIST;         
    }

    rv = apr_dbd_get_row(driver, ptemp, res, &row, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    kw_id = apr_dbd_get_entry(driver, row, 0);
    if (kw_id == NULL) {
        return CONV_FAIL;
    }

    *keyword_id = kw_id;
    return CONV_CONFLICT;
}

/* ������Ҫ�����µĹ���ID�Ĺؼ��� */
static int add_keyword_with_id(const char *keyword, const char *sec_name, const int type, 
                            const int sec_subpolicy, apr_pool_t *ptemp)
{
    int                     rv, nrow, new_id;
    char                    *state;
    unsigned long           kw_id;
    apr_dbd_transaction_t   *trans = NULL;
    

    kw_id = kw_id_flag++;
    new_id = bm_firstunset(&nbitmap);
    bm_setbit(&nbitmap, new_id);

    rv = apr_dbd_transaction_start(driver, ptemp, tmp_db, &trans);
    if (rv > 0) {
        kw_id_flag--;
        bm_clrbit(&nbitmap, new_id);
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "INSERT INTO %s VALUES (%ld, '%s', %d, %d)",
                         KEYWORD_ID_TAB, kw_id_flag, sec_name, sec_subpolicy, type);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        kw_id_flag--;
        bm_clrbit(&nbitmap, new_id);
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }  

    state = apr_psprintf(ptemp, "INSERT INTO %s VALUES (%ld, %d, '%s')",
                         KEYWORD_TAB, kw_id_flag, KEYWORD_FIRST, keyword);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        kw_id_flag--;
        bm_clrbit(&nbitmap, new_id);
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    } 

    state = apr_psprintf(ptemp, "INSERT INTO %s VALUES (%ld, %d)",
                         NEW_TAB, kw_id_flag, new_id);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        kw_id_flag--;
        bm_clrbit(&nbitmap, new_id);
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    rv = apr_dbd_transaction_end(driver, ptemp, trans);
    if (rv > 0) {
        kw_id_flag--;
        bm_clrbit(&nbitmap, new_id);
        return CONV_FAIL;
    }

    return CONV_OK; 
}

/* �������贴���µĹ���ID�Ĺؼ��� */
static int add_keyword(const char *keyword, const char *sec_name, const int type, 
                            const int sec_subpolicy, apr_pool_t *ptemp)
{
    int                     rv, nrow;
    char                    *state;
    unsigned long           kw_id;
    apr_dbd_transaction_t   *trans = NULL;
    

    kw_id = kw_id_flag++;

    rv = apr_dbd_transaction_start(driver, ptemp, tmp_db, &trans);
    if (rv > 0) {
        kw_id_flag--;
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "INSERT INTO %s VALUES (%ld, '%s', %d, %d)",
                         KEYWORD_ID_TAB, kw_id_flag, sec_name, sec_subpolicy, type);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        kw_id_flag--;
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }  

    state = apr_psprintf(ptemp, "INSERT INTO %s VALUES (%ld, %d, '%s')",
                         KEYWORD_TAB, kw_id_flag, KEYWORD_FIRST, keyword);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
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

static int csrf_keyword_add(keyword_t *k, apr_pool_t *ptemp)
{
    int         rv;
    char        **kw;
    const char  *keyword_id;
    const char  *sec_name, *keyword;

    if ((k == NULL) || (ptemp == NULL)) {
        return CONV_FAIL;
    }

    rv = k->keyword->nelts;
    if (rv != 1) {
        return CONV_FAIL;
    }

    sec_name = apr_dbd_escape(driver, ptemp, k->sec_policy, tmp_db);
    kw = apr_array_pop(k->keyword);
    keyword = apr_dbd_escape(driver, ptemp, *kw, tmp_db);    
    
    /* �жϹؼ����Ƿ��Ѿ����� */
    rv = check_new_keyword(keyword, sec_name, k->type, k->sec_subpolicy, &keyword_id, ptemp);
    if (rv != CONV_NOTEXIST) {
        return rv;
    }

    if (k->type == CSRF_COOKIE_NAME) {
        return add_keyword_with_id(keyword, sec_name, k->type, k->sec_subpolicy, ptemp);
    } else if (k->type == CSRF_URL_PLAIN || k->type == CSRF_URL_REGEX) {
        return add_keyword(keyword, sec_name, k->type, k->sec_subpolicy, ptemp);
    } else {
        return CONV_FAIL;
    }
}

/* ɾ��ָ������Ҫ�����µĹ���ID�Ĺؼ��� */
static int del_keyword_with_id(const char *keyword_id, apr_pool_t *ptemp)
{
    int                     rv, nrow, new;
    char                    *state;
    apr_dbd_transaction_t   *trans = NULL;


    rv = apr_dbd_transaction_start(driver, ptemp, tmp_db, &trans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s where keyword_id = '%s'",
                         NEW_TAB, keyword_id);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s where keyword_id = '%s'",
                         KEYWORD_TAB, keyword_id);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s where keyword_id = '%s'",
                         KEYWORD_ID_TAB, keyword_id);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    rv = apr_dbd_transaction_end(driver, ptemp, trans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    bm_clrbit(&nbitmap, new);
    return CONV_OK;    
}

/* ɾ�����������µĹ���ID�Ĺؼ��� */
static int del_keyword(const char *keyword_id, apr_pool_t *ptemp)
{
    int                     rv, nrow;
    char                    *state;
    apr_dbd_transaction_t   *trans = NULL;    

    rv = apr_dbd_transaction_start(driver, ptemp, tmp_db, &trans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s where keyword_id = '%s'",
                         KEYWORD_TAB, keyword_id);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s where keyword_id = '%s'",
                         KEYWORD_ID_TAB, keyword_id);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    rv = apr_dbd_transaction_end(driver, ptemp, trans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    return CONV_OK;
}

/* ɾ���������������¹���ID�Ĺؼ��� */
static int del_all_keyword(const char *sec_name, const int sec_subpolicy, const int type, 
                        apr_pool_t *ptemp)
{
    int                     rv, nrow;
    int                     num;
    char                    *state;
    apr_dbd_results_t       *res = NULL;
    apr_dbd_row_t           *row = NULL;
    apr_dbd_transaction_t   *trans = NULL;

    rv = apr_dbd_transaction_start(driver, ptemp, tmp_db, &trans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s WHERE keyword_id in (SELECT keyword_id"\
                         " FROM %s where sec_policy = '%s' and sec_subpolicy = %d and type = %d)",
                         NEW_TAB, KEYWORD_ID_TAB, sec_name, sec_subpolicy, type);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s WHERE keyword_id in (SELECT keyword_id"\
                         " FROM %s where sec_policy = '%s' and sec_subpolicy = %d and type = %d)",
                         KEYWORD_TAB, KEYWORD_ID_TAB, sec_name, sec_subpolicy, type);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    state = apr_psprintf(ptemp, "DELETE FROM %s where sec_policy = '%s' and sec_subpolicy = %d and type = %d",
                         KEYWORD_ID_TAB, sec_name, sec_subpolicy, type);
    rv = apr_dbd_query(driver, tmp_db, &nrow, state);
    if (rv > 0) {
        apr_dbd_transaction_end(driver, ptemp, trans);
        return CONV_FAIL;
    }

    rv = apr_dbd_transaction_end(driver, ptemp, trans);
    if (rv > 0) {
        return CONV_FAIL;
    }

    return CONV_OK;
}

static int del_all_keyword_with_id(const char *sec_name, const int sec_subpolicy, const int type, 
                        apr_pool_t *ptemp)
{
    int                     rv, i, new;
    int                     num;
    char                    *state;
    const char              *new_id;
    apr_dbd_results_t       *res = NULL;
    apr_dbd_row_t           *row = NULL;
    apr_array_header_t      *id_arr;

    state = apr_psprintf(ptemp, "SELECT DISTINCT new_id FROM %s WHERE keyword_id in (SELECT"\
                         " keyword_id FROM %s where sec_policy = '%s' and sec_subpolicy = %d and type = %d)",
                         NEW_TAB, KEYWORD_ID_TAB, sec_name, sec_subpolicy, type);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {
        return CONV_FAIL;
    } else if (rv == 0) {
        return CONV_OK;
    } else {
        id_arr = apr_array_make(ptemp, 1, sizeof(int));
        num = rv;
        for (i = 1; i <= num; i++) {
            rv = apr_dbd_get_row(driver, ptemp, res, &row, i);
            if (rv == -1) {
                return CONV_FAIL;
            }

            new_id = apr_dbd_get_entry(driver, row, 0);
            new = atoi(new_id);
            *(int *)apr_array_push(id_arr) = new;
        }
    }

    rv = del_all_keyword(sec_name, sec_subpolicy, type, ptemp);
    if (rv != CONV_OK) {
        return CONV_OK;
    }

    for (i = 0; i < id_arr->nelts; i++) {
        new = ((int *)id_arr->elts)[i];
        bm_clrbit(&nbitmap, new);
    }

    return CONV_OK;
}

static int csrf_keyword_del(keyword_t *k, apr_pool_t *ptemp)
{
    int         rv;
    char        **kw;
    const char  *keyword_id;
    const char  *sec_name, *keyword;

    if ((k == NULL) || (ptemp == NULL)) {
        return CONV_FAIL;
    }

    sec_name = apr_dbd_escape(driver, ptemp, k->sec_policy, tmp_db);

    /* ɾ�����йؼ��� */
    if (k->flag == FLAG_ALL) {
        if (k->type == CSRF_COOKIE_NAME) {
            return del_all_keyword_with_id(sec_name, k->sec_subpolicy, CSRF_COOKIE_NAME, ptemp);
        } else if (k->type == CSRF_URL_ALL) {
            rv =  del_all_keyword(sec_name, k->sec_subpolicy, CSRF_URL_PLAIN, ptemp);
            if (rv != CONV_OK) {
                return CONV_OK;
            }
            return del_all_keyword(sec_name, k->sec_subpolicy, CSRF_URL_REGEX, ptemp);
        } else {
            return CONV_FAIL;
        }
    } 
    
    rv = k->keyword->nelts;
    if (rv != 1) {
        return CONV_FAIL;
    }

    kw = apr_array_pop(k->keyword);
    keyword = apr_dbd_escape(driver, ptemp, *kw, tmp_db);

    /* ���ؼ����Ƿ���� */
    rv = check_new_keyword(keyword, sec_name, k->type, k->sec_subpolicy, &keyword_id, ptemp);
    if (rv != CONV_CONFLICT) {
            return rv;
    }    
    
    if (k->type == CSRF_COOKIE_NAME) {
        return del_keyword_with_id(keyword_id, ptemp);
    } else if (k->type == CSRF_URL_PLAIN || k->type == CSRF_URL_REGEX) {
        return del_keyword(keyword_id, ptemp);
    } else {
        return CONV_FAIL;
    }     
}

static int combind_csrf_cookie(const char *name, apr_dbd_row_t *row, const char *rule,
                                apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 i, rv;
    int                 num;
    char                **new, *state, *check_value;
    const char          *sec_name, *keyword, *rule_id, *entry;
    apr_pool_t          *tpool;
    apr_dbd_results_t   *csrf_res = NULL;
    apr_dbd_row_t       *csrf_row = NULL;

    tpool = (*result)->pool;
    
    combined_rule(row, NULL, NULL, result, ptemp);
    new = (char **)apr_array_push(*result);
    *new = apr_pstrdup(tpool, rule);    

    sec_name = apr_dbd_escape(driver, ptemp, name, tmp_db);
    state = apr_psprintf(ptemp, "SELECT %s.new_id, %s.value FROM %s, %s where "\
                         "%s.keyword_id = %s.keyword_id and %s.keyword_id in (SELECT "\
                         "keyword_id from %s WHERE sec_policy = '%s' and sec_subpolicy = %d)",
                         NEW_TAB, KEYWORD_TAB, NEW_TAB, KEYWORD_TAB, NEW_TAB, KEYWORD_TAB,
                         KEYWORD_TAB, KEYWORD_ID_TAB, sec_name, CSRF);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &csrf_res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }
    rv = apr_dbd_num_tuples(driver, csrf_res);
    if (rv == -1) {
        return CONV_FAIL;
    } else if (rv == 0){
        return CONV_OK;
    }
    
    num = rv;
    for (i = 1; i <= num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, csrf_res, &csrf_row, i);
        if (rv == -1) {
            return CONV_FAIL;
        }

        entry = apr_dbd_get_entry(driver, csrf_row, 1);
        keyword = escape_keyword(entry, ptemp);
        rule_id = apr_dbd_get_entry(driver, csrf_row, 0);
        check_value = apr_psprintf(ptemp,  csrf_check, keyword);
        combined_rule(row, check_value, rule_id, result, ptemp);

        /* ����ڶ������� */
        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, rule);
        
    }

    return CONV_OK;    
}

static int combind_inner_rule(apr_array_header_t **result, const char **start, 
                                    const char **end, apr_pool_t *ptemp)
{
    int                 rv, i, num;
    char                *state;
    const char          *entry, *ptype, *start_id, *end_id;
    apr_dbd_results_t   *res = NULL;
    apr_dbd_row_t       *row = NULL;    

    /* ��ȡLocation ���� */
    entry = apr_psprintf(ptemp, "%s", policy_type[CSRF]);
    ptype = apr_dbd_escape(driver, ptemp, entry, tmp_db);

    state = apr_psprintf(ptemp, "select list_id from %s where type = '%s' and"\
                        " keyword like '%%Location%%' order by list_id",
                         BASIC_TAB, ptype);
    rv = apr_dbd_select(driver, ptemp, default_db, &res, state, 1);
    if (rv > 0) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "WAF_CONVERT: default policy is destroyed");
        return CONV_FAIL;
    }

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {
        return CONV_FAIL;
    } else if (rv != 2) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "WAF_CONVERT: default policy is destroyed");
        return CONV_FAIL;
    }

    rv = apr_dbd_get_row(driver, ptemp, res, &row, 1);
    if (rv) {
        return CONV_FAIL;
    }

    start_id = apr_dbd_get_entry(driver, row, 0); 
    *start  = start_id;
   
    rv = apr_dbd_get_row(driver, ptemp, res, &row, 2);
    if (rv) {
        return CONV_FAIL;
    }

    end_id = apr_dbd_get_entry(driver, row, 0); 
    *end = end_id;

    /* ��ȡLocation�����еĹ��� */
    state = apr_psprintf(ptemp, "select * from %s left join %s on %s.list_id = "\
                         "%s.list_id where %s.list_id > %s and %s.list_id < %s",
                         BASIC_TAB, EXTEND_TAB, BASIC_TAB, EXTEND_TAB, BASIC_TAB,
                         start_id, BASIC_TAB, end_id);
    rv = apr_dbd_select(driver, ptemp, default_db, &res, state, 1);
    if (rv > 0) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "WAF_CONVERT: default policy is destroyed");
        return CONV_FAIL;
    }    

    rv = apr_dbd_num_tuples(driver, res);
    if (rv == -1) {
        return CONV_FAIL;
    } else if (rv == 0) {        
        return CONV_OK;
    }

    num = rv;
    for (i = 1; i <= num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, res, &row, i);
        if (rv) {
            return CONV_FAIL;
        }
        combined_rule(row, NULL, NULL, result, ptemp);
    }
    return CONV_OK; 
}

static int combind_csrf_url(const char *name, apr_array_header_t *inner_rule,
                                apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 i, rv, num;
    char                *rule, **new, *state, *check_value;
    const char          *sec_name, *keyword, *rule_id, *entry;
    apr_pool_t          *tpool;
    apr_dbd_results_t   *csrf_res = NULL;
    apr_dbd_row_t       *csrf_row = NULL;


    sec_name = apr_dbd_escape(driver, ptemp, name, tmp_db);
    /* ����Location���� */
    state = apr_psprintf(ptemp, "SELECT value FROM %s WHERE keyword_id IN (SELECT keyword_id"\
                                    " FROM %s where sec_policy = '%s' and sec_subpolicy = %d"\
                                    " and type = %d)", KEYWORD_TAB, KEYWORD_ID_TAB, 
                                    sec_name,  CSRF, CSRF_URL_PLAIN);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &csrf_res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }
    rv = apr_dbd_num_tuples(driver, csrf_res);
    if (rv == -1) {
        return CONV_FAIL;
    } 

    num = rv;
    tpool = (*result)->pool;
    for (i = 1; i <= num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, csrf_res, &csrf_row, i);
        if (rv == -1) {
            return CONV_FAIL;
        }

        entry = apr_dbd_get_entry(driver, csrf_row, 0);        
        rule = apr_psprintf(ptemp, "<Location %s>", entry);
        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, rule);

        *result = apr_array_append(tpool, *result, inner_rule);        

        rule = apr_psprintf(ptemp, "</Location>");
        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, rule);        
    }  

    /* ����LocationMatch���� */
    csrf_res = NULL;
    csrf_row = NULL;
    state = apr_psprintf(ptemp, "SELECT value FROM %s WHERE keyword_id IN (SELECT keyword_id"\
                                    " FROM %s where sec_policy = '%s' and sec_subpolicy = %d"\
                                    " and type = %d)", KEYWORD_TAB, KEYWORD_ID_TAB, 
                                    sec_name,  CSRF, CSRF_URL_REGEX);
    rv = apr_dbd_select(driver, ptemp, tmp_db, &csrf_res, state, 1);
    if (rv > 0) {
        return CONV_FAIL;
    }
    rv = apr_dbd_num_tuples(driver, csrf_res);
    if (rv == -1) {
        return CONV_FAIL;
    } 

    num = rv;
    tpool = (*result)->pool;
    for (i = 1; i <= num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, csrf_res, &csrf_row, i);
        if (rv == -1) {
            return CONV_FAIL;
        }

        entry = apr_dbd_get_entry(driver, csrf_row, 0);        
        rule = apr_psprintf(ptemp, "<LocationMatch %s>", entry);
        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, rule);

        *result = apr_array_append(tpool, *result, inner_rule);        

        rule = apr_psprintf(ptemp, "</LocationMatch>");
        new = (char **)apr_array_push(*result);
        *new = apr_pstrdup(tpool, rule);        
    }
    return CONV_OK;
}

static int generate_rule(const char *sec_name, const char *start, const char *end, 
                                apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 rv, num, i;
    char                *state, **new;
    const char          *ptype, *entry, *rule;
    apr_dbd_results_t   *csrf_res = NULL;
    apr_dbd_row_t       *csrf_row = NULL, *rule_row = NULL; 
    apr_pool_t          *tpool;

    ptype = apr_dbd_escape(driver, ptemp, policy_type[CSRF], tmp_db);

    if (end == NULL) {
        state = apr_psprintf(ptemp, "select * from %s left join %s on %s.list_id = "\
                             "%s.list_id where type = '%s' and %s.list_id < %s",
                             BASIC_TAB, EXTEND_TAB, BASIC_TAB, EXTEND_TAB, ptype, BASIC_TAB, start);
    } else if (start == NULL) {
        state = apr_psprintf(ptemp, "select * from %s left join %s on %s.list_id = "\
                             "%s.list_id where type = '%s' and %s.list_id > %s",
                             BASIC_TAB, EXTEND_TAB, BASIC_TAB, EXTEND_TAB, ptype, BASIC_TAB, end);
    } else if ((end == NULL) && (start == NULL)) {
        state = apr_psprintf(ptemp, "select * from %s left join %s on %s.list_id = "\
                             "%s.list_id where type = '%s'",
                             BASIC_TAB, EXTEND_TAB, BASIC_TAB, EXTEND_TAB, ptype);
    } else {
        state = apr_psprintf(ptemp, "select * from %s left join %s on %s.list_id = "\
                             "%s.list_id where type = '%s' and %s.list_id < %s and %s.list_id > %s",
                             BASIC_TAB, EXTEND_TAB, BASIC_TAB, EXTEND_TAB, ptype, BASIC_TAB, start,
                             BASIC_TAB, end);
    }
    
    rv = apr_dbd_select(driver, ptemp, default_db, &csrf_res, state, 1);
    if (rv > 0) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "WAF_CONVERT: default policy is destroyed");
        return CONV_FAIL;
    }   

    rv = apr_dbd_num_tuples(driver, csrf_res);
    if (rv == -1) {
        return CONV_FAIL;
    } else if (rv == 0) {
        return CONV_OK;
    }

    num = rv;
    for (i = 1; i <= num; i++) {
        rv = apr_dbd_get_row(driver, ptemp, csrf_res, &csrf_row, i);
        if (rv) {
            return CONV_FAIL;
        }

        entry = apr_dbd_get_entry(driver, csrf_row, B_FLAG);
        if (!entry) {
            combined_rule(csrf_row, NULL, NULL, result, ptemp);
            continue;
        } else if (!strcmp(entry, cookie_name)) {
            combined_rule(csrf_row, NULL, NULL, result, ptemp);
            /* �����������һ������ */
            rv = apr_dbd_get_row(driver, ptemp, csrf_res, &rule_row, i+1);
            if (rv) {
                return CONV_FAIL;
            }
            
            combined_rule(rule_row, NULL, NULL, result, ptemp);
            new = apr_array_pop(*result);
            rule = *new;

            new = (char **)apr_array_push(*result);
            tpool = (*result)->pool;
            *new = apr_pstrdup(tpool, rule); 
        
            combind_csrf_cookie(sec_name, csrf_row, rule, result, ptemp);
            i++;
        }
    }    

    return CONV_OK;
}

static int csrf_subpolicy_query(const char *name, apr_dbd_row_t *row,
                                    apr_array_header_t **result, apr_pool_t *ptemp)
{
    int                 rv, i;
    const char          *entry, *sec_name;
    const char          *start, *end;
    apr_array_header_t  *inner_rule;
    apr_pool_t          *tpool;
    

    sec_name = apr_dbd_escape(driver, ptemp, name, tmp_db);

    /* ��������Location���еĹ��� */
    tpool = (*result)->pool;
    inner_rule = apr_array_make(tpool, 1, sizeof(char *));
    rv = combind_inner_rule(&inner_rule, &start, &end, ptemp);
    if (rv != CONV_OK) {
        return rv;
    }

    /* ���ζ�ȡ�����ɹ��� */
    rv = generate_rule(sec_name, start, NULL, result, ptemp);
    if (rv != CONV_OK) {
        return rv;
    }    

    /* ����Location�ι��� */
    rv = combind_csrf_url(sec_name, inner_rule, result, ptemp);
    if (rv != CONV_OK) {
        return rv;
    }    

    /* �������ɹ��� */
    rv = generate_rule(sec_name, NULL, end, result, ptemp);
    if (rv != CONV_OK) {
        return rv;
    }

    return CONV_OK;
}

static int csrf_subpolicy_del(const char *sec_policy, apr_pool_t *ptemp)
{
    int rv;

    rv = del_all_keyword_with_id(sec_policy, CSRF, CSRF_COOKIE_NAME, ptemp);
    if (rv != CONV_OK) {
        return rv;
    }    

    rv = del_all_keyword(sec_policy, CSRF, CSRF_URL_PLAIN, ptemp);
    if (rv != CONV_OK) {
        return rv;
    }   

    rv = del_all_keyword(sec_policy, CSRF, CSRF_URL_REGEX, ptemp);
    if (rv != CONV_OK) {
        return rv;
    }   

    return CONV_OK;
}

static int csrf_subpolicy_list(const char *sec_policy,
                                        apr_array_header_t **result, apr_pool_t *ptemp)
{
    int rv;
    
    rv = new_keyword_list(sec_policy, CSRF, CSRF_COOKIE_NAME, NULL, ASCII, result, ptemp);
    if (rv != CONV_OK) {
        return rv;
    } 

    rv = new_keyword_list(sec_policy, CSRF, CSRF_URL_PLAIN, NULL, ASCII, result, ptemp);
    if (rv != CONV_OK) {
        return rv;
    }     

    rv = new_keyword_list(sec_policy, CSRF, CSRF_URL_REGEX, NULL, ASCII, result, ptemp);
    if (rv != CONV_OK) {
        return rv;
    } 

    return CONV_OK;
}

/* ���󷽷����������ṹ */
subpolicy_t csrf_subpolicy = {
    CSRF,
    csrf_keyword_add,
    csrf_keyword_del,
    csrf_subpolicy_query,
    csrf_subpolicy_del,
    csrf_subpolicy_list
};

