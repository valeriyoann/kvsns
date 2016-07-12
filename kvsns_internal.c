#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "kvsns.h"
#include <string.h>

int kvsns_next_inode(kvsns_ino_t *ino)
{
	int rc;
	if (!ino)
		return -EINVAL;

	rc = kvshl_incr_counter("ino_counter", ino);
	if (rc != 0)
		return rc;

	return 0;
}

int kvsns_str2parentlist(kvsns_ino_t *inolist, int *size, char *str)
{
	char *token;
	char *rest = str;
	int maxsize = *size;
	int pos = 0;

	if (!inolist || !str || !size)
		return -EINVAL;

	while((token = strtok_r(rest, "|", &rest))) {
		sscanf(token, "%llu", &inolist[pos++]);

		if (pos == maxsize)
			break;
	}

	*size = pos;

	return 0;
}

int kvsns_parentlist2str(kvsns_ino_t *inolist, int size, char *str)
{
	int i;
	char tmp[VLEN];

	if (!inolist || !str)
		return -EINVAL;

	strcpy(str, "");

	for (i=0; i < size ; i++)
		if (inolist[i] != 0LL) {
			snprintf(tmp, VLEN, "%llu|", inolist[i]);
			strcat(str, tmp);
		}
	
	return 0;	
}

int kvsns_create_entry(kvsns_cred_t *cred, kvsns_ino_t *parent, char *name,
			mode_t mode, kvsns_ino_t *new_entry, enum kvsns_type type)
{
	int rc;
	char k[KLEN];
	char v[KLEN];
	struct stat bufstat;

	if (!cred || !parent || !name || !new_entry)
		return -EINVAL;

	rc = kvsns_lookup(cred, parent, name, new_entry);
	if (rc == 0)
		return -EEXIST;

	rc = kvsns_next_inode(new_entry);
	if (rc != 0)
		return rc;

	kvshl_begin_transaction();
	snprintf(k, KLEN, "%llu.dentries.%s", 
		 *parent, name);
	snprintf(v, VLEN, "%llu", *new_entry);
	
	rc = kvshl_set_char(k, v);
	if (rc != 0)
		return rc;

	snprintf(k, KLEN, "%llu.parentdir", *new_entry);
	snprintf(v, VLEN, "%llu|", *parent);

	rc = kvshl_set_char(k, v);
	if (rc != 0)
		return rc;

	/* Set stat */
	memset(&bufstat, 0, sizeof(struct stat));
	bufstat.st_uid = getuid(); 
	bufstat.st_gid = getgid(); 
	bufstat.st_ino = *new_entry; 
	bufstat.st_atim.tv_sec = time(NULL);
	bufstat.st_mtim.tv_sec = bufstat.st_atim.tv_sec;
	bufstat.st_ctim.tv_sec = bufstat.st_atim.tv_sec;

	switch(type) {
	case KVSNS_DIR:
		bufstat.st_mode = S_IFDIR|mode;
		bufstat.st_nlink = 2;
		break;

	case KVSNS_FILE:
		bufstat.st_mode = S_IFREG|mode;
		bufstat.st_nlink = 1;
		break;

	case KVSNS_SYMLINK:
		bufstat.st_mode = S_IFLNK|mode;
		bufstat.st_nlink = 1;
		break;

	default:
		return -EINVAL;
	}

	snprintf(k, KLEN, "%llu.stat", *new_entry);
	rc = kvshl_set_stat(k, &bufstat);
	if (rc != 0)
		return rc;

	kvshl_end_transaction();
	return 0;
}
