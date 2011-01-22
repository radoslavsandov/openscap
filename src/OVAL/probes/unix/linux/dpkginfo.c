/**
 * @file   dpkginfo.c
 * @brief  dpkginfo probe
 * @author "Pierre Chifflier" <chifflier@edenwall.com>
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
 *      "Pierre Chifflier <chifflier@edenwall.com>"
 */


/*
 * dpkginfo probe:
 *
 *  dpkginfo_object(string name)
 *
 *  dpkginfo_state(string name,
 *                string arch,
 *                string epoch,
 *                string release,
 *                string version,
 *                string evr,
 *                string signature_keyid)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

/* SEAP */
#include <seap.h>
#include <probe-api.h>
#include <alloc.h>


#include "dpkginfo-helper.h"


struct dpkginfo_global {
        pthread_mutex_t mutex;
};

static struct dpkginfo_global g_dpkg;


void *probe_init(void)
{
        pthread_mutex_init (&(g_dpkg.mutex), NULL);
        dpkginfo_init();

        return ((void *)&g_dpkg);
}

void probe_fini (void *ptr)
{
        struct dpkginfo_global *d = (struct dpkginfo_global *)ptr;

        pthread_mutex_destroy (&(d->mutex));
        dpkginfo_fini();

        return;
}

int probe_main (SEXP_t *object, SEXP_t *probe_out, void *arg)
{
        SEXP_t *val, *item_sexp, *r0;
        char *request_st = NULL;
        struct dpkginfo_reply_t *dpkginfo_reply = NULL;
        int errflag;

	if (object == NULL || probe_out == NULL) {
		return (PROBE_EINVAL);
	}

        val = probe_obj_getentval (object, "name", 1);

        if (val == NULL) {
                _D("%s: no value\n", "name");
                return (PROBE_ENOVAL);
        }

        request_st = SEXP_string_cstr (val);
        SEXP_free (val);

        if (request_st == NULL) {
                switch (errno) {
                case EINVAL:
                        _D("%s: invalid value type\n", "name");
			return PROBE_EINVAL;
                        break;
                case EFAULT:
                        _D("%s: element not found\n", "name");
			return PROBE_ENOELM;
                        break;
		default:
			return PROBE_EUNKNOWN;
                }
        }

        /* get info from debian apt cache */
        pthread_mutex_lock (&(g_dpkg.mutex));
        dpkginfo_reply = dpkginfo_get_by_name(request_st, &errflag);
        pthread_mutex_unlock (&(g_dpkg.mutex));

        if (dpkginfo_reply == NULL) {
                switch (errflag) {
		case 0: /* Not found */
		{
                                _D("Package \"%s\" not found.\n", request_st);
				/*
                                item_sexp = probe_item_creat ("dpkginfo_item", NULL,
                                                "name", NULL,
                                                r0 = SEXP_string_newf(request_st),
                                                NULL);

                                probe_item_setstatus (item_sexp, OVAL_STATUS_DOESNOTEXIST);
                                probe_itement_setstatus (item_sexp, "name", 1, OVAL_STATUS_DOESNOTEXIST);

				probe_cobj_add_item(probe_out, item_sexp);
                                SEXP_free (item_sexp);
                                SEXP_free (r0);
				*/
                                break;
		}
		case -1: /* Error */
		{
				char *s = "dpkginfo_get_by_name() failed.\n";
				SEXP_t *msg;

				_D(s);

				msg = probe_msg_creat(OVAL_MESSAGE_LEVEL_ERROR, s);
				probe_cobj_add_msg(probe_out, msg);
				SEXP_free(msg);
				probe_cobj_set_flag(probe_out, SYSCHAR_FLAG_ERROR);

                                break;
		}
                }
        } else { /* Ok */
                SEXP_t *r1, *r2, *r3, *r4, *r5;
                int i;
                int num_items = 1; /* FIXME */

                for (i = 0; i < num_items; ++i) {
                        item_sexp = probe_item_creat ("dpkginfo_item", NULL,

                                        "name", NULL,
                                        r0 = SEXP_string_newf (dpkginfo_reply->name),

                                        "arch", NULL,
                                        r1 = SEXP_string_newf (dpkginfo_reply->arch),

                                        "epoch", NULL,
                                        r2 = SEXP_string_newf (dpkginfo_reply->epoch),

                                        "release", NULL,
                                        r3 = SEXP_string_newf (dpkginfo_reply->release),

                                        "version", NULL,
                                        r4 = SEXP_string_newf (dpkginfo_reply->version),

                                        "evr", NULL,
                                        r5 = SEXP_string_newf (dpkginfo_reply->evr),

                                        NULL, NULL,
                                        NULL,

                                        NULL);

			probe_cobj_add_item(probe_out, item_sexp);
                        SEXP_free (item_sexp);
                        /* FIXME: this is... stupid */
                        SEXP_free (r0);
                        SEXP_free (r1);
                        SEXP_free (r2);
                        SEXP_free (r3);
                        SEXP_free (r4);
                        SEXP_free (r5);

                        dpkginfo_free_reply(dpkginfo_reply);
                }
        }

        return (0);
}
