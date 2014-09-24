/*
 * virtfs-convert: Convert file metadata to QEMU/VirtFS mapped-xattr format
 * Copyright (C) 2014  Daniel Gröber <dxld ÄT darkboxed DOT org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/xattr.h>

int FLAG_map_ids = 0;

static int local_set_xattr(char *path, struct stat *s)
{
    int err;

    if(FLAG_map_ids) {
            if(s->st_uid == getuid())
                    s->st_uid = 0;

            if(s->st_gid == getgid())
                    s->st_gid = 0;
    }

    err = setxattr(path, "user.virtfs.uid", &s->st_uid, sizeof(uid_t), 0);
    if (err)
            return err;

    err = setxattr(path, "user.virtfs.gid", &s->st_gid, sizeof(gid_t), 0);
    if (err)
            return err;

    fprintf(stderr, "Mode %s: %o\n", path, s->st_mode);
    err = setxattr(path, "user.virtfs.mode", &s->st_mode, sizeof(mode_t), 0);
    if (err)
            return err;

    err = setxattr(path, "user.virtfs.rdev", &s->st_rdev, sizeof(dev_t), 0);
    if (err)
            return err;

    return 0;
}

int full_write(int fd, char *buf, int len) {
	int rv;
	int index = 0;
	while(len > 0) {
		rv = write(fd, buf + index, len);
		if(rv < 0)
			return rv;

		index += rv;
		len -= rv;
	}

	return 0;
}

char* readlink_alloc(char* path, size_t *plen)
{
        struct stat s;
        int rv = lstat(path, &s);
        if(rv == -1) {
                perror("lstat: ");
                exit(1);
        }

        char *link = (char*)malloc(s.st_size + 1);
        ssize_t len = readlink(path, link, s.st_size);
        if(plen)
                *plen = len;

        return link;
}

void convert(char *path)
{
        ssize_t rv;
        struct stat s;

        rv = lgetxattr(path, "user.virtfs.uid", NULL, 0);
        if(rv == sizeof(uid_t)) {
                fprintf(stderr, "Already converted: %s\n", path);
                return;
        }

        rv = lstat(path, &s);
        if(rv == -1) {
                fprintf(stderr, "File: %s: ", path);
                perror("lstat");
                exit(1);
        }

        if((s.st_mode & S_IFMT) == S_IFLNK) {
                ssize_t rv;

                size_t len;
                char* link = readlink_alloc(path, &len);

                char* abs_tmpfile;
                {
                        char *tmpfile_tpl = "virtfs-convert.tmpXXXXXXXXXX";
                        char* t = strdup(path);
                        char* dir =  dirname(t);

                        size_t len = strlen(dir) + strlen(tmpfile_tpl) + 2;
                        abs_tmpfile = (char*) malloc(len);
                        abs_tmpfile[0] = '\0';

                        strcat(abs_tmpfile, dir);
                        strcat(abs_tmpfile, "/");
                        strcat(abs_tmpfile, tmpfile_tpl);

                        free(t);
                }

                int fd = mkstemp(abs_tmpfile);
                if(fd < 0) {
                        perror("mkstemp");
                        exit(1);
                }

                rv = full_write(fd, link, len);
                if(rv != 0) {
                        perror("write");
                        exit(1);
                }
                free(link);
                close(fd);

                rv = local_set_xattr(abs_tmpfile, &s);
                if(rv != 0) {
                        fprintf(stderr, "File: %s: ", path);
                        perror("setxattr");
                        exit(1);
                }

                rv = rename(abs_tmpfile, path);
                if(rv < 0) {
                        perror("rename");
                        exit(1);
                }
        } else if((s.st_mode & S_IFMT) == S_IFREG
                  || (s.st_mode & S_IFMT) == S_IFDIR) {

                if((s.st_mode & S_IFMT) == S_IFREG)
                        rv = chmod(path, 0600);
                else
                        rv = chmod(path, 0700);

                if(rv < 0) {
                        perror("chmod");
                        exit(1);
                }

                rv = local_set_xattr(path, &s);
                if(rv != 0) {
                        fprintf(stderr, "File: %s: ", path);
                        perror("setxattr");
                        exit(1);
                }
        } else {
                fprintf(stderr, "Warning: Unsupported file type: %s\n", path);
                return;

        }
}

int main(int argc, char* argv[])
{
        if(argc <= 1) {
                fprintf(stderr, "Usage: %s [-m] FILES...\n", argv[0]);
                exit(1);
        }

        int i, end_opts;
        for(i=1; i < argc; i++) {
                if(!end_opts) {
                        if(0 == strcmp(argv[i], "--")) {
                                end_opts = 1;
                                continue;
                        } else if(0 == strcmp(argv[i], "-m")) {
                                FLAG_map_ids = 1;
                                continue;
                        }
                }

                convert(argv[i]);
        }

        return 0;
}
