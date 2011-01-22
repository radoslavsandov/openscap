/**
 * @file   file.c
 * @brief  file probe
 * @author "Daniel Kopecek" <dkopecek@redhat.com>
 *
 * 2010/06/13 dkopecek@redhat.com
 *  This probe is able to process a file_object as defined in OVAL 5.4 and 5.5.
 *
 */

/*
 * Copyright 2009 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "Daniel Kopecek" <dkopecek@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <seap.h>
#include <probe-api.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>

#include "oval_fts.h"
#include "SEAP/generic/rbt/rbt.h"

#ifndef _A
#define _A(x) assert(x)
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#undef OS_FREEBSD
#undef OS_LINUX
#undef OS_SOLARIS
#undef OS_SUNOS
#undef OS_WINDOWS

#if defined(__FreeBSD__)
# define OS_FREEBSD
#elif defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)
# define OS_LINUX
#elif defined(sun) || defined(__sun)
# if defined(__SVR4) || defined(__svr4__)
#  define OS_SOLARIS
# else
#  define OS_SUNOS
# endif
#elif defined(_WIN32)
# define OS_WINDOWS
#else
# error "Sorry, your OS isn't supported."
#endif


static SEXP_t *gr_true   = NULL, *gr_false  = NULL, *gr_t_reg  = NULL;
static SEXP_t *gr_t_dir  = NULL, *gr_t_lnk  = NULL, *gr_t_blk  = NULL;
static SEXP_t *gr_t_fifo = NULL, *gr_t_sock = NULL, *gr_t_char = NULL;

static SEXP_t *strfiletype (mode_t mode)
{
        switch (mode & S_IFMT) {
        case S_IFREG:  return (gr_t_reg);
        case S_IFDIR:  return (gr_t_dir);
        case S_IFLNK:  return (gr_t_lnk);
        case S_IFBLK:  return (gr_t_blk);
        case S_IFIFO:  return (gr_t_fifo);
        case S_IFSOCK: return (gr_t_sock);
        case S_IFCHR:  return (gr_t_char);
        default:
                abort ();
        }
        /* NOTREACHED */
        return (NULL);
}

struct cbargs {
        SEXP_t *cobj;
	SEXP_t *filters;
	int     error;
};

static rbt_t   *g_ID_cache     = NULL;
static uint32_t g_ID_cache_max = 0; /* 0 = unlimited */

static SEXP_t *ID_cache_get(int32_t id)
{
	SEXP_t *s_id = NULL, *s_id2 = NULL;

	if (rbt_i32_get(g_ID_cache, id, (void *)&s_id) == 0)
		return SEXP_ref(s_id); /* cache hit (first attempt) */

	s_id = SEXP_string_newf("%u", (uint32_t)(id));

	if (g_ID_cache_max == 0 || rbt_i32_size(g_ID_cache) < g_ID_cache_max) {
		if (rbt_i32_add(g_ID_cache, id, (void *)s_id, NULL) == 0)
			return SEXP_ref(s_id); /* insert succeeded */

		if (rbt_i32_get(g_ID_cache, id, (void *)&s_id2) == 0) {
			SEXP_free (s_id); /* cache hit (second attempt) */
			return SEXP_ref(s_id2);
		}
	}

	/* fallback */
	return (s_id);
}

static void ID_cache_init(uint32_t max)
{
	g_ID_cache_max = max;
	g_ID_cache     = rbt_i32_new();
}

static void ID_cache_free_cb(rbt_i32_node_t *n)
{
	SEXP_free(n->data);
}

static void ID_cache_free(void)
{
	rbt_i32_free_cb(g_ID_cache, ID_cache_free_cb);
	g_ID_cache     = NULL;
	g_ID_cache_max = 0;
}

static int file_cb (const char *p, const char *f, void *ptr)
{
        char path_buffer[PATH_MAX];
        SEXP_t *cobj, *item, *filters;
        struct cbargs *args = (struct cbargs *) ptr;
        struct stat st;
        const char *st_path;

        cobj = args->cobj;
	filters = args->filters;

        if (f == NULL)
		return (0);

        snprintf (path_buffer, sizeof path_buffer, "%s/%s", p, f);
        st_path = path_buffer;

        if (lstat (st_path, &st) == -1) {
                _D("FAIL: errno=%u, %s.\n", errno, strerror (errno));
                return (-1);
        } else {
                SEXP_t *r0, *r2, *r1, *r3, *r4;
                SEXP_t *r5, *r6, *r7, *r8;

		r3 = ID_cache_get(st.st_uid);
		r4 = st.st_gid != st.st_uid ? ID_cache_get(st.st_gid) : SEXP_ref(r3);

                item = probe_item_creat ("file_item", NULL,
                                        /* entities */
                                        "path", NULL,
                                        r0 = SEXP_string_newf ("%s", p),

                                        "filename", NULL,
                                        r1 = (f != NULL ? SEXP_string_newf ("%s", f) : NULL),

					"filepath", NULL,
					r2 =  (f != NULL ? SEXP_string_newf ("%s/%s", p, f) : NULL),

                                        "type", NULL,
                                        strfiletype (st.st_mode),

					"group_id", NULL, r4,
					"user_id",  NULL, r3,

                                        "a_time", NULL,
                                        r5 = SEXP_string_newf (
#if defined(OS_FREEBSD)
# if (__STDC_VERSION__ >= 199901L)
						               "%jd", (intmax_t) st.st_atimespec.tv_sec
# else
							       "%lld", (unsigned long long) st.st_atimespec.tv_sec
# endif
#elif defined(OS_LINUX) || defined(OS_SOLARIS)
                                                               "%u", (unsigned int)st.st_atim.tv_sec
#endif
                                                ),

                                        "c_time", NULL,
                                        r6 = SEXP_string_newf (
#if defined(OS_FREEBSD)
# if (__STDC_VERSION__ >= 199901L)
						               "%jd", (intmax_t) st.st_ctimespec.tv_sec
# else
							       "%lld", (unsigned long long) st.st_ctimespec.tv_sec
# endif
#elif defined(OS_LINUX) || defined(OS_SOLARIS)
                                                               "%u", (unsigned int)st.st_ctim.tv_sec
#endif
                                                ),

                                        "m_time", NULL,
                                        r7 = SEXP_string_newf (
#if defined(OS_FREEBSD)
# if (__STDC_VERSION__ >= 199901L)
						               "%jd", (intmax_t) st.st_mtimespec.tv_sec
# else
							       "%lld", (unsigned long long) st.st_mtimespec.tv_sec
# endif
#elif defined(OS_LINUX) || defined(OS_SOLARIS)
                                                               "%u", (unsigned int)st.st_mtim.tv_sec
#endif
                                                ),

                                        "size", NULL,
#if defined(_FILE_OFFSET_BITS)
# if   _FILE_OFFSET_BITS == 64
                                        r8 = SEXP_number_newu_64 (st.st_size),
# elif _FILE_OFFSET_BITS == 32
                                        r8 = SEXP_number_newu_32 (st.st_size),
# else
#  error "Invalid _FILE_OFFSET_BITS value"
                                        r8 = NULL,
# endif
#elif defined(LARGE_FILE_SOURCE)
                                        r8 = SEXP_number_newu_64 (st.st_size),
#else
                                        r8 = SEXP_number_newu_32 (st.st_size),
#endif
                                        "suid", NULL,
                                        (st.st_mode & S_ISUID ? gr_true : gr_false),

                                        "sgid", NULL,
                                        (st.st_mode & S_ISGID ? gr_true : gr_false),

                                        "sticky", NULL,
                                        (st.st_mode & S_ISVTX ? gr_true : gr_false),

                                        "uread", NULL,
                                        (st.st_mode & S_IRUSR ? gr_true : gr_false),

                                        "uwrite", NULL,
                                        (st.st_mode & S_IWUSR ? gr_true : gr_false),

                                        "uexec", NULL,
                                        (st.st_mode & S_IXUSR ? gr_true : gr_false),

                                        "gread", NULL,
                                        (st.st_mode & S_IRGRP ? gr_true : gr_false),

                                        "gwrite", NULL,
                                        (st.st_mode & S_IWGRP ? gr_true : gr_false),

                                        "gexec", NULL,
                                        (st.st_mode & S_IXGRP ? gr_true : gr_false),

                                        "oread", NULL,
                                        (st.st_mode & S_IROTH ? gr_true : gr_false),

                                        "owrite", NULL,
                                        (st.st_mode & S_IWOTH ? gr_true : gr_false),

                                        "oexec", NULL,
                                        (st.st_mode & S_IXOTH ? gr_true : gr_false),
                                        NULL);


		SEXP_free(r1);
		SEXP_free(r2);
		SEXP_vfree(r0, r3, r4, r5, r6, r7, r8, NULL);

		if (!probe_item_filtered(item, filters)) {
			if (probe_cobj_add_item(cobj, item) != 0) {
				if (errno == ENOMEM)
					args->error = PROBE_ENOMEM;

				SEXP_free(item);
				return (-1);
			}
		}
		SEXP_free(item);
        }

        return (0);
}

static pthread_mutex_t __file_probe_mutex;

void *probe_init (void)
{
        _LOGCALL_;

        /*
         * Initialize true/false global reference.
         */
        gr_true  = SEXP_number_newb (true);
        gr_false = SEXP_number_newb (false);

        /*
         * Initialize file type string references.
         * (Used by strfiletype())
         */
#define STR_REGULAR   "regular"
#define STR_DIRECTORY "directory"
#define STR_SYMLINK   "symbolic link"
#define STR_BLKSPEC   "block special"
#define STR_FIFO      "fifo"
#define STR_SOCKET    "socket"
#define STR_CHARSPEC  "character special"
#define STRLEN_PAIR(str) (str), strlen(str)

        gr_t_reg  = SEXP_string_new (STRLEN_PAIR(STR_REGULAR));
        gr_t_dir  = SEXP_string_new (STRLEN_PAIR(STR_DIRECTORY));
        gr_t_lnk  = SEXP_string_new (STRLEN_PAIR(STR_SYMLINK));
        gr_t_blk  = SEXP_string_new (STRLEN_PAIR(STR_BLKSPEC));
        gr_t_fifo = SEXP_string_new (STRLEN_PAIR(STR_FIFO));
        gr_t_sock = SEXP_string_new (STRLEN_PAIR(STR_SOCKET));
        gr_t_char = SEXP_string_new (STRLEN_PAIR(STR_CHARSPEC));

	/*
	 * Initialize ID cache
	 */
	ID_cache_init(10000);

        /*
         * Initialize mutex.
         */
        switch (pthread_mutex_init (&__file_probe_mutex, NULL)) {
        case 0:
                return ((void *)&__file_probe_mutex);
        default:
                _D("Can't initialize mutex: errno=%u, %s.\n", errno, strerror (errno));
        }
#if 0
	probe_setoption(PROBE_VARREF_HANDLING, false, "path");
	probe_setoption(PROBE_VARREF_HANDLING, false, "filename");
#endif
        return (NULL);
}

void probe_fini (void *arg)
{
        _A((void *)arg == (void *)&__file_probe_mutex);

        /*
         * Release global reference.
         */
        SEXP_vfree (gr_true, gr_false, gr_t_reg,
                    gr_t_dir, gr_t_lnk, gr_t_blk,
                    gr_t_fifo, gr_t_sock, gr_t_char,
                    NULL);

	/*
	 * Free ID cache
	 */
	ID_cache_free();

        /*
         * Destroy mutex.
         */
        (void) pthread_mutex_destroy (&__file_probe_mutex);

        return;
}

int probe_main (SEXP_t *probe_in, SEXP_t *probe_out, void *mutex)
{
        SEXP_t *path, *filename, *behaviors, *filepath;
        SEXP_t *r0, *r1, *r2, *r3, *r4;
        int     filecnt, err;
        struct cbargs cbargs;
	OVAL_FTS    *ofts;
	OVAL_FTSENT *ofts_ent;

	if (probe_in == NULL || probe_out == NULL) {
		return (PROBE_EINVAL);
	}

        if (mutex == NULL) {
                return PROBE_EINIT;
	}

        _A(mutex == &__file_probe_mutex);

        path      = probe_obj_getent (probe_in, "path",      1);
        filename  = probe_obj_getent (probe_in, "filename",  1);
        behaviors = probe_obj_getent (probe_in, "behaviors", 1);
        filepath =  probe_obj_getent (probe_in, "filepath", 1);

	/* we want either path+filename or filepath */
        if ( (path == NULL || filename == NULL) && filepath==NULL ) {
                SEXP_free (behaviors);
                SEXP_free (path);
                SEXP_free (filename);
                SEXP_free (filepath);

                return PROBE_ENOELM;
        }

	/* behaviours are not important if filepath is used */
	if(filepath != NULL && behaviors != NULL) {
		SEXP_free (behaviors);
		behaviors = NULL;
	}

        if (behaviors == NULL) {
                SEXP_t *bh_list;

                bh_list = probe_ent_creat ("behaviors",
                                           r0 = probe_attr_creat ("max_depth",
                                                                  r1 = SEXP_string_newf ("-1"),
                                                                  "recurse",
                                                                  r2 = SEXP_string_newf ("symlinks and directories"),
                                                                  "recurse_direction",
                                                                  r3 = SEXP_string_newf ("none"),
                                                                  "recurse_file_system",
                                                                  r4 = SEXP_string_newf ("all"),
                                                                  NULL),
                                           NULL /* val */,
                                           NULL /* end */);

                behaviors = SEXP_list_first (bh_list);

                SEXP_vfree (bh_list,
                            r0, r1, r2, r3, r4, NULL);
        } else {
                if (!probe_ent_attrexists (behaviors, "max_depth")) {
                        probe_ent_attr_add (behaviors,
                                            "max_depth", r0 = SEXP_string_newf ("-1"));
                        SEXP_free (r0);
                }

                if (!probe_ent_attrexists (behaviors, "recurse")) {
                        probe_ent_attr_add (behaviors,
                                            "recurse", r0 = SEXP_string_newf ("symlinks and directories"));
                        SEXP_free (r0);
                }

                if (!probe_ent_attrexists (behaviors, "recurse_direction")) {
                        probe_ent_attr_add (behaviors,
                                            "recurse_direction", r0 = SEXP_string_newf ("none"));
                        SEXP_free (r0);
                }

                if (!probe_ent_attrexists (behaviors, "recurse_file_system")) {
                        probe_ent_attr_add (behaviors,
                                            "recurse_file_system", r0 = SEXP_string_newf ("all"));
                        SEXP_free (r0);
                }
        }

        _A(behaviors != NULL);

        switch (pthread_mutex_lock (&__file_probe_mutex)) {
        case 0:
                break;
        default:
                _D("Can't lock mutex(%p): %u, %s.\n", &__file_probe_mutex, errno, strerror (errno));

                return PROBE_EFATAL;
        }

        cbargs.cobj = probe_out;
	cbargs.filters = probe_prepare_filters(probe_in);
	cbargs.error = 0;

	filecnt = 0;

	if ((ofts = oval_fts_open(path, filename, filepath, behaviors)) != NULL) {
		for (; (ofts_ent = oval_fts_read(ofts)) != NULL; ++filecnt) {
			file_cb(ofts_ent->path, ofts_ent->file, &cbargs);
			oval_ftsent_free(ofts_ent);
		}
		oval_fts_close(ofts);
	}

	err = 0;

        if (filecnt < 0) {
		char s[50];
		SEXP_t *msg;

		snprintf(s, sizeof (s), "find_files returned error: %d", filecnt);
		msg = probe_msg_creat(OVAL_MESSAGE_LEVEL_ERROR, s);
		probe_cobj_add_msg(probe_out, msg);
		SEXP_free(msg);
		probe_cobj_set_flag(probe_out, SYSCHAR_FLAG_ERROR);

		err = cbargs.error;
        }

	SEXP_free(path);
	SEXP_free(filename);
	SEXP_free(filepath);
	SEXP_free(behaviors);
	SEXP_free(cbargs.filters);

        switch (pthread_mutex_unlock (&__file_probe_mutex)) {
        case 0:
                break;
        default:
                _D("Can't unlock mutex(%p): %u, %s.\n", &__file_probe_mutex, errno, strerror (errno));

                return PROBE_EFATAL;
        }

        return err;
}
