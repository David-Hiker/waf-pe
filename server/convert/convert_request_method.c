/*
 * $Id: convert_request_method.c 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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

static char *method_flag = "request-method";
static int request_method_subpolicy_del(const char *sec_policy, apr_pool_t *ptemp);

static int request_method_keyword_add(keyword_t *k, apr_pool_t *ptemp)
{
    return new_keyword_add(k, method_flag, ptemp);
}

static int request_method_keyword_del(keyword_t *k, apr_pool_t *ptemp)
{
    return new_keyword_del(k, ptemp);
}

static int request_method_subpolicy_query(const char *name, apr_dbd_row_t *row,
                                    apr_array_header_t **result, apr_pool_t *ptemp)
{
    return new_keyword_query(name, REQUEST_METHOD, row, 0, result, ptemp);
}

static int request_method_subpolicy_del(const char *sec_policy, apr_pool_t *ptemp)
{
    return sub_without_new_del(sec_policy, REQUEST_METHOD, ptemp);
}

static int request_method_subpolicy_list(const char *sec_policy,
                                    apr_array_header_t **result, apr_pool_t *ptemp)
{
    return new_keyword_list(sec_policy, REQUEST_METHOD, 0, method_flag, ASCII, result, ptemp);
}

/* 请求方法处理驱动结构 */
subpolicy_t request_method_subpolicy = {
    REQUEST_METHOD,
    request_method_keyword_add,
    request_method_keyword_del,
    request_method_subpolicy_query,
    request_method_subpolicy_del,
    request_method_subpolicy_list
};

