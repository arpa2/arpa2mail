/*
 * Copyright (c) 2018, 2019 Tim Kuijsten
 *
 * Permission to use, copy, modify, and distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "util.h"

#define MAXUID (1<<16)
#define MAXGID (1<<16)

extern char background, verbose;

/*
 * Check if the leaf component of a path is owned by the superuser and not
 * writable by the group or others.
 *
 * Return 1 if this is the case, 0 if not.
 */
int
leafmodsuperuseronly(const char *path)
{
	struct stat st;

	if (stat(path, &st) == -1)
		return 0;

	/* owned by the superuser */
	if (st.st_uid != 0)
		return 0;

	if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0)
		return 0;

	return 1;
}

/*
 * Drop real, effective and saved set-user and group ID to a non-superuser and
 * remove all supplementary groups.
 *
 * Return 0 on success, -1 on failure with errno set.
 */
int
dropuser(uid_t uid, gid_t gid)
{
	if (uid == 0 || gid == 0) {
		errno = EINVAL;
		return -1;
	}

	if (geteuid() != 0) {
		errno = EPERM;
		return -1;
	}

	if (setgroups(1, &gid) == -1)
		return -1;

	if (setgid(gid) == -1)
		return -1;

	if (setuid(uid) == -1)
		return -1;

	return 0;
}

/*
 * Resolve a user and primary group id. Supports the name as a string, a decimal
 * number, hexadecimal or octal number (precedence of names over ids is based on
 * chown(1) and POSIX).
 *
 * Return 0 on success, -1 on failure with errno set.
 */
int
resolveuser(uid_t *uid, gid_t *gid, const char *userstr)
{
	struct passwd *pwd;
	intmax_t tmpid;
	char *errchr;

	if (uid == NULL || gid == NULL || userstr == NULL) {
		errno = EINVAL;
		return -1;
	}

	pwd = getpwnam(userstr);

	if (pwd == NULL) {
		/* Maybe it's a uid. */

		tmpid = strtoimax(userstr, &errchr, 0);
		if (tmpid < 0 || tmpid > MAXUID || *errchr != '\0')
			return -1;

		pwd = getpwuid(tmpid);

		if (pwd == NULL) {
			/*
			 * uid is not set in passwd, but thats
			 * ok. Use the same id for the group.
			 */

			*uid = tmpid;
			*gid = tmpid;
		} else {
			/* Use the configured primary group. */
			*uid = pwd->pw_uid;
			*gid = pwd->pw_gid;
		}
	} else {
		*uid = pwd->pw_uid;
		*gid = pwd->pw_gid;
	}

	return 0;
}

/*
 * Resolve a group id. Supports the name as a string, a decimal number,
 * hexadecimal or octal number (precedence of names over ids is based on
 * chown(1) and POSIX).
 *
 * Return 0 on success, -1 on failure with errno set.
 */
int
resolvegroup(gid_t *gid, const char *groupstr)
{
	struct group *grp;
	intmax_t tmpid;
	char *errchr;

	if (gid == NULL || groupstr == NULL) {
		errno = EINVAL;
		return -1;
	}

	grp = getgrnam(groupstr);

	if (grp == NULL) {
		/* Maybe it's a gid. */

		tmpid = strtoimax(groupstr, &errchr, 0);
		if (tmpid < 0 || tmpid > MAXGID || *errchr != '\0')
			return -1;

		grp = getgrgid(tmpid);

		if (grp == NULL) {
			/*
			 * gid is not set in passwd, but thats
			 * ok. Use the same id for the group.
			 */

			*gid = tmpid;
		} else {
			*gid = grp->gr_gid;
		}
	} else {
		*gid = grp->gr_gid;
	}

	return 0;
}


/*
 * Input must be a nul terminated string containing a facility from
 * syslog.conf(5).
 *
 * Return 0 if "logfacility" is set, or -1 if "facility" is not recognized.
 */
static int
facilitystrtoint(int *logfacility, const char *facility)
{
	if (strcmp(facility, "auth") == 0) {
		*logfacility = LOG_AUTH;
	} else if (strcmp(facility, "authpriv") == 0) {
		*logfacility = LOG_AUTHPRIV;
	} else if (strcmp(facility, "cron") == 0) {
		*logfacility = LOG_CRON;
	} else if (strcmp(facility, "daemon") == 0) {
		*logfacility = LOG_DAEMON;
	} else if (strcmp(facility, "ftp") == 0) {
		*logfacility = LOG_FTP;
	} else if (strcmp(facility, "kern") == 0) {
		*logfacility = LOG_KERN;
	} else if (strcmp(facility, "lpr") == 0) {
		*logfacility = LOG_LPR;
	} else if (strcmp(facility, "mail") == 0) {
		*logfacility = LOG_MAIL;
	} else if (strcmp(facility, "news") == 0) {
		*logfacility = LOG_NEWS;
	} else if (strcmp(facility, "syslog") == 0) {
		*logfacility = LOG_SYSLOG;
	} else if (strcmp(facility, "user") == 0) {
		*logfacility = LOG_USER;
	} else if (strcmp(facility, "uucp") == 0) {
		*logfacility = LOG_UUCP;
	} else if (strcmp(facility, "local0") == 0) {
		*logfacility = LOG_LOCAL0;
	} else if (strcmp(facility, "local1") == 0) {
		*logfacility = LOG_LOCAL1;
	} else if (strcmp(facility, "local2") == 0) {
		*logfacility = LOG_LOCAL2;
	} else if (strcmp(facility, "local3") == 0) {
		*logfacility = LOG_LOCAL3;
	} else if (strcmp(facility, "local4") == 0) {
		*logfacility = LOG_LOCAL4;
	} else if (strcmp(facility, "local5") == 0) {
		*logfacility = LOG_LOCAL5;
	} else if (strcmp(facility, "local6") == 0) {
		*logfacility = LOG_LOCAL6;
	} else if (strcmp(facility, "local7") == 0) {
		*logfacility = LOG_LOCAL7;
	} else {
		return -1;
	}

	return 0;
}

/*
 * Daemonize.
 *
 * Note: this function does *not* umask, chdir, chroot or open syslog.
 *
 * fork and exit parent to ensure we're not a process group leader
 * setsid to create a new session and disassociate from controlling terminal
 * fork again to ensure we can't aquire a controlling terminal
 * close all open descriptors
 * reopen 0, 1 and 2 to /dev/null
 *
 * Return 0 on success, -1 on error with errno set.
 */
int
daemonize(void)
{
	int i;

	/* fork and exit parent */
	if (fork() != 0)
		exit(0);

	if (setsid() == -1)
		return -1;

	/*
	 * Fork again to ensure we're not a session leader and so we're not able
	 * to ever open a controlling terminal.
	 */
	if (fork() != 0)
		exit(0);

	/* close all descriptors (only the first 64 and hope for the best) */
	for (i = 0; i < 64; i++)
		if (close(i) == -1 && errno != EBADF)
			return -1;

	/* open stdin, stdout, stderr */
	open("/dev/null", O_RDONLY);
	open("/dev/null", O_WRONLY);
	open("/dev/null", O_WRONLY);

	return 0;
}

/*
 * Init logging and if running in the background, arrange logging via syslog.
 *
 * Expects the global int "background" to be 0 or 1.
 *
 * If running in the "background", open syslog with "facility", otherwise
 * "facility" is ignored.
 *
 * Return 0 on success, -1 otherwise.
 */
int
initlog(const char *facility)
{
	int logfacility;

	logfacility = 0;
	if (facility && strlen(facility))
		if (facilitystrtoint(&logfacility, facility) == -1)
			return -1;

	if (background)
		openlog(NULL, LOG_NDELAY | LOG_PID, logfacility);

	return 0;
}

void
logexit(int code, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_ERR, msg, ap);
		syslog(LOG_ERR, "%m");
		exit(code);
	} else {
		if (msg) {
			vfprintf(stderr, msg, ap);
			fprintf(stderr, ": %s\n", strerror(errno));
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
		}
		exit(code);
	}
	va_end(ap);
}

void
logexitx(int code, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_ERR, msg, ap);
		exit(code);
	} else {
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
		exit(code);
	}
	va_end(ap);
}

/*
 * These and the other log functions expect the global integers "background" and
 * "verbose" to be set.
 *
 * "verbose" must be a number between -2 and 2:
 * -2 = err
 * -1 = lower + warn
 *  0 = lower + notice
 *  1 = lower + info
 *  2 = lower + debug
 */
void
logwarn(const char *msg, ...)
{
	va_list ap;

	if (verbose < -1)
		return;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_WARNING, msg, ap);
		syslog(LOG_WARNING, "%m");
	} else {
		if (msg) {
			vfprintf(stderr, msg, ap);
			fprintf(stderr, ": %s\n", strerror(errno));
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
		}
	}
	va_end(ap);
}

void
logwarnx(const char *msg, ...)
{
	va_list ap;

	if (verbose < -1)
		return;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_WARNING, msg, ap);
	} else {
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}

void
lognotice(const char *msg, ...)
{
	va_list ap;

	if (verbose < 0)
		return;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_NOTICE, msg, ap);
		syslog(LOG_NOTICE, "%m");
	} else {
		if (msg) {
			vfprintf(stderr, msg, ap);
			fprintf(stderr, ": %s\n", strerror(errno));
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
		}
	}
	va_end(ap);
}

void
lognoticex(const char *msg, ...)
{
	va_list ap;

	if (verbose < 0)
		return;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_NOTICE, msg, ap);
	} else {
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}

void
loginfo(const char *msg, ...)
{
	va_list ap;

	if (verbose < 1)
		return;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_INFO, msg, ap);
		syslog(LOG_INFO, "%m");
	} else {
		if (msg) {
			vfprintf(stderr, msg, ap);
			fprintf(stderr, ": %s\n", strerror(errno));
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
		}
	}
	va_end(ap);
}

void
loginfox(const char *msg, ...)
{
	va_list ap;

	if (verbose < 1)
		return;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_INFO, msg, ap);
	} else {
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}

void
logdebug(const char *msg, ...)
{
	va_list ap;

	if (verbose < 2)
		return;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_DEBUG, msg, ap);
		syslog(LOG_DEBUG, "%m");
	} else {
		if (msg) {
			vfprintf(stderr, msg, ap);
			fprintf(stderr, ": %s\n", strerror(errno));
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
		}
	}
	va_end(ap);
}

void
logdebugx(const char *msg, ...)
{
	va_list ap;

	if (verbose < 2)
		return;

	va_start(ap, msg);
	if (background) {
		vsyslog(LOG_DEBUG, msg, ap);
	} else {
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}
