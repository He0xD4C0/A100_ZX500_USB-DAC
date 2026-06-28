/*
 * Copyright 2019 Sony Home Entertainment & Sound Products Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <watchdog.h>
#include <cli.h>
#include <u-boot/md5.h>
#ifdef CONFIG_DMP
#include <nvp.h>
#endif

/* Must register a function that get secret key adapted to the target. */
static int (*pwd_get_secret_key)(char *, size_t) =
#if defined(CONFIG_DMP) && defined(CONFIG_ICX_NVP_EMMC)
	nvp_emmc_get_pwd;
#else
	NULL;
#endif

#define PWD_NO_OPERATION_RESET_TIME 60
#define PWD_LEN 32

static const char pwd_prompt[] = "> ";

static void chr2asc(char *dst, char src) 
{
	char c;
	int i;

	for (i = 1; i >= 0; i--) {
		c = ((unsigned int)src >> (4 * i)) & 0xF; 
		if ((unsigned int)c < 10)
			c = c + '0'; 
		else
			c = c - 10 + 'a'; 
		dst[(1-i)] = c; 
	}
}

static int pwd_check(const char *pwd, const char *key, int len)
{
	unsigned char sig[16];
	char buf[PWD_LEN];
	int i;
	int rc;

	if (len > PWD_LEN)
		return 0;

	/* calcurate MD5 digest */
	md5((unsigned char *)pwd, len, sig);

	for (i = 0; i < 16; i++)
		chr2asc(&buf[i * 2], sig[i]);

	/* compare pwd */
	if (strncmp(buf, key, PWD_LEN) == 0)
		rc = 1;
	else
		rc = 0;

	return rc;
}

/*
 * This function is based on cli_readline_into_buffer() at common/cli_readline.c
 */
static int pwd_readline(char *linebuf)
{
	uint64_t endtime;
	char *p = linebuf;
	char c;
	int n = 0;

	endtime = endtick(PWD_NO_OPERATION_RESET_TIME);

	puts(pwd_prompt);

	for (;;) {
		while (!tstc()) { /* while no incoming data */
			if (get_ticks() > endtime)
				return -ETIMEDOUT;
			WATCHDOG_RESET();
		}
		WATCHDOG_RESET(); /* Trigger watchdog, if needed */

		c = getc();
		/*
		 * Special character handling
		 */
		switch (c) {
		case '\r':                      /* Enter           */
		case '\n':
			*p = '\0';
			puts("\r\n");
			return p - linebuf;

		case '\0':                      /* nul             */
			continue;

		case 0x03:                      /* ^C - break      */
			return -1;              /* discard input   */

		default:
			/*
			 * Must be a normal character then
			 */
			if (n < (CONFIG_SYS_CBSIZE - 2)) {
				/* Echo input as '*' using puts() */
				puts("*\0");

				*p++ = c;
				++n;
			} else {                /* Buffer full */
				putc('\a');
			}
		}
	}

	return 0;
}

void pwd_loop(void)
{
	char pwd[CONFIG_SYS_CBSIZE + 1];
	char key[PWD_LEN];
	int len;
	char *s;
	int passed;

	if (pwd_get_secret_key == NULL)
		return;

	if ((s = env_get("pwd")) == NULL)
		return;

	passed = ((int)simple_strtol(s, NULL, 10) == 0) ? 1 : 0;

	pwd_get_secret_key(key, PWD_LEN);

	while (passed == 0) {
		len = pwd_readline(pwd);
		if (len == -ETIMEDOUT) {
			do_reset(NULL, 0, 0, NULL); /* reset and boot android */
		} else if (len == -1) {
			puts("\n");
		} else if (len > 0) {
			if (pwd_check(pwd, key, len) == 0)
				udelay(3000000);
			else
				passed = 1;
		}
	}
}
