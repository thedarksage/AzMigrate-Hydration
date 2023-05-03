#ifndef _INM_CMD_PARSER_H
#define _INM_CMD_PARSER_H 1

extern char *inm_optarg;
extern int inm_optind;

struct inm_option
{
	const char	*opt_name;
	int		arg_needed;
	int		*reserved_flag;
	int		opt_val;
};

#define INM_NO_ARGUMENT		0
#define INM_REQUIRED_ARGUMENT	1

int inm_getopt_long(int argc, char *const *argv, char *short_options,
		struct inm_option *long_options, int *long_option_index);
#endif
