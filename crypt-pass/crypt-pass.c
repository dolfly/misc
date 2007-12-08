/* Copyright (C) Heikki Orsila 2003 <heikki.orsila@tut.fi> */

#define _XOPEN_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#define __USE_BSD
#include <string.h>

int main(int argc, char **argv) {
	char salt[3];
	FILE *rfile;
	/* only these characters are acceptable for the two salt characters */
	char *saltset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
	char *descset = "[A-Za-z0-9./]";
	int set_len;
	int i;
	char pass[10];

	if (argc < 2) {
		fprintf(stderr, "not enough args. you must give the password argument for %s\n\n", argv[0]);
		fprintf(stderr, "SYNOPSIS\n");
		fprintf(stderr, "\tcrypt-pass outputs the password hash for a given password. man 3 crypt\n\tfor more details.\n\n");
		fprintf(stderr, "USAGE\n");
		fprintf(stderr, "\t%s password [salt]\n\n", argv[0]);
		fprintf(stderr, "\tpassword is at most 8 characters. salt is at most 2 characters, and\n\tit is optional.\n");
		fprintf(stderr, "\tif password is - then the password is read from stdin,\n");
		fprintf(stderr, "\tbut salt is _not_ given from the command line.\n");
		return -1;
	}

	strncpy(pass, argv[1], 8);
	pass[8] = 0;

	/* read passwd from stdin. salt is either given or randomized. */
	if (strcmp(pass, "-") == 0) {
		int len;
		while (fgets(pass, sizeof(pass), stdin) == NULL) {
			if (feof(stdin) == NULL) {
				fprintf(stderr, "Could not read password\n");
				exit(1);
			}
		}
		len = strlen(pass);
		if (pass[len - 1] == '\n')
			pass[len - 1] = 0;
	}

	if (strlen (pass) == 0 || strlen(pass) > 8) {
		fprintf(stderr, "password must be longer than 0 characters, but at most 8 characters\n");
		return -1;
	}

	set_len = strlen(saltset);
	if (set_len != 64) {
		fprintf(stderr, "internal error: saltset is not 64 long\n");
		return -1;
	}
	if (argc < 3) {
		/* /dev/random is used for generating salt */
		if (!(rfile = fopen("/dev/urandom", "r"))) {
			fprintf(stderr, "can't open /dev/urandom\n");
			return -1;
		}
		if (fread(salt, 2, 1, rfile) != 1) {
			fprintf(stderr, "/dev/urandom read error\n");
			fclose(rfile);
			return -1;
		}
		fclose(rfile);
		for (i = 0; i < 2; i++) {
			salt[i] = saltset[((unsigned char) salt[i]) % set_len];
		}
		salt[2] = 0;
	} else {
		/* the user has given salt */
		if (strlen (argv[2]) != 2) {
			fprintf(stderr, "salt must be exactly 2 charaters given from the set: %s\n", descset);
			return -1;
		}
		for (i = 0; i < 2; i++) {
			if (!index(saltset, argv[2][i])) {
				fprintf(stderr, "all salt characters must be given from set: %s\n", descset);
				return -1;
			}
			salt[i] = argv[2][i];
		}
		salt[2] = 0;
	}

	printf("%s\n", crypt(pass, salt));
	return 0;
}
