/**
 * @file  convert_rule.h
 * @brief Rule Convert Modules functions
 *
 * @defgroup APACHE_CONVERT Rule Convert Modules
 * @ingroup  APACHE
 * @{
 */

#ifndef _APACHE_CONVERT_RULE_H_
#define _APACHE_CONVERT_RULE_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "cli_common.h"

#define URL_BWLIST_ENTRY_NUM    128
#define IP_BWLIST_ENTRY_NUM     128

#define IP_BLACK_TIMEOUT        "60"
#define URL_BLACK_TIMEOUT       "60"

#define FLAG_ALL                1

#define OUTER_KEYWORD           0
#define INNER_KEYWORD           1

/* ������ */
#define CONV_OK                 0       /* �����ɹ� */
#define CONV_FAIL               -1      /* ����ʧ�� */
#define CONV_CONFLICT           -2      /* ���������� */
#define CONV_OVERFLOW           -3      /* �������� */
#define CONV_NOTEXIST           -4      /* ���ݲ����� */
#define CONV_EXIST              -5      /* �����Ѿ����������ÿ��� */

/* ������� */
enum variable {
    VARIABLE_URL = 1,
    VARIABLE_POST,
    VARIABLE_COOKIE,
    VARIABLE_REQUEST,
    VARIABLE_RESPONSE
};

enum operator {
    OPERATOR_EQ = 1,
    OPERATOR_GT,
    OPERATOR_GTE,
    OPERATOR_LT,
    OPERATOR_LTE,
    OPERATOR_MATCH,
    OPERATOR_NONMATCH
};

enum action {
    ACTION_DROP = 1,
    ACTION_DENY,
    ACTION_PASS
};

enum status {
    DISABLE,
    ENABLE
};

enum method {
    PUT = 1,
    HEAD,
    OPTIONS,
    DETELETE,
    TRACE,
    CONNECT,
    OTHER
};

/* �������� */
typedef enum {
    UTF_8 = 1,
    BIG5,
    GB2312,
    GBK,
    GB18030
}encode_type_t;

enum information_leakage_type {
    PLAIN = 1,
    REGEX
};

enum weakpasswd_keyword_type {
    PLAIN_URL = 1,
    REGEX_URL,
    PLAIN_PLAIN,
    PLAIN_REGEX,
    REGEX_PLAIN,
    REGEX_REGEX
};

enum weakpasswd_flag {
    WP_CHECK_URL = 1,                   /* ���� url {plain url-string | regular-exp url-string} password-name string */
    WP_NO_URL_ALL,                      /* ���� no url all */
    WP_NO_URL,                          /* ���� no url {plain url-string | regular-exp url-string} */
    WP_NO_KEYWORD_ALL,                  /* ����no keyword all */
    WP_KEYWORD,                         /* �������ӻ�ɾ�������ؼ��� */
    WP_KEYWORD_DEFAULT                  /* ��������Ĭ�ϵĹؼ��� */
};

/* ��ȫ�Ӳ��Ե������ */
/* Э�����������ȫ�Ӳ��Թؼ������ */
enum protocol_keyword {
    REQUEST_HEADER_NUM = 1,
    HEADER_SIZE_EXCEPT_COOKIE,
    COOKIE_SIZE,
    REQUEST_URL_SIZE,
    QUERY_STRING_SIZE,
    ARGUMENT_NUM,
    ARGUMENT_SIZE,
    BODY_SIZE,
    ARGUMENT_NAME_SIZE,
    ARGUMENT_NAME_VALUE_SIZE,
    END_PROTOCOL
};

/* �ؼ��ֹ��˰�ȫ�Ӳ��Թؼ������ */
enum keyword_filter {
    URL = 1,
    KEY_COOKIE,
    POST,
    REQUEST_BODY,
    RESPONSE_BODY
};

/* �ļ��ϴ���ȫ�Ӳ��Թؼ������ */
enum fileupload_keyword {
    FILE_TYPE = 1,
    INDIVIDUAL_FILE_SIZE,
    ALL_FILE_SIZE
};

/* Cookie������ȫ�Ӳ��Թؼ������ */
enum cookie {
    COOKIE_NAME = 1,
    EXPIRE_TIME,
    VERIFICATION,
    ATTRIBUTION
};

enum verification_method_group_one {
    SIGNATURE = 1,
    ENCRYPTION
};

enum verification_method_group_two {
    COOKIE_VERTFICATION = 1,
    IP_AND_COOKIE
};

enum attribution {
    SECURE = 1,
    HTTPDONLY,
    SECURE_HTTPONLY
};

enum cc_keyword {
    SOURCE_IP = 1,
    REFERRER_URL,
    STATUS_408_RATE,
    CC_PLAIN_URL,
    CC_REGEX_URL,
    URL_ACCESS_RATE,
    CC_KEYWORD_ALL
};

enum csrf_keyword {
    CSRF_COOKIE_NAME = 1,
    CSRF_URL_PLAIN,
    CSRF_URL_REGEX,
    CSRF_URL_ALL
};

/* ��ȫ�Ӳ������ */
typedef enum {
    SQL_INJECTION,          /* SQLע����� */
    LDAP_INJECTION,             /* LDAPע����� */
    EMAIL_INJECTION,            /* EMAILע����� */
    COMMAND_INJECTION,          /* COMMANDע����� */
    CODE_INJECTION,             /* CODEע����� */
    NULL_BYTE_INJECTION,        /* NULL BYTEע����� */
    XSS,                        /* XSS�������� */
    CSRF,                       /* CSRF�������� */
    OVERFLOW,                   /* ����������� */
    FILE_INCLUDE,               /* �ļ������������� */
    BASE_ATTACK,                /* ������������ */
    PATH_TRAVERSAL,             /* ·�������������� */
    DIRECTORY_INDEX,            /* Ŀ¼�������� */
    SPIDER_SCANNER,             /* ������ɨ�蹥������ */
    TROJAN,                     /* ľ����� */
    XML_ATTACK,                 /* XML�������� */
    WEAK_PASSWORD,              /* ��������� */
    SERVER_VERSION,             /* �������汾��Ϣй¶���� */
    HTTP_STATUS_CODE,           /* HTTP״̬����Ϣй¶���� */
    ICCARD_INFORMATION,         /* IC����Ϣй©���� */
    SERVER_ERROR_INFO,          /* ������������Ϣй©���� */
    PROGRAM_CODE,               /* ���������Ϣй¶���� */
    MAGCARD_INFORMATION,        /* ���ôſ���Ϣ���� */
    IDCARD_INFORMATION,         /* ����ID����Ϣ���� */
    FILE_DOWNLOAD,              /* �����ļ����� */
    FILE_UPLOAD,                /* �����ļ����� */
    COOKIE,                     /* COOKIE���� */
    PROTOCOL_PARAM,             /* Э��������� */
    REQUEST_METHOD,             /* ���󷽷����� */
    KEYWORD_FILTER,             /* �ؼ��ֹ��� */
    CC_PROTECT,                 /* CC���� */
    MAIN_CONFIG,                /* �������ļ� */
    MAX_SUBPOLICY               /* ���ȫ�Ӳ��Ա�� */
}  subpolicy_type_t;

/* �ڰ�������ض��� */
enum black_white_lst {
    IP_BLACK = 0,
    IP_WHITE,
    URL_BLACK,
    URL_WHITE,
    ALL_LIST
};

/* IP����������� */
enum ip_black_type {
    IB_CLIENT_IP = 0,
    IB_DYN_TIMEOUT,
    IB_DYN_EXCEPT,
    IB_DYN_CLEAR
};

/* IP����������� */
enum ip_white_type {
    IW_CLIENT_IP = 0,
    IW_SERVER_IP,
    IW_SERVER_HOST
};

/* URL����������� */
enum url_black_type {
    UB_URL_PLAIN = 0,
    UB_URL_REGEX,
    UB_DYN_TIMEOUT,
    UB_DYN_EXCEPT,
    UB_DYN_CLEAR
};

/* URL����������� */
enum url_white_type {
    UW_URL_PLAIN = 0,
    UW_URL_REGEX
};

/* ͨ�õĹؼ��� all */
#define KEYWORD_ALL             "all"
#define KEYWORD_DEFAULT         "default"
#define KEYWORD_ALL_FLAG        1
#define KEYWORD_DEFAULT_FLAG    2

extern encode_type_t encode_type;           /* �������� */

/* �ж������Ƿ�ȫ��ΪASCII�ַ� */
extern int is_ascii(char *str);

/* ת���ӿ� */

/**
 * Set up for rule convert.
 * @param p The pool for save persistent data
 * @param ptrans Pool for save transient data
 */
int convert_init(apr_pool_t *p, apr_pool_t *pconf);

/* ��ȫ����ת����ؽӿ� */

/**
 * Delete security policy
 * @param name The policy to be deleted
 * @return
 *      CONV_OK         succeed
 *      CONV_FAIL       failure
 *      CONV_NOTEXIST   not exist
 */
int convert_sec_policy_del(const char *name);

/**
 * Query for security policy directive
 * @param name The security policy to be query
 * @param result An array of all rule string in this security policy
 * @return OK (succeed) or others( failure)
 */
int convert_sec_policy_query(const char *name, apr_array_header_t **result);

/**
 * Set security sub policy
 * @param s The security sub policy to be set
 * @return OK (succeed) or others( failure)
 */
int convert_sec_subpolicy_set(sec_subpolicy_t *s);

/**
 * Add security sub policy keyword
 * @param k The keyword to be added
 * @return
 *      CONV_OK         succeed
 *      CONV_FAIL       failure
 *      CONV_CONFLICT   keyword already exist
 */
int convert_keyword_add(keyword_t *k);

/**
 * Delete security sub policy keyword
 * @param k The keyword to be deleted
 * @return
 *      CONV_OK         succeed
 *      CONV_FAIL       failure
 *      CONV_NOTEXIST   not exist
 */
int convert_keyword_del(keyword_t *k);

/**
 * list the security policy's information
 * @sec_policy: the name of the policy
 * @sec_subpolicy: the subpolicy, see also subpolicy_type_t.
 * @result: the result to be shown *
 * @return: CONV_OK (succeed) or CONV_FAIL(failure)
 */
int convert_sec_policy_list(char *sec_policy, int sec_subpolicy, apr_array_header_t **result);


/* �����б��ڰ�������ת����ؽӿ� */

/**
 * Set the access control list
 * @param lst Access control list. Including black and white lists.
 * @param type Set the access control object type. For example, the client Ip
 * @param data Access control data. For example, IP address, time
 * @return
 *      CONV_OK         succeed
 *      CONV_FAIL       failure
 *      CONV_CONFLICT   already existed
 *      CONV_OVERFLOW   overflow
 */
int convert_access_list_set(int lst, int type, char *data);

/**
 * Clear the access control list
 * @param lst Access control list. Including black and white lists.
 * @param type Set the access control object type. For example, the client Ip
 * @param data Access control data. For example, IP address
 * @return CONV_OK (succeed) or CONV_FAIL( failure)
 */
int convert_access_list_clear(int lst, int type, char *data);

/**
 * Query for the access control list directive
 * @param lst Access control list. Including black and white
 *            lists.
 * @param result An array of all access list string
 * @return CONV_OK (succeed) or CONV_FAIL(failure)
 */
int convert_access_list_query(int lst, apr_array_header_t **result);

/**
 * Show the access control list data
 * @param lst Access control list. Including black and white
 *            lists.
 * @param type Set the access control object type. For example, the client Ip
 * @param result An array of all access list string
 * @return CONV_OK (succeed) or CONV_FAIL(failure)
 */
int convert_access_list_show(int lst, int type, apr_array_header_t **result);


#ifdef __cplusplus
}
#endif

#endif /* !APACHE_CONVERT_RULE_H */
/** @} */

