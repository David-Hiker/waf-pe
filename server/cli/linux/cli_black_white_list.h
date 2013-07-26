/*
 * $Id: cli_black_white_list.h 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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
#define DYN_BLIST_DEFAULT_TIMEOUT      60

/* �����ݽṹ������ʶ�ڰ�������commit�����Լ��Ƿ���뵽ģʽ�У�����show running-config */
typedef struct blackwhite_flag_t blackwhite_flag_t;
struct blackwhite_flag_t {
    int ipblack_flag;                   /* ������ʶ�Ƿ���뵽ip������ģʽ */                   
    int ipwhite_flag;                   /* ������ʶ�Ƿ���뵽ip������ģʽ */
    int urlblack_flag;                  /* ������ʶ�Ƿ���뵽URL������ģʽ */
    int urlwhite_flag;                  /* ������ʶ�Ƿ���뵽URL������ģʽ */
    int ipblack_commit_flag;            /* ������ʶ�Ƿ���ip������ģʽ������commit���� */        
    int ipwhite_commit_flag;            /* ������ʶ�Ƿ���ip������ģʽ������commit���� */
    int urlblack_commit_flag;           /* ������ʶ�Ƿ���url������ģʽ������commit���� */
    int urlwhite_commit_flag;           /* ������ʶ�Ƿ���url������ģʽ������commit���� */
};

