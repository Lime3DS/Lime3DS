/* Getopt for Microsoft C
This code is a modification of the Free Software Foundation, Inc.
Getopt library for parsing command line argument the purpose was
to provide a Microsoft Visual C friendly derivative. This code
provides functionality for both Unicode and Multibyte builds.

Date: 02/03/2011 - Ludvik Jerabek - Initial Release
Version: 1.1
Comment: Supports getopt, getopt_long, and getopt_long_only
and POSIXLY_CORRECT environment flag
License: LGPL

Revisions:

02/03/2011 - Ludvik Jerabek - Initial Release
02/20/2011 - Ludvik Jerabek - Fixed compiler warnings at Level 4
07/05/2011 - Ludvik Jerabek - Added no_argument, required_argument, optional_argument defs
08/03/2011 - Ludvik Jerabek - Fixed non-argument runtime bug which caused runtime exception
08/09/2011 - Ludvik Jerabek - Added code to export functions for DLL and LIB
02/15/2012 - Ludvik Jerabek - Fixed _GETOPT_THROW definition missing in implementation file
08/01/2012 - Ludvik Jerabek - Created separate functions for char and wchar_t characters so single dll can do both unicode and ansi
10/15/2012 - Ludvik Jerabek - Modified to match latest GNU features
06/19/2015 - Ludvik Jerabek - Fixed maximum option limitation caused by option_a (255) and option_w (65535) structure val variable
09/24/2022 - Ludvik Jerabek - Updated to match most recent getopt release
09/25/2022 - Ludvik Jerabek - Fixed memory allocation (malloc call) issue for wchar_t* 

**DISCLAIMER**
THIS MATERIAL IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING, BUT Not LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE, OR NON-INFRINGEMENT. SOME JURISDICTIONS DO NOT ALLOW THE
EXCLUSION OF IMPLIED WARRANTIES, SO THE ABOVE EXCLUSION MAY NOT
APPLY TO YOU. IN NO EVENT WILL I BE LIABLE TO ANY PARTY FOR ANY
DIRECT, INDIRECT, SPECIAL OR OTHER CONSEQUENTIAL DAMAGES FOR ANY
USE OF THIS MATERIAL INCLUDING, WITHOUT LIMITATION, ANY LOST
PROFITS, BUSINESS INTERRUPTION, LOSS OF PROGRAMS OR OTHER DATA ON
YOUR INFORMATION HANDLING SYSTEM OR OTHERWISE, EVEN If WE ARE
EXPRESSLY ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/
#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include "getopt.h"

#ifdef __cplusplus
#define _GETOPT_THROW throw()
#else
#define _GETOPT_THROW
#endif

int optind = 1;
int opterr = 1;
int optopt = '?';
enum ENUM_ORDERING { REQUIRE_ORDER, PERMUTE, RETURN_IN_ORDER };

//
//
//		Ansi structures and functions follow
// 
//

static struct _getopt_data_a
{
	int optind;
	int opterr;
	int optopt;
	char* optarg;
	int __initialized;
	char* __nextchar;
	enum ENUM_ORDERING __ordering;
	int __first_nonopt;
	int __last_nonopt;
} getopt_data_a;
char* optarg_a;

static void exchange_a(char** argv, struct _getopt_data_a* d)
{
	int bottom = d->__first_nonopt;
	int middle = d->__last_nonopt;
	int top = d->optind;
	char* tem;
	while (top > middle && middle > bottom)
	{
		if (top - middle > middle - bottom)
		{
			int len = middle - bottom;
			int i;
			for (i = 0; i < len; i++)
			{
				tem = argv[bottom + i];
				argv[bottom + i] = argv[top - (middle - bottom) + i];
				argv[top - (middle - bottom) + i] = tem;
			}
			top -= len;
		}
		else
		{
			int len = top - middle;
			int i;
			for (i = 0; i < len; i++)
			{
				tem = argv[bottom + i];
				argv[bottom + i] = argv[middle + i];
				argv[middle + i] = tem;
			}
			bottom += len;
		}
	}
	d->__first_nonopt += (d->optind - d->__last_nonopt);
	d->__last_nonopt = d->optind;
}

static int process_long_option_a(int argc, char** argv, const char* optstring, const struct option_a* longopts, int* longind, int long_only, struct _getopt_data_a* d, int print_errors, const char* prefix)
{
	assert(longopts != NULL);
	char* nameend;
	size_t namelen;
	const struct option_a* p;
	const struct option_a* pfound = NULL;
	int n_options;
	int option_index = 0;
	for (nameend = d->__nextchar; *nameend && *nameend != '='; nameend++);
	namelen = nameend - d->__nextchar;
	for (p = longopts, n_options = 0; p->name; p++, n_options++)
		if (!strncmp(p->name, d->__nextchar, namelen) && namelen == strlen(p->name))
		{
			pfound = p;
			option_index = n_options;
			break;
		}
	if (pfound == NULL)
	{
		unsigned char* ambig_set = NULL;
		int ambig_fallback = 0;
		int indfound = -1;
		for (p = longopts, option_index = 0; p->name; p++, option_index++)
			if (!strncmp(p->name, d->__nextchar, namelen))
			{
				if (pfound == NULL)
				{
					pfound = p;
					indfound = option_index;
				}
				else if (long_only
					|| pfound->has_arg != p->has_arg
					|| pfound->flag != p->flag
					|| pfound->val != p->val)
				{
					if (!ambig_fallback)
					{
						if (!print_errors)
							ambig_fallback = 1;

						else if (!ambig_set)
						{
							if ((ambig_set = malloc(n_options * sizeof(char))) == NULL)
								ambig_fallback = 1;

							if (ambig_set)
							{
								memset(ambig_set, 0, n_options * sizeof(char));
								ambig_set[indfound] = 1;
							}
						}
						if (ambig_set)
							ambig_set[option_index] = 1;
					}
				}
			}
		if (ambig_set || ambig_fallback)
		{
			if (print_errors)
			{
				if (ambig_fallback)
					fprintf(stderr, "%s: option '%s%s' is ambiguous\n", argv[0], prefix, d->__nextchar);
				else
				{
					_lock_file(stderr);
					fprintf(stderr, "%s: option '%s%s' is ambiguous; possibilities:", argv[0], prefix, d->__nextchar);
					for (option_index = 0; option_index < n_options; option_index++)
						if (ambig_set[option_index])
							fprintf(stderr, " '%s%s'", prefix, longopts[option_index].name);
					fprintf(stderr, "\n");
					_unlock_file(stderr);
				}
			}
			free(ambig_set);
			d->__nextchar += strlen(d->__nextchar);
			d->optind++;
			d->optopt = 0;
			return '?';
		}
		option_index = indfound;
	}
	if (pfound == NULL)
	{
		if (!long_only || argv[d->optind][1] == '-'
			|| strchr(optstring, *d->__nextchar) == NULL)
		{
			if (print_errors)
				fprintf(stderr, "%s: unrecognized option '%s%s'\n", argv[0], prefix, d->__nextchar);
			d->__nextchar = NULL;
			d->optind++;
			d->optopt = 0;
			return '?';
		}
		return -1;
	}
	d->optind++;
	d->__nextchar = NULL;
	if (*nameend)
	{
		if (pfound->has_arg)
			d->optarg = nameend + 1;
		else
		{
			if (print_errors)
				fprintf(stderr, "%s: option '%s%s' doesn't allow an argument\n", argv[0], prefix, pfound->name);
			d->optopt = pfound->val;
			return '?';
		}
	}
	else if (pfound->has_arg == 1)
	{
		if (d->optind < argc)
			d->optarg = argv[d->optind++];
		else
		{
			if (print_errors)
				fprintf(stderr, "%s: option '%s%s' requires an argument\n", argv[0], prefix, pfound->name);
			d->optopt = pfound->val;
			return optstring[0] == ':' ? ':' : '?';
		}
	}
	if (longind != NULL)
		*longind = option_index;

	if (pfound->flag)
	{
		*(pfound->flag) = pfound->val;
		return 0;
	}
	return pfound->val;
}

static const char* _getopt_initialize_a(const char* optstring, struct _getopt_data_a* d, int posixly_correct)
{
	if (d->optind == 0)
		d->optind = 1;

	d->__first_nonopt = d->__last_nonopt = d->optind;
	d->__nextchar = NULL;

	if (optstring[0] == '-')
	{
		d->__ordering = RETURN_IN_ORDER;
		++optstring;
	}
	else if (optstring[0] == '+')
	{
		d->__ordering = REQUIRE_ORDER;
		++optstring;
	}
	else if (posixly_correct | !!getenv("POSIXLY_CORRECT"))
		d->__ordering = REQUIRE_ORDER;
	else
		d->__ordering = PERMUTE;

	d->__initialized = 1;
	return optstring;
}

int _getopt_internal_r_a(int argc, char* const* argv, const char* optstring, const struct option_a* longopts, int* longind, int long_only, struct _getopt_data_a* d, int posixly_correct)
{
	int print_errors = d->opterr;
	if (argc < 1)
		return -1;
	d->optarg = NULL;
	if (d->optind == 0 || !d->__initialized)
		optstring = _getopt_initialize_a(optstring, d, posixly_correct);
	else if (optstring[0] == '-' || optstring[0] == '+')
		optstring++;
	if (optstring[0] == ':')
		print_errors = 0;

	if (d->__nextchar == NULL || *d->__nextchar == '\0')
	{
		if (d->__last_nonopt > d->optind)
			d->__last_nonopt = d->optind;
		if (d->__first_nonopt > d->optind)
			d->__first_nonopt = d->optind;
		if (d->__ordering == PERMUTE)
		{
			if (d->__first_nonopt != d->__last_nonopt
				&& d->__last_nonopt != d->optind)
				exchange_a((char**)argv, d);
			else if (d->__last_nonopt != d->optind)
				d->__first_nonopt = d->optind;
			while (d->optind < argc && (argv[d->optind][0] != '-' || argv[d->optind][1] == '\0'))
				d->optind++;
			d->__last_nonopt = d->optind;
		}
		if (d->optind != argc && !strcmp(argv[d->optind], "--"))
		{
			d->optind++;
			if (d->__first_nonopt != d->__last_nonopt
				&& d->__last_nonopt != d->optind)
				exchange_a((char**)argv, d);
			else if (d->__first_nonopt == d->__last_nonopt)
				d->__first_nonopt = d->optind;
			d->__last_nonopt = argc;
			d->optind = argc;
		}
		if (d->optind == argc)
		{
			if (d->__first_nonopt != d->__last_nonopt)
				d->optind = d->__first_nonopt;
			return -1;
		}
		if (argv[d->optind][0] != '-' || argv[d->optind][1] == '\0')
		{
			if (d->__ordering == REQUIRE_ORDER)
				return -1;
			d->optarg = argv[d->optind++];
			return 1;
		}
		if (longopts)
		{
			if (argv[d->optind][1] == '-')
			{
				d->__nextchar = argv[d->optind] + 2;
				return process_long_option_a(argc, (char**)argv, optstring, longopts, longind, long_only, d, print_errors, "--");
			}
			if (long_only && (argv[d->optind][2]
				|| !strchr(optstring, argv[d->optind][1])))
			{
				int code;
				d->__nextchar = argv[d->optind] + 1;
				code = process_long_option_a(argc, (char**)argv, optstring, longopts,
					longind, long_only, d,
					print_errors, "-");
				if (code != -1)
					return code;
			}
		}
		d->__nextchar = argv[d->optind] + 1;
	}
	{
		char c = *d->__nextchar++;
		const char* temp = strchr(optstring, c);
		if (*d->__nextchar == '\0')
			++d->optind;
		if (temp == NULL || c == ':' || c == ';')
		{
			if (print_errors)
				fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], c);
			d->optopt = c;
			return '?';
		}
		if (temp[0] == 'W' && temp[1] == ';' && longopts != NULL)
		{
			if (*d->__nextchar != '\0')
				d->optarg = d->__nextchar;
			else if (d->optind == argc)
			{
				if (print_errors)
					fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], c);
				d->optopt = c;
				if (optstring[0] == ':')
					c = ':';
				else
					c = '?';
				return c;
			}
			else
				d->optarg = argv[d->optind];
			d->__nextchar = d->optarg;
			d->optarg = NULL;
			return process_long_option_a(argc, (char**)argv, optstring, longopts, longind, 0, d, print_errors, "-W ");
		}
		if (temp[1] == ':')
		{
			if (temp[2] == ':')
			{
				if (*d->__nextchar != '\0')
				{
					d->optarg = d->__nextchar;
					d->optind++;
				}
				else
					d->optarg = NULL;
				d->__nextchar = NULL;
			}
			else
			{
				if (*d->__nextchar != '\0')
				{
					d->optarg = d->__nextchar;
					d->optind++;
				}
				else if (d->optind == argc)
				{
					if (print_errors)
						fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], c);
					d->optopt = c;
					if (optstring[0] == ':')
						c = ':';
					else
						c = '?';
				}
				else
					d->optarg = argv[d->optind++];
				d->__nextchar = NULL;
			}
		}
		return c;
	}
}

int _getopt_internal_a(int argc, char* const* argv, const char* optstring, const struct option_a* longopts, int* longind, int long_only, int posixly_correct)
{
	int result;
	getopt_data_a.optind = optind;
	getopt_data_a.opterr = opterr;
	result = _getopt_internal_r_a(argc, argv, optstring, longopts, longind, long_only, &getopt_data_a, posixly_correct);
	optind = getopt_data_a.optind;
	optarg_a = getopt_data_a.optarg;
	optopt = getopt_data_a.optopt;
	return result;
}

int getopt_a(int argc, char* const* argv, const char* optstring) _GETOPT_THROW
{
	return _getopt_internal_a(argc, argv, optstring, (const struct option_a*)0, (int*)0, 0, 0);
}

int getopt_long_a(int argc, char* const* argv, const char* options, const struct option_a* long_options, int* opt_index) _GETOPT_THROW
{
	return _getopt_internal_a(argc, argv, options, long_options, opt_index, 0, 0);
}

int getopt_long_only_a(int argc, char* const* argv, const char* options, const struct option_a* long_options, int* opt_index) _GETOPT_THROW
{
	return _getopt_internal_a(argc, argv, options, long_options, opt_index, 1, 0);
}

int _getopt_long_r_a(int argc, char* const* argv, const char* options, const struct option_a* long_options, int* opt_index, struct _getopt_data_a* d)
{
	return _getopt_internal_r_a(argc, argv, options, long_options, opt_index, 0, d, 0);
}

int _getopt_long_only_r_a(int argc, char* const* argv, const char* options, const struct option_a* long_options, int* opt_index, struct _getopt_data_a* d)
{
	return _getopt_internal_r_a(argc, argv, options, long_options, opt_index, 1, d, 0);
}

//
//
//	Unicode Structures and Functions
// 
//

static struct _getopt_data_w
{
	int optind;
	int opterr;
	int optopt;
	wchar_t* optarg;
	int __initialized;
	wchar_t* __nextchar;
	enum ENUM_ORDERING __ordering;
	int __first_nonopt;
	int __last_nonopt;
} getopt_data_w;
wchar_t* optarg_w;

static void exchange_w(wchar_t** argv, struct _getopt_data_w* d)
{
	int bottom = d->__first_nonopt;
	int middle = d->__last_nonopt;
	int top = d->optind;
	wchar_t* tem;
	while (top > middle && middle > bottom)
	{
		if (top - middle > middle - bottom)
		{
			int len = middle - bottom;
			int i;
			for (i = 0; i < len; i++)
			{
				tem = argv[bottom + i];
				argv[bottom + i] = argv[top - (middle - bottom) + i];
				argv[top - (middle - bottom) + i] = tem;
			}
			top -= len;
		}
		else
		{
			int len = top - middle;
			int i;
			for (i = 0; i < len; i++)
			{
				tem = argv[bottom + i];
				argv[bottom + i] = argv[middle + i];
				argv[middle + i] = tem;
			}
			bottom += len;
		}
	}
	d->__first_nonopt += (d->optind - d->__last_nonopt);
	d->__last_nonopt = d->optind;
}

static int process_long_option_w(int argc, wchar_t** argv, const wchar_t* optstring, const struct option_w* longopts, int* longind, int long_only, struct _getopt_data_w* d, int print_errors, const wchar_t* prefix)
{
	assert(longopts != NULL);
	wchar_t* nameend;
	size_t namelen;
	const struct option_w* p;
	const struct option_w* pfound = NULL;
	int n_options;
	int option_index = 0;
	for (nameend = d->__nextchar; *nameend && *nameend != L'='; nameend++);
	namelen = nameend - d->__nextchar;
	for (p = longopts, n_options = 0; p->name; p++, n_options++)
		if (!wcsncmp(p->name, d->__nextchar, namelen) && namelen == wcslen(p->name))
		{
			pfound = p;
			option_index = n_options;
			break;
		}
	if (pfound == NULL)
	{
		wchar_t* ambig_set = NULL;
		int ambig_fallback = 0;
		int indfound = -1;
		for (p = longopts, option_index = 0; p->name; p++, option_index++)
			if (!wcsncmp(p->name, d->__nextchar, namelen))
			{
				if (pfound == NULL)
				{
					pfound = p;
					indfound = option_index;
				}
				else if (long_only
					|| pfound->has_arg != p->has_arg
					|| pfound->flag != p->flag
					|| pfound->val != p->val)
				{
					if (!ambig_fallback)
					{
						if (!print_errors)
							ambig_fallback = 1;

						else if (!ambig_set)
						{
							if ((ambig_set = malloc(n_options * sizeof(wchar_t))) == NULL)
								ambig_fallback = 1;

							if (ambig_set)
							{
								memset(ambig_set, 0, n_options * sizeof(wchar_t));
								ambig_set[indfound] = 1;
							}
						}
						if (ambig_set)
							ambig_set[option_index] = 1;
					}
				}
			}
		if (ambig_set || ambig_fallback)
		{
			if (print_errors)
			{
				if (ambig_fallback)
					fwprintf(stderr, L"%s: option '%s%s' is ambiguous\n", argv[0], prefix, d->__nextchar);
				else
				{
					_lock_file(stderr);
					fwprintf(stderr, L"%s: option '%s%s' is ambiguous; possibilities:", argv[0], prefix, d->__nextchar);
					for (option_index = 0; option_index < n_options; option_index++)
						if (ambig_set[option_index])
							fwprintf(stderr, L" '%s%s'", prefix, longopts[option_index].name);
					fwprintf(stderr, L"\n");
					_unlock_file(stderr);
				}
			}
			free(ambig_set);
			d->__nextchar += wcslen(d->__nextchar);
			d->optind++;
			d->optopt = 0;
			return L'?';
		}
		option_index = indfound;
	}
	if (pfound == NULL)
	{
		if (!long_only || argv[d->optind][1] == L'-' || wcschr(optstring, *d->__nextchar) == NULL)
		{
			if (print_errors)
				fwprintf(stderr, L"%s: unrecognized option '%s%s'\n", argv[0], prefix, d->__nextchar);
			d->__nextchar = NULL;
			d->optind++;
			d->optopt = 0;
			return L'?';
		}
		return -1;
	}
	d->optind++;
	d->__nextchar = NULL;
	if (*nameend)
	{
		if (pfound->has_arg)
			d->optarg = nameend + 1;
		else
		{
			if (print_errors)
				fwprintf(stderr, L"%s: option '%s%s' doesn't allow an argument\n", argv[0], prefix, pfound->name);
			d->optopt = pfound->val;
			return L'?';
		}
	}
	else if (pfound->has_arg == 1)
	{
		if (d->optind < argc)
			d->optarg = argv[d->optind++];
		else
		{
			if (print_errors)
				fwprintf(stderr, L"%s: option '%s%s' requires an argument\n", argv[0], prefix, pfound->name);
			d->optopt = pfound->val;
			return optstring[0] == L':' ? L':' : L'?';
		}
	}
	if (longind != NULL)
		*longind = option_index;
	if (pfound->flag)
	{
		*(pfound->flag) = pfound->val;
		return 0;
	}
	return pfound->val;
}

static const wchar_t* _getopt_initialize_w(const wchar_t* optstring, struct _getopt_data_w* d, int posixly_correct)
{
	if (d->optind == 0)
		d->optind = 1;

	d->__first_nonopt = d->__last_nonopt = d->optind;
	d->__nextchar = NULL;

	if (optstring[0] == L'-')
	{
		d->__ordering = RETURN_IN_ORDER;
		++optstring;
	}
	else if (optstring[0] == L'+')
	{
		d->__ordering = REQUIRE_ORDER;
		++optstring;
	}
	else if (posixly_correct | !!_wgetenv(L"POSIXLY_CORRECT"))
		d->__ordering = REQUIRE_ORDER;
	else
		d->__ordering = PERMUTE;

	d->__initialized = 1;
	return optstring;
}

int _getopt_internal_r_w(int argc, wchar_t* const* argv, const wchar_t* optstring, const struct option_w* longopts, int* longind, int long_only, struct _getopt_data_w* d, int posixly_correct)
{
	int print_errors = d->opterr;
	if (argc < 1)
		return -1;
	d->optarg = NULL;
	if (d->optind == 0 || !d->__initialized)
		optstring = _getopt_initialize_w(optstring, d, posixly_correct);
	else if (optstring[0] == L'-' || optstring[0] == L'+')
		optstring++;
	if (optstring[0] == L':')
		print_errors = 0;
#define NONOPTION_P (argv[d->optind][0] != L'-' || argv[d->optind][1] == L'\0')

	if (d->__nextchar == NULL || *d->__nextchar == L'\0')
	{
		if (d->__last_nonopt > d->optind)
			d->__last_nonopt = d->optind;
		if (d->__first_nonopt > d->optind)
			d->__first_nonopt = d->optind;
		if (d->__ordering == PERMUTE)
		{
			if (d->__first_nonopt != d->__last_nonopt
				&& d->__last_nonopt != d->optind)
				exchange_w((wchar_t**)argv, d);
			else if (d->__last_nonopt != d->optind)
				d->__first_nonopt = d->optind;
			while (d->optind < argc && NONOPTION_P)
				d->optind++;
			d->__last_nonopt = d->optind;
		}
		if (d->optind != argc && !wcscmp(argv[d->optind], L"--"))
		{
			d->optind++;
			if (d->__first_nonopt != d->__last_nonopt
				&& d->__last_nonopt != d->optind)
				exchange_w((wchar_t**)argv, d);
			else if (d->__first_nonopt == d->__last_nonopt)
				d->__first_nonopt = d->optind;
			d->__last_nonopt = argc;
			d->optind = argc;
		}
		if (d->optind == argc)
		{
			if (d->__first_nonopt != d->__last_nonopt)
				d->optind = d->__first_nonopt;
			return -1;
		}
		if (NONOPTION_P)
		{
			if (d->__ordering == REQUIRE_ORDER)
				return -1;
			d->optarg = argv[d->optind++];
			return 1;
		}
		if (longopts)
		{
			if (argv[d->optind][1] == L'-')
			{
				d->__nextchar = argv[d->optind] + 2;
				return process_long_option_w(argc, (wchar_t**)argv, optstring, longopts, longind, long_only, d, print_errors, L"--");
			}
			if (long_only && (argv[d->optind][2] || !wcschr(optstring, argv[d->optind][1])))
			{
				int code;
				d->__nextchar = argv[d->optind] + 1;
				code = process_long_option_w(argc, (wchar_t**)argv, optstring, longopts, longind, long_only, d, print_errors, L"-");
				if (code != -1)
					return code;
			}
		}
		d->__nextchar = argv[d->optind] + 1;
	}
	{
		wchar_t c = *d->__nextchar++;
		const wchar_t* temp = wcschr(optstring, c);
		if (*d->__nextchar == L'\0')
			++d->optind;
		if (temp == NULL || c == L':' || c == L';')
		{
			if (print_errors)
				fwprintf(stderr, L"%s: invalid option -- '%c'\n", argv[0], c);
			d->optopt = c;
			return L'?';
		}
		if (temp[0] == L'W' && temp[1] == L';' && longopts != NULL)
		{
			if (*d->__nextchar != L'\0')
				d->optarg = d->__nextchar;
			else if (d->optind == argc)
			{
				if (print_errors)
					fwprintf(stderr, L"%s: option requires an argument -- '%c'\n", argv[0], c);
				d->optopt = c;
				if (optstring[0] == L':')
					c = L':';
				else
					c = L'?';
				return c;
			}
			else
				d->optarg = argv[d->optind];
			d->__nextchar = d->optarg;
			d->optarg = NULL;
			return process_long_option_w(argc, (wchar_t**)argv, optstring, longopts, longind,
				0, d, print_errors, L"-W ");
		}
		if (temp[1] == L':')
		{
			if (temp[2] == L':')
			{
				if (*d->__nextchar != L'\0')
				{
					d->optarg = d->__nextchar;
					d->optind++;
				}
				else
					d->optarg = NULL;
				d->__nextchar = NULL;
			}
			else
			{
				if (*d->__nextchar != L'\0')
				{
					d->optarg = d->__nextchar;
					d->optind++;
				}
				else if (d->optind == argc)
				{
					if (print_errors)
						fwprintf(stderr, L"%s: option requires an argument -- '%c'\n", argv[0], c);
					d->optopt = c;
					if (optstring[0] == L':')
						c = L':';
					else
						c = L'?';
				}
				else
					d->optarg = argv[d->optind++];
				d->__nextchar = NULL;
			}
		}
		return c;
	}
}

int _getopt_internal_w(int argc, wchar_t* const* argv, const wchar_t* optstring, const struct option_w* longopts, int* longind, int long_only, int posixly_correct)
{
	int result;
	getopt_data_w.optind = optind;
	getopt_data_w.opterr = opterr;
	result = _getopt_internal_r_w(argc, argv, optstring, longopts, longind, long_only, &getopt_data_w, posixly_correct);
	optind = getopt_data_w.optind;
	optarg_w = getopt_data_w.optarg;
	optopt = getopt_data_w.optopt;
	return result;
}

int getopt_w(int argc, wchar_t* const* argv, const wchar_t* optstring) _GETOPT_THROW
{
	return _getopt_internal_w(argc, argv, optstring, (const struct option_w*)0, (int*)0, 0, 0);
}

int getopt_long_w(int argc, wchar_t* const* argv, const wchar_t* options, const struct option_w* long_options, int* opt_index) _GETOPT_THROW
{
	return _getopt_internal_w(argc, argv, options, long_options, opt_index, 0, 0);
}

int getopt_long_only_w(int argc, wchar_t* const* argv, const wchar_t* options, const struct option_w* long_options, int* opt_index) _GETOPT_THROW
{
	return _getopt_internal_w(argc, argv, options, long_options, opt_index, 1, 0);
}

int _getopt_long_r_w(int argc, wchar_t* const* argv, const wchar_t* options, const struct option_w* long_options, int* opt_index, struct _getopt_data_w* d)
{
	return _getopt_internal_r_w(argc, argv, options, long_options, opt_index, 0, d, 0);
}

int _getopt_long_only_r_w(int argc, wchar_t* const* argv, const wchar_t* options, const struct option_w* long_options, int* opt_index, struct _getopt_data_w* d)
{
	return _getopt_internal_r_w(argc, argv, options, long_options, opt_index, 1, d, 0);
}