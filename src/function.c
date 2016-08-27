#include <stdlib.h>

#include "function.h"
#include "datastructs.h"

function* init_function (function* to_init, unsigned int start_addr, unsigned int stop_addr)
{
	jump_block* root;
	jump_block* current;
	jump_block* temp;

	to_init->start_addr = start_addr;
	to_init->stop_addr = stop_addr;
	next_flags = 0;

	root = init_jump_block (malloc (sizeof (jump_block)), start_addr, stop_addr);
	current = root;

	//Find all jump blocks
	while (num_push_ebp != 2)
	{
		temp = init_jump_block (malloc (sizeof (jump_block)), current->end, stop_addr);
		link (current, temp);
		current = current->next;
	}
	num_push_ebp = 0;

	//Get jump addresses
	to_init->num_jump_addrs = 0;
	to_init->jump_addrs_buf_size = 8 * sizeof (unsigned int);
	to_init->jump_addrs = malloc (to_init->jump_addrs_buf_size);
	to_init->orig_addrs = malloc (to_init->jump_addrs_buf_size);
	list_loop (resolve_jumps, root, root, to_init);

	//Memory management for "}" placement algorithms
	to_init->num_dups = 0;
	to_init->dup_targets_buf_size = 8 * sizeof (unsigned int);
	to_init->dup_targets = malloc (to_init->dup_targets_buf_size);
	to_init->else_starts = malloc (to_init->dup_targets_buf_size);
	to_init->pivots = malloc (to_init->dup_targets_buf_size);

	//Split jump blocks 
	struct search_params params;

	int i;
	for (i = 0; i < to_init->num_jump_addrs; i ++)
	{
		current = NULL;
		params.ret = (void**)&current;
		params.key = to_init->jump_addrs [i];
		list_loop (search_start_addrs, root, root, params);
	
		if (!current)
		{
			printf ("Error: invalid jump instruction at %p\n", (void *)(long)(to_init->orig_addrs [i]));
			continue;
		}
	
		split_jump_blocks (current, params.key, stop_addr);
	}

	to_init->jump_block_list = root;

	//Free some memory for the time being
	list_loop (cleanup_instruction_list, root, root, 1);
	
	return to_init;
}

void resolve_jumps (jump_block* to_resolve, function* benefactor)
{
	if (to_resolve->instructions [to_resolve->num_instructions-1].mnemonic [0] == 'j')
	{
		benefactor->num_jump_addrs ++;

		if (benefactor->num_jump_addrs * sizeof (unsigned int) > benefactor->jump_addrs_buf_size)
		{
			benefactor->jump_addrs_buf_size *= 2;
			benefactor->jump_addrs = realloc (benefactor->jump_addrs, benefactor->jump_addrs_buf_size);
			benefactor->orig_addrs = realloc (benefactor->orig_addrs, benefactor->jump_addrs_buf_size);
			
		}

		benefactor->jump_addrs [benefactor->num_jump_addrs-1] = relative_insn (&(to_resolve->instructions [to_resolve->num_instructions-1]), to_resolve->end);
		benefactor->orig_addrs [benefactor->num_jump_addrs-1] = to_resolve->instructions [to_resolve->num_instructions-1].address + to_resolve->start;
	}
}

//Cleanup dynamically allocated memory of a function
void cleanup_function (function* to_cleanup, char scrub_insn)
{
	free (to_cleanup->jump_addrs);
	free (to_cleanup->orig_addrs);
	jump_block_list_cleanup (to_cleanup->jump_block_list, scrub_insn);
}

//Properly free memory used in a function list
void function_list_cleanup (function* to_cleanup, char scrub_insn)
{
	list_cleanup (to_cleanup, cleanup_function, scrub_insn);
}

//Search function start addresses to look for repetition so we don't add the same function multiple times
void search_func_start_addrs (function* to_test, struct search_params arg)
{
	if (to_test->start_addr == arg.key)
	{
		*(char*)(arg.ret) = 1;
	}
}

//Helper function for resolve calls (callback for list_loop)
void resolve_calls_help (jump_block* benefactor, function* parent)
{
	struct search_params arg;
	char ret = 0;
	arg.ret = (void**)&ret;
	function* to_link;

	int i;
	for (i = 0; i < benefactor->num_calls; i ++)
	{
		arg.key = benefactor->calls [i];
		list_loop (search_func_start_addrs, parent, parent, arg);
		if (!ret && addr_to_index (arg.key) < file_size && !find_reloc_sym (*(int*)&(file_buf [addr_to_index (arg.key)+2])))
		{
			if (benefactor->calls [i] < text_addr) //Likely a reference to plt, data isn't in this file so don't bother
			{
				continue;
			}
			if (addr_to_index (benefactor->calls [i]) >= file_size) //Critical error: should not call outside of address space
			{
				continue;
			}
			to_link = init_function (malloc (sizeof (function)), benefactor->calls [i], parent->stop_addr);
			link (parent, to_link);
		}
	}
}

//Find every function call in every function and add that function to the list
void resolve_calls (function* benefactor)
{
	function* start = benefactor;

	//list_loop is already a macro, can't pass a macro to a macro like you could a function
	do
	{
		list_loop (resolve_calls_help, benefactor->jump_block_list, benefactor->jump_block_list, benefactor);
		benefactor = benefactor->next;
	} while (benefactor != start && benefactor);
}

void split_jump_blocks (jump_block* to_split, unsigned int addr, unsigned int stop_addr)
{
	if (to_split->start == addr)
	{
		return;
	}
	jump_block* new_block;
	unsigned int flags = to_split->flags;

	int i = 0;
	while (to_split->instructions [i].address + to_split->start != addr && i < to_split->num_instructions)
		i ++;

	if (i >= to_split->num_instructions)
	{
		printf ("Error: jump into instruction at %p\n", (void *)(long)addr);
		return;
	}

	new_block = init_jump_block (malloc (sizeof (jump_block)), to_split->instructions [i].address + to_split->start, stop_addr);

	int j;
	for (j = i; j < to_split->num_instructions; j ++)
	{
		free (to_split->instructions [j].detail);
	}
	
	to_split->num_instructions = i;
	to_split->end = new_block->start;

	new_block->flags = flags & (IS_LOOP | IS_CONTINUE | IS_BREAK | IS_GOTO);
	to_split->flags = flags & (IS_ELSE | IS_IF | IS_AFTER_ELSE | IS_IF_TARGET | IS_AFTER_LOOP);

	link (to_split, new_block);
}
