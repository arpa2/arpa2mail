#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <arpa2/a2acl.h>
#include <libmilter/mfapi.h>

#include "util.h"

#define HEADER "X-ARPA2-ACL"

int background, verbose;

static const char *progname;

sfsistat
mlfi_envfrom(SMFICTX *ctx, char **argv)
{
	char list, *lp;

	if ((lp = malloc(sizeof(list))) == NULL)
		logexit(1, "malloc");

	if (smfi_setpriv(ctx, lp) == MI_FAILURE)
		logexitx(1, "%s smfi_setpriv", __func__);

	loginfox("%s list resource allocated", __func__);

	return SMFIS_CONTINUE;
}

sfsistat
mlfi_envrcpt(SMFICTX *ctx, char **argv)
{
	struct a2id remoteid, localid;
	char *mailaddr, *rcptaddr;
	char list, *lp;

	/* prevent compiler warning */
	argv = NULL;

	mailaddr = smfi_getsymval(ctx, "{mail_addr}");
	rcptaddr = smfi_getsymval(ctx, "{rcpt_addr}");;

	if (mailaddr == NULL) {
		logwarnx("sender unknown");
		return SMFIS_REJECT;
	}

	if (rcptaddr == NULL) {
		logwarnx("receiver unknown");
		return SMFIS_REJECT;
	}

	if (a2id_parsestr(&remoteid, mailaddr, 0) != 0) {
		lognoticex("illegal sender %s", mailaddr);
		return SMFIS_REJECT;
	}

	if (a2id_parsestr(&localid, rcptaddr, 0) != 0) {
		lognoticex("illegal receiver %s", rcptaddr);
		return SMFIS_REJECT;
	}

	if (a2acl_whichlist(&list, &remoteid, &localid) == -1) {
		logwarnx("a2acl_whichlist failed sender: %s, receiver %s",
		    mailaddr, rcptaddr);
		return SMFIS_REJECT;
	}

	lognoticex("%s => %s: %c", mailaddr, rcptaddr, list);

	/* update list of this message transaction */
	if ((lp = smfi_getpriv(ctx)) == NULL)
		logexitx(1, "%s: smfi_getpriv == NULL", __func__);

	*lp = list;
	if (smfi_setpriv(ctx, lp) == MI_FAILURE)
		logexitx(1, "%s smfi_setpriv", __func__);

	switch (list) {
	case 'W':
		return SMFIS_CONTINUE;
	case 'G':
		return SMFIS_CONTINUE;
	case 'B':
		return SMFIS_REJECT;
	case 'A':
		return SMFIS_DISCARD;
	default:
		logexitx(1, "unexpected ACL");
	}

	/* noop to prevent compiler warning */
	return SMFIS_REJECT;
}

sfsistat
mlfi_header(SMFICTX *ctx, char *headerf, char *headerv)
{
	if (strcasestr(headerf, HEADER) != NULL) {
		logwarnx("unexpected " HEADER " header encountered");
		return SMFIS_REJECT;
	}

	loginfox("header: %s: %s", headerf, headerv);

	/* continue processing */
	return SMFIS_CONTINUE;
}

sfsistat
mlfi_eom(SMFICTX *ctx)
{
	char *lp, *val;

	if ((lp = smfi_getpriv(ctx)) == NULL)
		logexitx(1, "%s: smfi_getpriv == NULL", __func__);

	val = NULL;

	switch (*lp) {
	case 'W':
		val = "Whitelisted";
		break;
	case 'G':
		val = "Greylisted";
		break;
	default:
		logexitx(1, "unexpected list %d %c", *lp, *lp);
	}

	if (smfi_addheader(ctx, HEADER, val) == MI_FAILURE)
		logexitx(1, "%s smfi_addheader", __func__);

	free(lp);
	lp = NULL;
	if (smfi_setpriv(ctx, NULL) == MI_FAILURE)
		logexitx(1, "%s smfi_setpriv", __func__);

	loginfox("%s header added, list resource freed", __func__);

	/* continue processing */
	return SMFIS_CONTINUE;
}

sfsistat
mlfi_abort(SMFICTX *ctx)
{
	char *lp;

	if ((lp = smfi_getpriv(ctx)) == NULL)
		return SMFIS_CONTINUE;

	free(lp);
	lp = NULL;
	if (smfi_setpriv(ctx, NULL) == MI_FAILURE)
		logexitx(1, "%s smfi_setpriv", __func__);

	loginfox("%s list resource freed", __func__);

	/* ignored but we have to return something */
	return SMFIS_CONTINUE;
}

static void
printusage(FILE *stream)
{
	fprintf(stream, "usage: %s [-dqv] [-g group] acldb user chrootdir "
	    "sockaddr\n", progname);
}

int
main(int argc, char **argv)
{
	struct smfiDesc smfilter;
	struct stat st;
	char errstr[20];
	const char *chrootdir, *userstr, *groupstr;
	char *acldb, *sockaddr, *dir;
	size_t totrules, updrules;
	gid_t gid;
	uid_t uid;
	int c, foreground;

	uid = gid = 0;
	chrootdir = userstr = groupstr = NULL;

	if ((progname = basename(argv[0])) == NULL) {
		perror("basename");
		exit(1);
	}

	foreground = 0;
	while ((c = getopt(argc, argv, "dg:hqv")) != -1) {
		switch (c) {
		case 'd':
			foreground = 1;
			break;
		case 'g':
			groupstr = optarg;
			break;
		case 'h':
			printusage(stdout);
			exit(0);
                case 'q':
                        verbose--;
                        break;
                case 'v':
                        verbose++;
                        break;
		default:
			printusage(stderr);
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 4) {
		printusage(stderr);
		exit(1);
	}

	if (geteuid() != 0)
		logexitx(1, "must run as the superuser");

	acldb = argv[0];
	userstr = argv[1];
	chrootdir = argv[2];
	sockaddr = argv[3];

	if (resolveuser(&uid, &gid, userstr) == -1)
		logexit(1, "could not resolve user: %s", userstr);

	if (uid < 1)
		logexitx(1, "user is privileged: %s", userstr);

	if (gid < 1)
		logexitx(1, "user has a privileged primary group id: %s",
		    userstr);

	if (groupstr) {
		if (resolvegroup(&gid, groupstr) == -1)
			logexit(1, "could not resolve group: %s", groupstr);

		if (gid < 1)
			logexitx(1, "group is privileged: %s", groupstr);
	}

	if (a2acl_fromfile(acldb, &totrules, &updrules, errstr, sizeof(errstr))
	    == -1)
		logexit(1, "%s: %s\n", acldb, errstr);

	umask(0);

	/*
	 * Quick check on the last path component, let the user be
	 * responsible for the resolved absolute path.
	 */
	if (leafmodsuperuseronly(chrootdir) == 0)
		logexitx(1, "chroot can be modified by others than the"
		    " superuser: %s", chrootdir);

	if (chroot(chrootdir) == -1)
		logexit(1, "chroot failed");

	if (chdir("/") == -1)
		logexit(1, "chdir failed");

	if (dropuser(uid, gid) == -1)
		logexit(1, "dropping privileges failed");

	/*
	 * Strip unix and local prefixes, libmilter will recognize Unix domain
	 * sockets without it.
	 */

	if (strncmp(sockaddr, "unix:", 5) == 0) {
		sockaddr += 5;
	} else if (strncmp(sockaddr, "local:", 6) == 0) {
		sockaddr += 6;
	}

	/*
	 * Automatically unlink a Unix domain socket if it already exists.
	 */

	if (strncmp(sockaddr, "inet:", 5) != 0 &&
	    strncmp(sockaddr, "inet6:", 6) != 0) {
		if (stat(sockaddr, &st) == -1) {
			if (errno != ENOENT)
				logexit(1, "stat: %s in %s", sockaddr,
				    chrootdir);

			/* ENOENT is ok */
		} else {
			if (S_ISSOCK(st.st_mode) == 0)
				logexitx(1, "file exists and is not a socket: "
				    "%s in %s", sockaddr, chrootdir);

			if (st.st_uid != uid)
				logexitx(1, "socket not owned by us: %d %d %s ",
				    "in %s", st.st_uid, uid, sockaddr,
				    chrootdir);

			/*
			 * Never run unlink as a privileged user. Do a quick
			 * superuser check only.
			 */
			assert(geteuid() != 0);
			if (unlink(sockaddr) == -1)
				logexit(1, "unlink: %s in %s", sockaddr,
				    chrootdir);
		}

		/*
		 * Make sure we can write in the directory to prevent a cryptic
		 * failure of smfi_main later on.
		 */

		if ((dir = dirname(sockaddr)) == NULL)
			logexit(1, "dirname: %s", sockaddr);

		if (access(dir, W_OK | X_OK) == -1)
			logexit(1, "can't create %s in %s", sockaddr,
			    chrootdir);
	}

	if (!foreground) {
		background = 1;
		daemonize();
	}

	if (initlog("mail") == -1)
		logexitx(1, "could not init log");

	loginfox("running as %d:%d", geteuid(), getegid());

	loginfox("total policy rules: %zu, newly updated %zu\n", totrules,
	    updrules);

	if (smfi_setconn(sockaddr) == MI_FAILURE)
		logexitx(1, "smfi_setconn of %s in %s failed", sockaddr,
		    chrootdir);

	memset(&smfilter, 0, sizeof(smfilter));
	smfilter.xxfi_name = "A2ACL";
	smfilter.xxfi_version = SMFI_VERSION;
	smfilter.xxfi_flags = SMFIF_ADDHDRS;
	smfilter.xxfi_envfrom = mlfi_envfrom;
	smfilter.xxfi_envrcpt = mlfi_envrcpt;
	smfilter.xxfi_header = mlfi_header;
	smfilter.xxfi_eom = mlfi_eom;
	smfilter.xxfi_abort = mlfi_abort;

	if (smfi_register(smfilter) == MI_FAILURE)
		logexitx(1, "smfi_register failed");

	if (smfi_main() == MI_FAILURE) {
		logexitx(1, "smfi_main failed, check permissions of %s in %s",
		    sockaddr, chrootdir);
	}

	return 0;
}
