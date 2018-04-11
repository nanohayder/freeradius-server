/*
 *   This program is is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or (at
 *   your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/**
 * $Id$
 * @file rlm_expiration.c
 * @brief Lockout user accounts based on control attributes.
 *
 * @copyright 2001,2006  The FreeRADIUS server project
 * @copyright 2004  Kostas Kalevras <kkalev@noc.ntua.gr>
 */
RCSID("$Id$")

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>

#include <ctype.h>

/*
 *      Check if account has expired, and if user may login now.
 */
static rlm_rcode_t CC_HINT(nonnull) mod_authorize(UNUSED void *instance, UNUSED void *thread, REQUEST *request)
{
	VALUE_PAIR *vp, *check_item = NULL;

	check_item = fr_pair_find_by_num(request->control, 0, FR_EXPIRATION, TAG_ANY);
	if (check_item != NULL) {
		/*
		*      Has this user's password expired?
		*
		*      If so, remove ALL reply attributes,
		*      and add our own Reply-Message, saying
		*      why they're being rejected.
		*/
		if (((time_t) check_item->vp_date) <= request->packet->timestamp.tv_sec) {
			REDEBUG("Account expired at '%pV'", &check_item->data);

			return RLM_MODULE_USERLOCK;
		} else {
			if (RDEBUG_ENABLED) RDEBUG("Account will expire at '%pV'", &check_item->data);
		}

		/*
		 *	Else the account hasn't expired, but it may do so
		 *	in the future.  Set Session-Timeout.
		 */
		vp = fr_pair_find_by_num(request->reply->vps, 0, FR_SESSION_TIMEOUT, TAG_ANY);
		if (!vp) {
			vp = radius_pair_create(request->reply, &request->reply->vps, FR_SESSION_TIMEOUT, 0);
			vp->vp_date = (uint32_t) (((time_t) check_item->vp_date) - request->packet->timestamp.tv_sec);
		} else if (vp->vp_date > ((uint32_t) (((time_t) check_item->vp_date) - request->packet->timestamp.tv_sec))) {
			vp->vp_date = (uint32_t) (((time_t) check_item->vp_date) - request->packet->timestamp.tv_sec);
		}
	} else {
		return RLM_MODULE_NOOP;
	}

	return RLM_MODULE_OK;
}

/*
 *      Compare the expiration date.
 */
static int expirecmp(UNUSED void *instance, REQUEST *req, UNUSED VALUE_PAIR *request, VALUE_PAIR *check,
		     UNUSED VALUE_PAIR *check_pairs, UNUSED VALUE_PAIR **reply_pairs)
{
	time_t now = 0;

	now = (req) ? req->packet->timestamp.tv_sec : time(NULL);

	if (now <= ((time_t) check->vp_date))
		return 0;
	return +1;
}


/*
 *	Do any per-module initialization that is separate to each
 *	configured instance of the module.  e.g. set up connections
 *	to external databases, read configuration files, set up
 *	dictionary entries, etc.
 *
 *	If configuration information is given in the config section
 *	that must be referenced in later calls, store a handle to it
 *	in *instance otherwise put a null pointer there.
 */
static int mod_instantiate(void *instance, UNUSED CONF_SECTION *conf)
{
	/*
	 *	Register the expiration comparison operation.
	 */
	paircompare_register(fr_dict_attr_by_num(NULL, 0, FR_EXPIRATION), NULL, false, expirecmp, instance);
	return 0;
}

/*
 *	The module name should be the only globally exported symbol.
 *	That is, everything else should be 'static'.
 *
 *	If the module needs to temporarily modify it's instantiation
 *	data, the type should be changed to RLM_TYPE_THREAD_UNSAFE.
 *	The server will then take care of ensuring that the module
 *	is single-threaded.
 */
extern rad_module_t rlm_expiration;
rad_module_t rlm_expiration = {
	.magic		= RLM_MODULE_INIT,
	.name		= "expiration",
	.type		= RLM_TYPE_THREAD_SAFE,
	.instantiate	= mod_instantiate,
	.methods = {
		[MOD_AUTHORIZE]		= mod_authorize,
		[MOD_POST_AUTH]		= mod_authorize
	},
};
