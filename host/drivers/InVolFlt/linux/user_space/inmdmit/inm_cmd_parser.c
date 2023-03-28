#include <stdio.h>
#include <string.h>

#include "inm_cmd_parser.h"

/* This version of getopt only handles long options only. The user
 * provided options has to be exact match with the existing ones.
 * Partial options won't be considered.
 */

/* When inm_getopt_long finds an option, it returns the argument value,
 * if any, using the following pointer.
 */

char *inm_optarg = NULL;


/* The next element to look at from arguments passed. When inm_getopt_long
 * finds an option, it sets the inm_optind properly and returns to the caller.
 * It is caller's responsibility to set the optnd correctly, if at all
 * accessing more arguments than expected.
 */

int inm_optind = 1;


/* Searches through the array of long options for an option pointed at
 * inm_optind in ARGV.
 *
 * If the option doesn't exist in the array of long options, returns
 * NULL. Otherwise, returns the corresponding "struct inm_option" object.
 *
 * If the user provided option has any "=" sign after the option, the
 * inm_optarg would be set to the next character to "=" and inm_optind
 * would be incremented. Otherwise, inm_optind would be adjusted depending
 * upon the option is execting an argument or not.
 */
struct inm_option *
inm_search_long_option(int argc, char *const *argv,
		struct inm_option *long_options, int *long_option_index)
{
	int			len;
	struct inm_option	*p, *found = NULL;
	int			option_index = 0;
	char			*option_name;

	/*
	 * The token is pointed by the inm_optind in the arguments. As
	 * token starts after "--", so pointing to the first character
	 * of the token.
	 */
	option_name = &argv[inm_optind][2];

	/*
	 * This loop is going to find the token and its length. As the
	 * option can have the "=" sign, the token ends before "=". Or
	 * if the option doesn't have "=", the token would be same as
	 * option ("--" are already excluded).
	 *
	 * After this loop, "len" contains the length of the token and
	 * "option_name[len]" points to either '=' or NULL depending
	 * upon the option has '=' or not respectively.
	 */
	for (len = 0; option_name[len] && option_name[len] != '='; len++)
		/* DO NOTHING */;

	/*
	 * Looping over the "struct inm_option" array to find the exact
	 * match.
	 */
	for (p = long_options; p->opt_name; p++, option_index++) {

		/* Token length & option name length are not matching */
		if (strlen(p->opt_name) != len)
			continue;

		/* Token & option names are not matching */
		if (strncmp(p->opt_name, option_name, len))
			continue;

		/* Found the option at index pointed by "option_index" */
		*long_option_index = option_index;
		found = p;

		/*
		 * Incremented "inm_optind" to process the next scan the
		 * next argument.
		 */
		inm_optind++;

		if (option_name[len]) {
			/*
			 * The option includes '=' and it is expecting an
			 * argument, "inm_optarg" will be pointed to the
			 * character next to '='.
			 */
			if (p->arg_needed)
				inm_optarg = &option_name[len + 1];
			else {
				printf ("%s: option `%s' doesn't "
					"require an argument\n",
					argv[0], argv[inm_optind - 1]);
				inm_optind--;
				found = NULL;
			}
		} else if (p->arg_needed == INM_REQUIRED_ARGUMENT) {
			/*
			 * The option doesn't have any '=' in it. But it
			 * expects an argument. So "inm_optarg" will be
			 * pointed to the next element in the argument list
			 * and the caller is going to process it. Now the
			 * element next to this is the element to be scanned
			 * and so incremented "inm_optind".
			 */
			if (inm_optind < argc) {
				inm_optarg = argv[inm_optind];
				inm_optind++;
			} else {
				printf ("%s: option `%s' requires an argument\n",
					argv[0], argv[inm_optind - 1]);
				inm_optind--;
				found = NULL;
			}
		}
		break;
	}

	return found;
}

/**
 * inm_getopt_long - Scans for the long options only through the arguments
 *
 * If the inm_getopt_long finds an option, it updates inm_optind so that
 * the next call to inm_getopt_long can resume the scanning.
 *
 * If there are no more options, inm_getopt_long returns -1.
 * 
 * long_options - This is array of "struct inm_option" elements and the last
 *		  contains the "opt_name" as NULL.
 *
 * long_option_index - If inm_getopt_long founds any long option, it sets
 *		       thi field to "opt_val" field of "struct inm_option".
 */
int
inm_getopt_long(int argc, char *const *argv, char *short_options,
		struct inm_option *long_options, int *long_option_index)
{
	int			option_index;
	int			option_code = -1;
	struct inm_option	*p;

	if (!long_options) {
		printf("No long options are provided. It can scan only long"
			" options. Exiting...\n");
		goto out;
	}

	inm_optarg = NULL;

	if (inm_optind == 0)
		inm_optind = 1;

	while (inm_optind < argc) {
		if (strncmp(argv[inm_optind], "--", 2)) {
			inm_optind++;
			continue;
		}

		p = inm_search_long_option(argc, argv, long_options,
							&option_index);
		if (!p) {
			printf("%s: unrecognized option `%s'\n", argv[0],
								argv[inm_optind]);
			goto out;
		}

		if (long_option_index)
			*long_option_index = option_index;

		option_code = p->opt_val;
		break;
	}

out:
	return option_code;
}
