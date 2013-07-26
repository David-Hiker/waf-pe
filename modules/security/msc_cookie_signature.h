/*
 * $Id: msc_cookie_signature.h 2786 2013-07-09 16:42:55 FreeWAF Development Team $
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
 
/**
 * get_cookie_signature - ��ȡcookieǩ��
 * @msr: ����������
 * @cookie_name_tb: ��������ǩ����cookiename�嵥
 * @need_ip: ����ǩ��ʱ�Ƿ���Ҫip
 *
 * �ɹ�����ǩ���ַ�����ʧ��ʱ����NULL
 */
extern char * get_cookie_signature(modsec_rec *msr, apr_table_t *cookie_name_tb, int need_ip);

