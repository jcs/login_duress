/* $OpenBSD$ */

/*-
 * Copyright (c) 2018 joshua stein <jcs@jcs.org>
 * Copyright (c) 2001 Hans Insulander <hin@openbsd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "common.h"

int
xasprintf(char **ret, const char *format, ...)
{
	int r;
	va_list ap;

	va_start(ap, format);
	r = vasprintf(ret, format, ap);
	va_end(ap);
	if (r == -1) {
		syslog(LOG_ERR, "vasprintf");
		_exit(1);
	}

	return (r);
}
int
pwd_login(char *username, char *password, char *wheel, int lastchance,
    char *class, struct passwd *pwd)
{
	size_t plen, linesize = 0, cmdsize;
	char *goodhash = NULL;
	char *dline = NULL, *odline = NULL, *tok = NULL, *cmd = NULL;
	char *environ[6];
	FILE *duress;
	pid_t duress_pid = -1;
	int passok = 0;
	int idx;

	if (wheel != NULL && strcmp(wheel, "yes") != 0) {
		fprintf(back, BI_VALUE " errormsg %s\n",
		    auth_mkvalue("you are not in group wheel"));
		fprintf(back, BI_REJECT "\n");
		return (AUTH_FAILED);
	}
	if (password == NULL)
		return (AUTH_FAILED);

	if (pwd)
		goodhash = pwd->pw_passwd;

	setpriority(PRIO_PROCESS, 0, -4);

	duress = fopen(DURESS_FILE, "re");
	if (duress != NULL) {
		while (getline(&odline, &linesize, duress) != -1) {
			dline = odline;

			tok = strsep(&dline, ":");
			if (strcmp(tok, username) != 0)
				goto next_user;

			tok = strsep(&dline, ":");
			if (crypt_checkpass(password, tok) == 0) {
				tok = strsep(&dline, ":");

				cmdsize = strlen(tok) + 1;
				cmd = malloc(cmdsize);
				if (cmd == NULL) {
					syslog(LOG_ERR, "malloc: %m");
					return (AUTH_FAILED);
				}
				strlcpy(cmd, tok, cmdsize);
			}
next_user:
			explicit_bzero(odline, linesize);
		}
		free(odline);
		fclose(duress);
	}

	if (pledge("stdio rpath id proc exec", NULL) == -1) {
		syslog(LOG_ERR, "pledge: %m");
		return (AUTH_FAILED);
	}

	/* always run crypt for both passwords to keep timing the same */
	if (crypt_checkpass(password, goodhash) == 0)
		passok = 1;
	plen = strlen(password);
	explicit_bzero(password, plen);

	if (cmd != NULL) {
		/* duress received, run command */
		passok = 1;

		idx = 0;
		xasprintf(&environ[idx++], "PATH=%s", _PATH_DEFPATH);
		xasprintf(&environ[idx++], "HOME=%s", pwd->pw_dir);
		xasprintf(&environ[idx++], "SHELL=/bin/sh");
		xasprintf(&environ[idx++], "LOGNAME=%s", pwd->pw_name);
		xasprintf(&environ[idx++], "USER=%s", pwd->pw_name);
		environ[idx++] = (char *)NULL;

		switch (duress_pid = fork()) {
		case -1:
			passok = 0;
			break;
		case 0:
			signal(SIGPIPE, SIG_IGN);
			if (setsid() == -1 ||
			    setresgid(pwd->pw_gid, pwd->pw_gid, pwd->pw_gid) ||
			    setresuid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid)) {
				syslog(LOG_ERR, "failed setsid/setresuid for %s",
				    pwd->pw_name);
				exit(1);
			}

			if (chdir(pwd->pw_dir) != 0) {
				syslog(LOG_ERR, "failed chdir to %s",
				    pwd->pw_dir);
				exit(1);
			}

			execle("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL,
			    environ);

			syslog(LOG_ERR, "failed executing: %s", cmd);
			_exit(1);
		}

		free(cmd);
	}

	if (!passok)
		return (AUTH_FAILED);

	if (login_check_expire(back, pwd, class, lastchance) == 0)
		fprintf(back, BI_AUTH "\n");
	else
		return (AUTH_FAILED);

	return (AUTH_OK);
}
