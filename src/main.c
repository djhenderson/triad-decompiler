#include <stdlib.h>
#include <capstone/capstone.h>

#include "elf_parser.h"
#include "lang_gen.h"
#include "var.h"

int main (int argc, char** argv)
{
	int i;
	int j;
	char* file_name = NULL;
	char* beginning_address_string = NULL;
	char* cutoff_address_string = NULL;
	unsigned int stop_addr;
	char follow_calls = 1;
	language_flag = 'f';
	constant_format [0] = '%';
	constant_format [1] = 'd';
	constant_format [2] = '\0';

	//Parse the command line
	for (i = 1; i < argc; i ++)
	{
		if (argv [i][0] == '-')
		{
			j = 1;
			while (argv [i][j] != '\0')
			{
				switch (argv [i][j])
				{
					case 'f':
						break;
					case 'p':
						language_flag = 'p';
						break;
					case 'd':
						language_flag = 'd';
						break;
					case 's':
						follow_calls = 0;
						break;
					case 'h':
						constant_format [1] = 'p';
						break;
					default:
						printf ("Unrecognized flag \"%c\"\n", argv [i][j]);
						exit (-1);
				}
				j ++;
			}
		}
		else if (file_name == NULL)
		{
			file_name = argv [i];
		}
		else if (beginning_address_string == NULL)
		{
			beginning_address_string = argv [i];
		}
		else if (cutoff_address_string == NULL)
		{
			cutoff_address_string = argv [i];
		}
		else
		{
			printf ("Unrecognized option \"%s\"\n", argv [i]);
			exit (-1);
		}
	}


	unsigned int beginning_address;
	function* func = 0;
	if (beginning_address_string == NULL)
	{
		parse_elf (file_name);
	}

	if (beginning_address_string)
	{
		init_elf_parser (file_name);
		if (architecture == ELFCLASS32)
		{
			if (cs_open (CS_ARCH_X86, CS_MODE_32, &handle) != CS_ERR_OK)
			{
				printf ("CRITICAL ERROR: Could not initialize Capstone\n");
				exit (-1);
			}
		}
		else
		{
			if (cs_open (CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
			{
				printf ("CRITICAL ERROR: Could not initialize Capstone\n");
				exit (-1);
			}
		}
		cs_option (handle, CS_OPT_DETAIL, CS_OPT_ON);
		beginning_address = strtoul (beginning_address_string, NULL, 16);

		if (cutoff_address_string)
		{
			stop_addr = strtoul (cutoff_address_string, NULL, 16);
		}
		else
		{
			stop_addr = end_of_text;
		}

		if (beginning_address)
		{
			func = init_function (malloc (sizeof (function)), beginning_address, stop_addr);
		}
		else
		{
			printf ("Error: invalid start address\n");
		}
	}
	else if (main_addr)
	{
		if (architecture == ELFCLASS32)
		{
			if (cs_open (CS_ARCH_X86, CS_MODE_32, &handle) != CS_ERR_OK)
			{
				printf ("CRITICAL ERROR: Could not initialize Capstone\n");
				exit (-1);
			}
		}
		else
		{
			if (cs_open (CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
			{
				printf ("CRITICAL ERROR: Could not initialize Capstone\n");
				exit (-1);
			}
		}

		cs_option (handle, CS_OPT_DETAIL, CS_OPT_ON);

		if (cutoff_address_string)
		{
			stop_addr = strtoul (cutoff_address_string, NULL, 16);
		}
		else
		{
			stop_addr = end_of_text;
		}

		func = init_function (malloc (sizeof (function)), main_addr, stop_addr);
	}
	else
	{
		printf ("Error: could not find main and no start address specified\n");
		exit (-1);
	}
	func->next = NULL;
	if (follow_calls)
	{
		resolve_calls (func);
	}
	translate_function_list (func);

	function_list_cleanup (func, 1); //Make sure those operands don't leak
	elf_parser_cleanup ();
	cs_close (&handle);
}
