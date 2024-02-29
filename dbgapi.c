#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "list.h"
#include "dbgapi.h"

extern unsigned int z100_memory_read_(unsigned int addr);
extern void cpu_op(void);

typedef enum
{
	NONE,
	EXECUTE,
	READ,
	WRITE,
	VECTOR,
	PORTIN,
	PORTOUT
	// REGEQU,
	// MEMEQU
} bpop_t;

typedef struct
{
	bpop_t operation;
	unsigned int value; // address, reg val, mem val, vector, port
} bpcond_t;

import_list(bpcond_t, bpcond_list);

typedef struct
{
	bpcond_list conditions;
	int active;
	int hits;
} breakpoint_t;

import_list(breakpoint_t, bp_list);

bp_list new_bplist(void)
{
	return new(bp_list);
}

breakpoint_t new_bp(void)
{
	return (breakpoint_t){ new(bpcond_list), 1, 0 };
}

void bp_new_cond(breakpoint_t *bp, bpop_t op, unsigned int value)
{
	list_add(bp->conditions, ((bpcond_t){ op, value }));
}

bp_list bps;

void bp_exec_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == EXECUTE && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					breakpoint_active = 1;
					printf("EXEC HIT\n");
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void bp_read_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == READ && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					breakpoint_active = 1;
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void bp_write_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == WRITE && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					breakpoint_active = 1;
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void bp_in_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == PORTIN && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					breakpoint_active = 1;
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void bp_out_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == PORTOUT && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					breakpoint_active = 1;
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void dbg_init(void)
{
	bps = new(bp_list);
}

#define USE_COLOR 1

static void blue(void)
{
#if USE_COLOR
	printf("\x1B[1;34m");
	fprintf(stderr, "\x1B[1;34m");
#endif
}

static void green(void)
{
#if USE_COLOR
	printf("\x1B[1;32m");
	fprintf(stderr, "\x1B[1;32m");
#endif
}

static void magenta(void)
{
#if USE_COLOR
	printf("\x1B[1;35m");
	fprintf(stderr, "\x1B[1;35m");
#endif
}

static void red(void)
{
#if USE_COLOR
	printf("\x1B[1;31m");
	fprintf(stderr, "\x1B[1;31m");
#endif
}

static void bold(void)
{
#if USE_COLOR
	printf("\x1B[1m");
	fprintf(stderr, "\x1B[1m");
#endif
}

static void reset(void)	
{
#if USE_COLOR
	printf("\x1B[0m");
	fprintf(stderr, "\x1B[0m");
#endif
}

void out_error(const char *format, ...)
{
	va_list va;
	bold();
	red();
	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
	reset();
	fflush(stderr);
}

void hex_dump(const void *ptr, size_t size, unsigned int base)
{
	if (ptr && size)
	{
		// Print header.
		blue();
		printf("\nOffset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  Decoded text\n");
		char text[16 + 1] = {0};
		const char *buffer = (const char *)ptr;
		size_t i;
		for (i = 0; i < size; i++)
		{
			if (!(i % 16))
			{
				// Beginning of line.
				if (i)
				{
					// Not first, print prev line's txt.
					magenta();
					printf("  %s\n", text);
					memset(text, 0, sizeof(text));
				}
				// Print address within buffer.
				blue();
				printf("%05X ", base + (unsigned int)i);
				green();
			}
			// Print each byte's hex value.
			printf(" %02X", (unsigned char)buffer[i]);
			// Cache the text representation to be printed later.
			text[i % 16] = isprint(buffer[i]) ? buffer[i] : '.';
		}
		// Pad to the position of text dump of last line.
		while ((i % 16))
		{
			// Space and 2 hex digits.
			printf("   ");
			i++;
		}
		// Print text of last line.
		magenta();	
		printf("  %s\n", text);	
		reset();
		fflush(stdout);
	}
}


int hexstr_to_ull(const char *str, unsigned long long *result)
{
	if (str && result)
	{
		char *endptr;
		errno = 0;
		*result = strtoull(str, &endptr, 16);
		if (!endptr[0] && !errno)
		{
			return 1;
		}
	}
	return 0;
}

int get_seg_addr(const char *str, unsigned int *addr)
{
	char addrstr1[5];
	char addrstr2[5];
	if (!str || !addr || str[4] != ':')
	{
		return 0;
	}
	memcpy(addrstr1, &str[0], 4);
	addrstr1[4] = '\0';
	memcpy(addrstr2, &str[5], 4);
	addrstr2[4] = '\0';
	unsigned long long tmp;
	if (!hexstr_to_ull(addrstr1, &tmp))
	{
		return 0;
	}
	*addr = (unsigned int)tmp;
	*addr = *addr << 4;
	if (!hexstr_to_ull(addrstr2, &tmp))
	{
		return 0;
	}
	*addr += (unsigned int)tmp;
	*addr &= 0xFFFFF;
	return 1;
}


void dbg_reg(P8088 *p8088)
{
	printf("AX=%04X BX=%04X CX=%04X DX=%04X SP=%04X BP=%04X SI=%04X DI=%04X \n",
		(p8088->AH << 8) | p8088->AL,
		(p8088->BH << 8) | p8088->BL,
		(p8088->CH << 8) | p8088->CL,
		(p8088->DH << 8) | p8088->DL,
		p8088->SP,
		p8088->BP,
		p8088->SI,
		p8088->DI
	);
	printf("SS=%04X DS=%04X ES=%04X V%d D%d I%d T%d S%d Z%d A%d P%d C%d \n",
		p8088->SS,
		p8088->DS,
		p8088->ES,
		p8088->o,
		p8088->d,
		p8088->i,
		p8088->t,
		p8088->s,
		p8088->z,
		p8088->ac,
		p8088->p,
		p8088->c
	
	);
	printf("CS:IP=%04X:%04X \n", p8088->CS, p8088->IP);
	printf("Last instruction: %s \n", p8088->name_opcode);
}

int dbg_cmd(P8088 *p8088, const char *command)
{
	char buffer[128];
	unsigned int value = 0;
	switch (command[0])
	{
		case 'r':
		case 'R':
			dbg_reg(p8088);
			break;
		case 'd':
		case 'D':
			if (!get_seg_addr(&command[2], &value))
			{
				return 0;
			}
			for (int i = 0; i < 128; i++)
			{
				buffer[i] = z100_memory_read_(value + i);
			}
			hex_dump(buffer, 128, value);
			break;
		case 't':
		case 'T':
			cpu_op();
			dbg_reg(p8088);
			break;
		case 'b':
		case 'B':
			if (command[1] == 'l' || command[1] == 'L')
			{
				for (size_t i = 0; i < list_size(bps); i++)
				{
					printf("BP %zu:", i);
					for (size_t j = 0; j < list_size(list_get(bps, i).conditions); j++)
					{
						bpcond_t cond = list_get(list_get(bps, i).conditions, j);
						const char *op;
						switch (cond.operation)
						{
							case EXECUTE:
								op = "EXEC";
								break;
							case READ:
								op = "READ";
								break;
							case WRITE:
								op = "WRIT";
								break;
							default:
								op = "UNKN";
								break;
						}
						printf(" %s(%X)", op, cond.value);
					}
					printf(" [%s] [%d hit%s]\n", list_get(bps, i).active ? "ACTIVE" : "INACTIVE", list_get(bps, i).hits, (list_get(bps, i).hits == 1) ? "" : "s");
				}
				return 1;
			}
			if (!get_seg_addr(&command[3], &value))
			{
				return 0;
			}
			if (command[1] == 'p' || command[1] == 'P')
			{
				breakpoint_t bp = new_bp();
				bp_new_cond(&bp, EXECUTE, value);
				list_add(bps, bp);
			}
			else if (command[1] == 'r' || command[1] == 'R')
			{
				breakpoint_t bp = new_bp();
				bp_new_cond(&bp, READ, value);
				list_add(bps, bp);
			}
			else if (command[1] == 'w' || command[1] == 'W')
			{
				breakpoint_t bp = new_bp();
				bp_new_cond(&bp, WRITE, value);
				list_add(bps, bp);
			}
			break;
		case 'q':
		case 'Q':
			exit(-1);
		case '?':
		case 'h':
		case 'H':
			printf("r - register dump\n");
			printf("d xxxx:xxxx - memory dump\n");
			printf("b[p|r|w] xxxx:xxxx - breakpoint\n");
			printf("bl - list breakpoints\n");
			printf("t - trace\n");
			printf("q - quit\n");
			printf("h - help\n");
			break;
		default:
			return 0;
	}
	return 1;
}
