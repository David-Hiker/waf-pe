/*
 * $Id: convert_server_version.c 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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

static char *server_rule = "SecRule  RESPONSE_HEADERS:Server  \"@rsub  s/%s/%s/\" \"phase:3,"\
                           "t:none,block,msg:'server version Information  Leakage', id:'%s',"\
                           "tag:'information leakage', severity:'3'\"";

static int server_version_subpolicy_del(const char *sec_policy, apr_pool_t *ptemp);

static int server_version_keyword_add(keyword_t *k, apr_pool_t *ptemp)
{
    return add_two_keyword(k, ptemp);
}

static int server_version_keyword_del(keyword_t *k, apr_pool_t *ptemp)
{
    if (k->flag == FLAG_ALL) {
        return server_version_subpolicy_del(k->sec_policy, ptemp);
    }

    return delete_two_keyword(k, ptemp, KEYWORD_SECOND);
}

static int server_version_subpolicy_query(const char *name, apr_dbd_row_t *orig_row,
                                        apr_array_header_t **result, apr_pool_t *ptemp)
{
    int rv;

    combined_rule(orig_row, NULL, NULL, result, ptemp);
    rv = query_info_leakage(name, SERVER_VERSION, server_rule, result, ptemp);
    if (rv != CONV_OK) {
        return rv;
    } else {
        return CONV_OK;
    }
}

static int server_version_subpolicy_del(const char *sec_policy, apr_pool_t *ptemp)
{
    return sub_with_new_del(sec_policy, SERVER_VERSION, ptemp);
}

static int server_version_subpolicy_list(const char *sec_policy,
                                        apr_array_header_t **result, apr_pool_t *ptemp)
{
    return list_two_keyword(sec_policy, SERVER_VERSION, result, ptemp);
}

subpolicy_t server_version_subpolicy = {
    SERVER_VERSION,
    server_version_keyword_add,
    server_version_keyword_del,
    server_version_subpolicy_query,
    server_version_subpolicy_del,
    server_version_subpolicy_list
};

