/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2010-2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2015 Daniel Elstner <daniel.kitta@gmail.com>
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

#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <string.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "libopentracecapture-internal.h"

/** @cond PRIVATE */
#define LOG_PREFIX "session"
/** @endcond */

/**
 * @file
 *
 * Creating, using, or destroying libopentracecapture sessions.
 */

/**
 * @defgroup grp_session Session handling
 *
 * Creating, using, or destroying libopentracecapture sessions.
 *
 * @{
 */

struct datafeed_callback {
	otc_datafeed_callback cb;
	void *cb_data;
};

/** Custom GLib event source for generic descriptor I/O.
 * @see https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html
 */
struct fd_source {
	GSource base;

	int64_t timeout_us;
	int64_t due_us;

	/* Meta-data needed to keep track of installed sources */
	struct otc_session *session;
	void *key;

	GPollFD pollfd;
};

/** FD event source prepare() method.
 * This is called immediately before poll().
 */
static gboolean fd_source_prepare(GSource *source, int *timeout)
{
	int64_t now_us;
	struct fd_source *fsource;
	int remaining_ms;

	fsource = (struct fd_source *)source;

	if (fsource->timeout_us >= 0) {
		now_us = g_source_get_time(source);

		if (fsource->due_us == 0) {
			/* First-time initialization of the expiration time */
			fsource->due_us = now_us + fsource->timeout_us;
		}
		remaining_ms = (MAX(0, fsource->due_us - now_us) + 999) / 1000;
	} else {
		remaining_ms = -1;
	}
	*timeout = remaining_ms;

	return (remaining_ms == 0);
}

/** FD event source check() method.
 * This is called after poll() returns to check whether an event fired.
 */
static gboolean fd_source_check(GSource *source)
{
	struct fd_source *fsource;
	unsigned int revents;

	fsource = (struct fd_source *)source;
	revents = fsource->pollfd.revents;

	return (revents != 0 || (fsource->timeout_us >= 0
			&& fsource->due_us <= g_source_get_time(source)));
}

/** FD event source dispatch() method.
 * This is called if either prepare() or check() returned TRUE.
 */
static gboolean fd_source_dispatch(GSource *source,
		GSourceFunc callback, void *user_data)
{
	struct fd_source *fsource;
	unsigned int revents;
	gboolean keep;

	fsource = (struct fd_source *)source;
	revents = fsource->pollfd.revents;

	if (!callback) {
		otc_err("Callback not set, cannot dispatch event.");
		return G_SOURCE_REMOVE;
	}
	keep = (*OTC_RECEIVE_DATA_CALLBACK(callback))
			(fsource->pollfd.fd, revents, user_data);

	if (fsource->timeout_us >= 0 && G_LIKELY(keep)
			&& G_LIKELY(!g_source_is_destroyed(source)))
		fsource->due_us = g_source_get_time(source)
				+ fsource->timeout_us;
	return keep;
}

/** FD event source finalize() method.
 */
static void fd_source_finalize(GSource *source)
{
	struct fd_source *fsource;

	fsource = (struct fd_source *)source;

	otc_dbg("%s: key %p", __func__, fsource->key);

	otc_session_source_destroyed(fsource->session, fsource->key, source);
}

/** Create an event source for I/O on a file descriptor.
 *
 * In order to maintain API compatibility, this event source also doubles
 * as a timer event source.
 *
 * @param session The session the event source belongs to.
 * @param key The key used to identify this source.
 * @param fd The file descriptor or HANDLE.
 * @param events Events.
 * @param timeout_ms The timeout interval in ms, or -1 to wait indefinitely.
 *
 * @return A new event source object, or NULL on failure.
 */
static GSource *fd_source_new(struct otc_session *session, void *key,
		gintptr fd, int events, int timeout_ms)
{
	static GSourceFuncs fd_source_funcs = {
		.prepare  = &fd_source_prepare,
		.check    = &fd_source_check,
		.dispatch = &fd_source_dispatch,
		.finalize = &fd_source_finalize
	};
	GSource *source;
	struct fd_source *fsource;

	source = g_source_new(&fd_source_funcs, sizeof(struct fd_source));
	fsource = (struct fd_source *)source;

	g_source_set_name(source, (fd < 0) ? "timer" : "fd");

	if (timeout_ms >= 0) {
		fsource->timeout_us = 1000 * (int64_t)timeout_ms;
		fsource->due_us = 0;
	} else {
		fsource->timeout_us = -1;
		fsource->due_us = INT64_MAX;
	}
	fsource->session = session;
	fsource->key = key;

	fsource->pollfd.fd = fd;
	fsource->pollfd.events = events;
	fsource->pollfd.revents = 0;

	if (fd >= 0)
		g_source_add_poll(source, &fsource->pollfd);

	return source;
}

/**
 * Create a new session.
 *
 * @param ctx         The context in which to create the new session.
 * @param new_session This will contain a pointer to the newly created
 *                    session if the return value is OTC_OK, otherwise the value
 *                    is undefined and should not be used. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_new(struct otc_context *ctx,
		struct otc_session **new_session)
{
	struct otc_session *session;

	if (!new_session)
		return OTC_ERR_ARG;

	session = g_malloc0(sizeof(struct otc_session));

	session->ctx = ctx;

	g_mutex_init(&session->main_mutex);

	/* To maintain API compatibility, we need a lookup table
	 * which maps poll_object IDs to GSource* pointers.
	 */
	session->event_sources = g_hash_table_new(NULL, NULL);

	*new_session = session;

	return OTC_OK;
}

/**
 * Destroy a session.
 * This frees up all memory used by the session.
 *
 * @param session The session to destroy. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid session passed.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_destroy(struct otc_session *session)
{
	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}

	otc_session_dev_remove_all(session);
	g_slist_free_full(session->owned_devs, (GDestroyNotify)otc_dev_inst_free);

	otc_session_datafeed_callback_remove_all(session);

	g_hash_table_unref(session->event_sources);

	g_mutex_clear(&session->main_mutex);

	g_free(session);

	return OTC_OK;
}

/**
 * Remove all the devices from a session.
 *
 * The session itself (i.e., the struct otc_session) is not free'd and still
 * exists after this function returns.
 *
 * @param session The session to use. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_BUG Invalid session passed.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_dev_remove_all(struct otc_session *session)
{
	struct otc_dev_inst *sdi;
	GSList *l;

	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}

	for (l = session->devs; l; l = l->next) {
		sdi = (struct otc_dev_inst *) l->data;
		sdi->session = NULL;
	}

	g_slist_free(session->devs);
	session->devs = NULL;

	return OTC_OK;
}

/**
 * Add a device instance to a session.
 *
 * @param session The session to add to. Must not be NULL.
 * @param sdi The device instance to add to a session. Must not
 *            be NULL. Also, sdi->driver and sdi->driver->dev_open must
 *            not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_dev_add(struct otc_session *session,
		struct otc_dev_inst *sdi)
{
	int ret;

	if (!sdi) {
		otc_err("%s: sdi was NULL", __func__);
		return OTC_ERR_ARG;
	}

	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}

	/* If sdi->session is not NULL, the device is already in this or
	 * another session. */
	if (sdi->session) {
		otc_err("%s: already assigned to session", __func__);
		return OTC_ERR_ARG;
	}

	/* If sdi->driver is NULL, this is a virtual device. */
	if (!sdi->driver) {
		/* Just add the device, don't run dev_open(). */
		session->devs = g_slist_append(session->devs, sdi);
		sdi->session = session;
		return OTC_OK;
	}

	/* sdi->driver is non-NULL (i.e. we have a real device). */
	if (!sdi->driver->dev_open) {
		otc_err("%s: sdi->driver->dev_open was NULL", __func__);
		return OTC_ERR_BUG;
	}

	session->devs = g_slist_append(session->devs, sdi);
	sdi->session = session;

	/* TODO: This is invalid if the session runs in a different thread.
	 * The usage semantics and restrictions need to be documented.
	 */
	if (session->running) {
		/* Adding a device to a running session. Commit settings
		 * and start acquisition on that device now. */
		if ((ret = otc_config_commit(sdi)) != OTC_OK) {
			otc_err("Failed to commit device settings before "
			       "starting acquisition in running session (%s)",
			       otc_strerror(ret));
			return ret;
		}
		if ((ret = otc_dev_acquisition_start(sdi)) != OTC_OK) {
			otc_err("Failed to start acquisition of device in "
			       "running session (%s)", otc_strerror(ret));
			return ret;
		}
	}

	return OTC_OK;
}

/**
 * List all device instances attached to a session.
 *
 * @param session The session to use. Must not be NULL.
 * @param devlist A pointer where the device instance list will be
 *                stored on return. If no devices are in the session,
 *                this will be NULL. Each element in the list points
 *                to a struct otc_dev_inst *.
 *                The list must be freed by the caller, but not the
 *                elements pointed to.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_dev_list(struct otc_session *session, GSList **devlist)
{
	if (!session)
		return OTC_ERR_ARG;

	if (!devlist)
		return OTC_ERR_ARG;

	*devlist = g_slist_copy(session->devs);

	return OTC_OK;
}

/**
 * Remove a device instance from a session.
 *
 * @param session The session to remove from. Must not be NULL.
 * @param sdi The device instance to remove from a session. Must not
 *            be NULL. Also, sdi->driver and sdi->driver->dev_open must
 *            not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_dev_remove(struct otc_session *session,
		struct otc_dev_inst *sdi)
{
	if (!sdi) {
		otc_err("%s: sdi was NULL", __func__);
		return OTC_ERR_ARG;
	}

	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}

	/* If sdi->session is not session, the device is not in this
	 * session. */
	if (sdi->session != session) {
		otc_err("%s: not assigned to this session", __func__);
		return OTC_ERR_ARG;
	}

	session->devs = g_slist_remove(session->devs, sdi);
	sdi->session = NULL;

	return OTC_OK;
}

/**
 * Remove all datafeed callbacks in a session.
 *
 * @param session The session to use. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid session passed.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_datafeed_callback_remove_all(struct otc_session *session)
{
	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}

	g_slist_free_full(session->datafeed_callbacks, g_free);
	session->datafeed_callbacks = NULL;

	return OTC_OK;
}

/**
 * Add a datafeed callback to a session.
 *
 * @param session The session to use. Must not be NULL.
 * @param cb Function to call when a chunk of data is received.
 *           Must not be NULL.
 * @param cb_data Opaque pointer passed in by the caller.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_BUG No session exists.
 *
 * @since 0.3.0
 */
OTC_API int otc_session_datafeed_callback_add(struct otc_session *session,
		otc_datafeed_callback cb, void *cb_data)
{
	struct datafeed_callback *cb_struct;

	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_BUG;
	}

	if (!cb) {
		otc_err("%s: cb was NULL", __func__);
		return OTC_ERR_ARG;
	}

	cb_struct = g_malloc0(sizeof(struct datafeed_callback));
	cb_struct->cb = cb;
	cb_struct->cb_data = cb_data;

	session->datafeed_callbacks =
	    g_slist_append(session->datafeed_callbacks, cb_struct);

	return OTC_OK;
}

/**
 * Get the trigger assigned to this session.
 *
 * @param session The session to use.
 *
 * @retval NULL Invalid (NULL) session was passed to the function.
 * @retval other The trigger assigned to this session (can be NULL).
 *
 * @since 0.4.0
 */
OTC_API struct otc_trigger *otc_session_trigger_get(struct otc_session *session)
{
	if (!session)
		return NULL;

	return session->trigger;
}

/**
 * Set the trigger of this session.
 *
 * @param session The session to use. Must not be NULL.
 * @param trig The trigger to assign to this session. Can be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_trigger_set(struct otc_session *session, struct otc_trigger *trig)
{
	if (!session)
		return OTC_ERR_ARG;

	session->trigger = trig;

	return OTC_OK;
}

static int verify_trigger(struct otc_trigger *trigger)
{
	struct otc_trigger_stage *stage;
	struct otc_trigger_match *match;
	GSList *l, *m;

	if (!trigger->stages) {
		otc_err("No trigger stages defined.");
		return OTC_ERR;
	}

	otc_spew("Checking trigger:");
	for (l = trigger->stages; l; l = l->next) {
		stage = l->data;
		if (!stage->matches) {
			otc_err("Stage %d has no matches defined.", stage->stage);
			return OTC_ERR;
		}
		for (m = stage->matches; m; m = m->next) {
			match = m->data;
			if (!match->channel) {
				otc_err("Stage %d match has no channel.", stage->stage);
				return OTC_ERR;
			}
			if (!match->match) {
				otc_err("Stage %d match is not defined.", stage->stage);
				return OTC_ERR;
			}
			otc_spew("Stage %d match on channel %s, match %d", stage->stage,
					match->channel->name, match->match);
		}
	}

	return OTC_OK;
}

/** Set up the main context the session will be executing in.
 *
 * Must be called just before the session starts, by the thread which
 * will execute the session main loop. Once acquired, the main context
 * pointer is immutable for the duration of the session run.
 */
static int set_main_context(struct otc_session *session)
{
	GMainContext *main_context;

	g_mutex_lock(&session->main_mutex);

	/* May happen if otc_session_start() is called a second time
	 * while the session is still running.
	 */
	if (session->main_context) {
		otc_err("Main context already set.");

		g_mutex_unlock(&session->main_mutex);
		return OTC_ERR;
	}
	main_context = g_main_context_ref_thread_default();
	/*
	 * Try to use an existing main context if possible, but only if we
	 * can make it owned by the current thread. Otherwise, create our
	 * own main context so that event source callbacks can execute in
	 * the session thread.
	 */
	if (g_main_context_acquire(main_context)) {
		g_main_context_release(main_context);

		otc_dbg("Using thread-default main context.");
	} else {
		g_main_context_unref(main_context);

		otc_dbg("Creating our own main context.");
		main_context = g_main_context_new();
	}
	session->main_context = main_context;

	g_mutex_unlock(&session->main_mutex);

	return OTC_OK;
}

/** Unset the main context used for the current session run.
 *
 * Must be called right after stopping the session. Note that if the
 * session is stopped asynchronously, the main loop may still be running
 * after the main context has been unset. This is OK as long as no new
 * event sources are created -- the main loop holds its own reference
 * to the main context.
 */
static int unset_main_context(struct otc_session *session)
{
	int ret;

	g_mutex_lock(&session->main_mutex);

	if (session->main_context) {
		g_main_context_unref(session->main_context);
		session->main_context = NULL;
		ret = OTC_OK;
	} else {
		/* May happen if the set/unset calls are not matched.
		 */
		otc_err("No main context to unset.");
		ret = OTC_ERR;
	}
	g_mutex_unlock(&session->main_mutex);

	return ret;
}

static unsigned int session_source_attach(struct otc_session *session,
		GSource *source)
{
	unsigned int id = 0;

	g_mutex_lock(&session->main_mutex);

	if (session->main_context)
		id = g_source_attach(source, session->main_context);
	else
		otc_err("Cannot add event source without main context.");

	g_mutex_unlock(&session->main_mutex);

	return id;
}

/* Idle handler; invoked when the number of registered event sources
 * for a running session drops to zero.
 */
static gboolean delayed_stop_check(void *data)
{
	struct otc_session *session;

	session = data;
	session->stop_check_id = 0;

	/* Session already ended? */
	if (!session->running)
		return G_SOURCE_REMOVE;

	/* New event sources may have been installed in the meantime. */
	if (g_hash_table_size(session->event_sources) != 0)
		return G_SOURCE_REMOVE;

	session->running = FALSE;
	unset_main_context(session);

	otc_info("Stopped.");

	/* This indicates a bug in user code, since it is not valid to
	 * restart or destroy a session while it may still be running.
	 */
	if (!session->main_loop && !session->stopped_callback) {
		otc_err("BUG: Session stop left unhandled.");
		return G_SOURCE_REMOVE;
	}
	if (session->main_loop)
		g_main_loop_quit(session->main_loop);

	if (session->stopped_callback)
		(*session->stopped_callback)(session->stopped_cb_data);

	return G_SOURCE_REMOVE;
}

static int stop_check_later(struct otc_session *session)
{
	GSource *source;
	unsigned int source_id;

	if (session->stop_check_id != 0)
		return OTC_OK; /* idle handler already installed */

	source = g_idle_source_new();
	g_source_set_callback(source, &delayed_stop_check, session, NULL);

	source_id = session_source_attach(session, source);
	session->stop_check_id = source_id;

	g_source_unref(source);

	return (source_id != 0) ? OTC_OK : OTC_ERR;
}

/**
 * Start a session.
 *
 * When this function returns with a status code indicating success, the
 * session is running. Use otc_session_stopped_callback_set() to receive
 * notification upon completion, or call otc_session_run() to block until
 * the session stops.
 *
 * Session events will be processed in the context of the current thread.
 * If a thread-default GLib main context has been set, and is not owned by
 * any other thread, it will be used. Otherwise, libopentracecapture will create its
 * own main context for the current thread.
 *
 * @param session The session to use. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid session passed.
 * @retval OTC_ERR Other error.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_start(struct otc_session *session)
{
	struct otc_dev_inst *sdi;
	struct otc_channel *ch;
	GSList *l, *c, *lend;
	int ret;

	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}

	if (!session->devs) {
		otc_err("%s: session->devs was NULL; a session "
		       "cannot be started without devices.", __func__);
		return OTC_ERR_ARG;
	}

	if (session->running) {
		otc_err("Cannot (re-)start session while it is still running.");
		return OTC_ERR;
	}

	if (session->trigger) {
		ret = verify_trigger(session->trigger);
		if (ret != OTC_OK)
			return ret;
	}

	/* Check enabled channels and commit settings of all devices. */
	for (l = session->devs; l; l = l->next) {
		sdi = l->data;
		for (c = sdi->channels; c; c = c->next) {
			ch = c->data;
			if (ch->enabled)
				break;
		}
		if (!c) {
			otc_err("%s device %s has no enabled channels.",
				sdi->driver->name, sdi->connection_id);
			return OTC_ERR;
		}

		ret = otc_config_commit(sdi);
		if (ret != OTC_OK) {
			otc_err("Failed to commit %s device %s settings "
				"before starting acquisition.",
				sdi->driver->name, sdi->connection_id);
			return ret;
		}
	}

	ret = set_main_context(session);
	if (ret != OTC_OK)
		return ret;

	otc_info("Starting.");

	session->running = TRUE;

	/* Have all devices start acquisition. */
	for (l = session->devs; l; l = l->next) {
		if (!(sdi = l->data)) {
			otc_err("Device sdi was NULL, can't start session.");
			ret = OTC_ERR;
			break;
		}
		ret = otc_dev_acquisition_start(sdi);
		if (ret != OTC_OK) {
			otc_err("Could not start %s device %s acquisition.",
				sdi->driver->name, sdi->connection_id);
			break;
		}
	}

	if (ret != OTC_OK) {
		/* If there are multiple devices, some of them may already have
		 * started successfully. Stop them now before returning. */
		lend = l->next;
		for (l = session->devs; l != lend; l = l->next) {
			sdi = l->data;
			otc_dev_acquisition_stop(sdi);
		}
		/* TODO: Handle delayed stops. Need to iterate the event
		 * sources... */
		session->running = FALSE;

		unset_main_context(session);
		return ret;
	}

	if (g_hash_table_size(session->event_sources) == 0)
		stop_check_later(session);

	return OTC_OK;
}

/**
 * Block until the running session stops.
 *
 * This is a convenience function which creates a GLib main loop and runs
 * it to process session events until the session stops.
 *
 * Instead of using this function, applications may run their own GLib main
 * loop, and use otc_session_stopped_callback_set() to receive notification
 * when the session finished running.
 *
 * @param session The session to use. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid session passed.
 * @retval OTC_ERR Other error.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_run(struct otc_session *session)
{
	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}
	if (!session->running) {
		otc_err("No session running.");
		return OTC_ERR;
	}
	if (session->main_loop) {
		otc_err("Main loop already created.");
		return OTC_ERR;
	}

	g_mutex_lock(&session->main_mutex);

	if (!session->main_context) {
		otc_err("Cannot run without main context.");
		g_mutex_unlock(&session->main_mutex);
		return OTC_ERR;
	}
	session->main_loop = g_main_loop_new(session->main_context, FALSE);

	g_mutex_unlock(&session->main_mutex);

	g_main_loop_run(session->main_loop);

	g_main_loop_unref(session->main_loop);
	session->main_loop = NULL;

	return OTC_OK;
}

static gboolean session_stop_sync(void *user_data)
{
	struct otc_session *session;
	struct otc_dev_inst *sdi;
	GSList *node;

	session = user_data;

	if (!session->running)
		return G_SOURCE_REMOVE;

	otc_info("Stopping.");

	for (node = session->devs; node; node = node->next) {
		sdi = node->data;
		otc_dev_acquisition_stop(sdi);
	}

	return G_SOURCE_REMOVE;
}

/**
 * Stop a session.
 *
 * This requests the drivers of each device participating in the session to
 * abort the acquisition as soon as possible. Even after this function returns,
 * event processing still continues until all devices have actually stopped.
 *
 * Use otc_session_stopped_callback_set() to receive notification when the event
 * processing finished.
 *
 * This function is reentrant. That is, it may be called from a different
 * thread than the one executing the session, as long as it can be ensured
 * that the session object is valid.
 *
 * If the session is not running, otc_session_stop() silently does nothing.
 *
 * @param session The session to use. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid session passed.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_stop(struct otc_session *session)
{
	GMainContext *main_context;

	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}

	g_mutex_lock(&session->main_mutex);

	main_context = (session->main_context)
		? g_main_context_ref(session->main_context)
		: NULL;

	g_mutex_unlock(&session->main_mutex);

	if (!main_context) {
		otc_dbg("No main context set; already stopped?");
		/* Not an error; as it would be racy. */
		return OTC_OK;
	}
	g_main_context_invoke(main_context, &session_stop_sync, session);
	g_main_context_unref(main_context);

	return OTC_OK;
}

/**
 * Return whether the session is currently running.
 *
 * Note that this function should be called from the same thread
 * the session was started in.
 *
 * @param session The session to use. Must not be NULL.
 *
 * @retval TRUE Session is running.
 * @retval FALSE Session is not running.
 * @retval OTC_ERR_ARG Invalid session passed.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_is_running(struct otc_session *session)
{
	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}
	return session->running;
}

/**
 * Set the callback to be invoked after a session stopped running.
 *
 * Install a callback to receive notification when a session run stopped.
 * This can be used to integrate session execution with an existing main
 * loop, without having to block in otc_session_run().
 *
 * Note that the callback will be invoked in the context of the thread
 * that calls otc_session_start().
 *
 * @param session The session to use. Must not be NULL.
 * @param cb The callback to invoke on session stop. May be NULL to unset.
 * @param cb_data User data pointer to be passed to the callback.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid session passed.
 *
 * @since 0.4.0
 */
OTC_API int otc_session_stopped_callback_set(struct otc_session *session,
		otc_session_stopped_callback cb, void *cb_data)
{
	if (!session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_ARG;
	}
	session->stopped_callback = cb;
	session->stopped_cb_data = cb_data;

	return OTC_OK;
}

/**
 * Debug helper.
 *
 * @param packet The packet to show debugging information for.
 */
static void datafeed_dump(const struct otc_datafeed_packet *packet)
{
	const struct otc_datafeed_logic *logic;
	const struct otc_datafeed_analog *analog;

	/* Please use the same order as in libopentracecapture.h. */
	switch (packet->type) {
	case OTC_DF_HEADER:
		otc_dbg("bus: Received OTC_DF_HEADER packet.");
		break;
	case OTC_DF_END:
		otc_dbg("bus: Received OTC_DF_END packet.");
		break;
	case OTC_DF_META:
		otc_dbg("bus: Received OTC_DF_META packet.");
		break;
	case OTC_DF_TRIGGER:
		otc_dbg("bus: Received OTC_DF_TRIGGER packet.");
		break;
	case OTC_DF_LOGIC:
		logic = packet->payload;
		otc_dbg("bus: Received OTC_DF_LOGIC packet (%" PRIu64 " bytes, "
		       "unitsize = %d).", logic->length, logic->unitsize);
		break;
	case OTC_DF_FRAME_BEGIN:
		otc_dbg("bus: Received OTC_DF_FRAME_BEGIN packet.");
		break;
	case OTC_DF_FRAME_END:
		otc_dbg("bus: Received OTC_DF_FRAME_END packet.");
		break;
	case OTC_DF_ANALOG:
		analog = packet->payload;
		otc_dbg("bus: Received OTC_DF_ANALOG packet (%d samples).",
		       analog->num_samples);
		break;
	default:
		otc_dbg("bus: Received unknown packet type: %d.", packet->type);
		break;
	}
}

/**
 * Helper to send a meta datafeed package (OTC_DF_META) to the session bus.
 *
 * @param sdi The device instance to send the package from. Must not be NULL.
 * @param key The config key to send to the session bus.
 * @param var The value to send to the session bus.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @private
 */
OTC_PRIV int otc_session_send_meta(const struct otc_dev_inst *sdi,
		uint32_t key, GVariant *var)
{
	struct otc_config *cfg;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_meta meta;
	int ret;

	cfg = otc_config_new(key, var);

	memset(&meta, 0, sizeof(meta));

	packet.type = OTC_DF_META;
	packet.payload = &meta;

	meta.config = g_slist_append(NULL, cfg);

	ret = otc_session_send(sdi, &packet);
	g_slist_free(meta.config);
	otc_config_free(cfg);

	return ret;
}

/**
 * Send a packet to whatever is listening on the datafeed bus.
 *
 * Hardware drivers use this to send a data packet to the frontend.
 *
 * @param sdi TODO.
 * @param packet The datafeed packet to send to the session bus.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @private
 */
OTC_PRIV int otc_session_send(const struct otc_dev_inst *sdi,
		const struct otc_datafeed_packet *packet)
{
	GSList *l;
	struct datafeed_callback *cb_struct;
	struct otc_datafeed_packet *packet_in, *packet_out;
	struct otc_transform *t;
	int ret;

	if (!sdi) {
		otc_err("%s: sdi was NULL", __func__);
		return OTC_ERR_ARG;
	}

	if (!packet) {
		otc_err("%s: packet was NULL", __func__);
		return OTC_ERR_ARG;
	}

	if (!sdi->session) {
		otc_err("%s: session was NULL", __func__);
		return OTC_ERR_BUG;
	}

	/*
	 * Pass the packet to the first transform module. If that returns
	 * another packet (instead of NULL), pass that packet to the next
	 * transform module in the list, and so on.
	 */
	packet_in = (struct otc_datafeed_packet *)packet;
	for (l = sdi->session->transforms; l; l = l->next) {
		t = l->data;
		otc_spew("Running transform module '%s'.", t->module->id);
		ret = t->module->receive(t, packet_in, &packet_out);
		if (ret < 0) {
			otc_err("Error while running transform module: %d.", ret);
			return OTC_ERR;
		}
		if (!packet_out) {
			/*
			 * If any of the transforms don't return an output
			 * packet, abort.
			 */
			otc_spew("Transform module didn't return a packet, aborting.");
			return OTC_OK;
		} else {
			/*
			 * Use this transform module's output packet as input
			 * for the next transform module.
			 */
			packet_in = packet_out;
		}
	}
	packet = packet_in;

	/*
	 * If the last transform did output a packet, pass it to all datafeed
	 * callbacks.
	 */
	for (l = sdi->session->datafeed_callbacks; l; l = l->next) {
		if (otc_log_loglevel_get() >= OTC_LOG_DBG)
			datafeed_dump(packet);
		cb_struct = l->data;
		cb_struct->cb(sdi, packet, cb_struct->cb_data);
	}

	return OTC_OK;
}

/**
 * Add an event source for a file descriptor.
 *
 * @param session The session to use. Must not be NULL.
 * @param key The key which identifies the event source.
 * @param source An event source object. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 * @retval OTC_ERR_BUG Event source with @a key already installed.
 * @retval OTC_ERR Other error.
 *
 * @private
 */
OTC_PRIV int otc_session_source_add_internal(struct otc_session *session,
		void *key, GSource *source)
{
	/*
	 * This must not ever happen, since the source has already been
	 * created and its finalize() method will remove the key for the
	 * already installed source. (Well it would, if we did not have
	 * another sanity check there.)
	 */
	if (g_hash_table_contains(session->event_sources, key)) {
		otc_err("Event source with key %p already exists.", key);
		return OTC_ERR_BUG;
	}
	g_hash_table_insert(session->event_sources, key, source);

	if (session_source_attach(session, source) == 0)
		return OTC_ERR;

	return OTC_OK;
}

/** @private */
OTC_PRIV int otc_session_fd_source_add(struct otc_session *session,
		void *key, gintptr fd, int events, int timeout,
		otc_receive_data_callback cb, void *cb_data)
{
	GSource *source;
	int ret;

	source = fd_source_new(session, key, fd, events, timeout);
	if (!source)
		return OTC_ERR;

	g_source_set_callback(source, G_SOURCE_FUNC(cb), cb_data, NULL);

	ret = otc_session_source_add_internal(session, key, source);
	g_source_unref(source);

	return ret;
}

/**
 * Add an event source for a file descriptor.
 *
 * @param session The session to use. Must not be NULL.
 * @param fd The file descriptor, or a negative value to create a timer source.
 * @param events Events to check for.
 * @param timeout Max time in ms to wait before the callback is called,
 *                or -1 to wait indefinitely.
 * @param cb Callback function to add. Must not be NULL.
 * @param cb_data Data for the callback function. Can be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @since 0.3.0
 * @private
 */
OTC_PRIV int otc_session_source_add(struct otc_session *session, int fd,
		int events, int timeout, otc_receive_data_callback cb, void *cb_data)
{
	if (fd < 0 && timeout < 0) {
		otc_err("Cannot create timer source without timeout.");
		return OTC_ERR_ARG;
	}
	return otc_session_fd_source_add(session, GINT_TO_POINTER(fd),
			fd, events, timeout, cb, cb_data);
}

/**
 * Add an event source for a GPollFD.
 *
 * @param session The session to use. Must not be NULL.
 * @param pollfd The GPollFD. Must not be NULL.
 * @param timeout Max time in ms to wait before the callback is called,
 *                or -1 to wait indefinitely.
 * @param cb Callback function to add. Must not be NULL.
 * @param cb_data Data for the callback function. Can be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @since 0.3.0
 * @private
 */
OTC_PRIV int otc_session_source_add_pollfd(struct otc_session *session,
		GPollFD *pollfd, int timeout, otc_receive_data_callback cb,
		void *cb_data)
{
	if (!pollfd) {
		otc_err("%s: pollfd was NULL", __func__);
		return OTC_ERR_ARG;
	}
	return otc_session_fd_source_add(session, pollfd, pollfd->fd,
			pollfd->events, timeout, cb, cb_data);
}

/**
 * Add an event source for a GIOChannel.
 *
 * @param session The session to use. Must not be NULL.
 * @param channel The GIOChannel.
 * @param events Events to poll on.
 * @param timeout Max time in ms to wait before the callback is called,
 *                or -1 to wait indefinitely.
 * @param cb Callback function to add. Must not be NULL.
 * @param cb_data Data for the callback function. Can be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 *
 * @since 0.3.0
 * @private
 */
OTC_PRIV int otc_session_source_add_channel(struct otc_session *session,
		GIOChannel *channel, int events, int timeout,
		otc_receive_data_callback cb, void *cb_data)
{
	GPollFD pollfd;

	if (!channel) {
		otc_err("%s: channel was NULL", __func__);
		return OTC_ERR_ARG;
	}
	/* We should be using g_io_create_watch(), but can't without
	 * changing the driver API, as the callback signature is different.
	 */
#ifdef _WIN32
	g_io_channel_win32_make_pollfd(channel, events, &pollfd);
#else
	pollfd.fd = g_io_channel_unix_get_fd(channel);
	pollfd.events = events;
#endif
	return otc_session_fd_source_add(session, channel, pollfd.fd,
			pollfd.events, timeout, cb, cb_data);
}

/**
 * Remove the source identified by the specified poll object.
 *
 * @param session The session to use. Must not be NULL.
 * @param key The key by which the source is identified.
 *
 * @retval OTC_OK Success
 * @retval OTC_ERR_BUG No event source for poll_object found.
 *
 * @private
 */
OTC_PRIV int otc_session_source_remove_internal(struct otc_session *session,
		void *key)
{
	GSource *source;

	source = g_hash_table_lookup(session->event_sources, key);
	/*
	 * Trying to remove an already removed event source is problematic
	 * since the poll_object handle may have been reused in the meantime.
	 */
	if (!source) {
		otc_warn("Cannot remove non-existing event source %p.", key);
		return OTC_ERR_BUG;
	}
	g_source_destroy(source);

	return OTC_OK;
}

/**
 * Remove the source belonging to the specified file descriptor.
 *
 * @param session The session to use. Must not be NULL.
 * @param fd The file descriptor for which the source should be removed.
 *
 * @retval OTC_OK Success
 * @retval OTC_ERR_ARG Invalid argument
 * @retval OTC_ERR_BUG Internal error.
 *
 * @since 0.3.0
 * @private
 */
OTC_PRIV int otc_session_source_remove(struct otc_session *session, int fd)
{
	return otc_session_source_remove_internal(session, GINT_TO_POINTER(fd));
}

/**
 * Remove the source belonging to the specified poll descriptor.
 *
 * @param session The session to use. Must not be NULL.
 * @param pollfd The poll descriptor for which the source should be removed.
 *               Must not be NULL.
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments, or
 *         OTC_ERR_MALLOC upon memory allocation errors, OTC_ERR_BUG upon
 *         internal errors.
 *
 * @since 0.2.0
 * @private
 */
OTC_PRIV int otc_session_source_remove_pollfd(struct otc_session *session,
		GPollFD *pollfd)
{
	if (!pollfd) {
		otc_err("%s: pollfd was NULL", __func__);
		return OTC_ERR_ARG;
	}
	return otc_session_source_remove_internal(session, pollfd);
}

/**
 * Remove the source belonging to the specified channel.
 *
 * @param session The session to use. Must not be NULL.
 * @param channel The channel for which the source should be removed.
 *                Must not be NULL.
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 * @return OTC_ERR_BUG Internal error.
 *
 * @since 0.2.0
 * @private
 */
OTC_PRIV int otc_session_source_remove_channel(struct otc_session *session,
		GIOChannel *channel)
{
	if (!channel) {
		otc_err("%s: channel was NULL", __func__);
		return OTC_ERR_ARG;
	}
	return otc_session_source_remove_internal(session, channel);
}

/** Unregister an event source that has been destroyed.
 *
 * This is intended to be called from a source's finalize() method.
 *
 * @param session The session to use. Must not be NULL.
 * @param key The key used to identify @a source.
 * @param source The source object that was destroyed.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_BUG Event source for @a key does not match @a source.
 * @retval OTC_ERR Other error.
 *
 * @private
 */
OTC_PRIV int otc_session_source_destroyed(struct otc_session *session,
		void *key, GSource *source)
{
	GSource *registered_source;

	registered_source = g_hash_table_lookup(session->event_sources, key);
	/*
	 * Trying to remove an already removed event source is problematic
	 * since the poll_object handle may have been reused in the meantime.
	 */
	if (!registered_source) {
		otc_err("No event source for key %p found.", key);
		return OTC_ERR_BUG;
	}
	if (registered_source != source) {
		otc_err("Event source for key %p does not match"
			" destroyed source.", key);
		return OTC_ERR_BUG;
	}
	g_hash_table_remove(session->event_sources, key);

	if (g_hash_table_size(session->event_sources) > 0)
		return OTC_OK;

	/* If no event sources are left, consider the acquisition finished.
	 * This is pretty crude, as it requires all event sources to be
	 * registered via the libopentracecapture API.
	 */
	return stop_check_later(session);
}

static void copy_src(struct otc_config *src, struct otc_datafeed_meta *meta_copy)
{
	struct otc_config *item;

#if GLIB_CHECK_VERSION(2, 67, 3)
	item = g_memdup2(src, sizeof(*src));
#else
	item = g_memdup(src, sizeof(*src));
#endif

	g_variant_ref(src->data);
	meta_copy->config = g_slist_append(meta_copy->config, item);
}

OTC_API int otc_packet_copy(const struct otc_datafeed_packet *packet,
		struct otc_datafeed_packet **copy)
{
	const struct otc_datafeed_meta *meta;
	struct otc_datafeed_meta *meta_copy;
	const struct otc_datafeed_logic *logic;
	struct otc_datafeed_logic *logic_copy;
	const struct otc_datafeed_analog *analog;
	struct otc_datafeed_analog *analog_copy;
	struct otc_analog_encoding *encoding_copy;
	struct otc_analog_meaning *meaning_copy;
	struct otc_analog_spec *spec_copy;
	uint8_t *payload;

	*copy = g_malloc0(sizeof(struct otc_datafeed_packet));
	(*copy)->type = packet->type;

	switch (packet->type) {
	case OTC_DF_TRIGGER:
	case OTC_DF_END:
		/* No payload. */
		break;
	case OTC_DF_HEADER:
		payload = g_malloc(sizeof(struct otc_datafeed_header));
		memcpy(payload, packet->payload, sizeof(struct otc_datafeed_header));
		(*copy)->payload = payload;
		break;
	case OTC_DF_META:
		meta = packet->payload;
		meta_copy = g_malloc0(sizeof(struct otc_datafeed_meta));
		g_slist_foreach(meta->config, (GFunc)copy_src, meta_copy->config);
		(*copy)->payload = meta_copy;
		break;
	case OTC_DF_LOGIC:
		logic = packet->payload;
		logic_copy = g_malloc(sizeof(*logic_copy));
		if (!logic_copy)
			return OTC_ERR;
		logic_copy->length = logic->length;
		logic_copy->unitsize = logic->unitsize;
		logic_copy->data = g_malloc(logic->length * logic->unitsize);
		if (!logic_copy->data) {
			g_free(logic_copy);
			return OTC_ERR;
		}
		memcpy(logic_copy->data, logic->data, logic->length * logic->unitsize);
		(*copy)->payload = logic_copy;
		break;
	case OTC_DF_ANALOG:
		analog = packet->payload;
		analog_copy = g_malloc(sizeof(*analog_copy));
		analog_copy->data = g_malloc(
				analog->encoding->unitsize * analog->num_samples);
		memcpy(analog_copy->data, analog->data,
				analog->encoding->unitsize * analog->num_samples);
		analog_copy->num_samples = analog->num_samples;
#if GLIB_CHECK_VERSION(2, 67, 3)
		encoding_copy = g_memdup2(analog->encoding, sizeof(*analog->encoding));
		meaning_copy = g_memdup2(analog->meaning, sizeof(*analog->meaning));
		spec_copy = g_memdup2(analog->spec, sizeof(*analog->spec));
#else
		encoding_copy = g_memdup(analog->encoding, sizeof(*analog->encoding));
		meaning_copy = g_memdup(analog->meaning, sizeof(*analog->meaning));
		spec_copy = g_memdup(analog->spec, sizeof(*analog->spec));
#endif
		analog_copy->encoding = encoding_copy;
		analog_copy->meaning = meaning_copy;
		analog_copy->meaning->channels = g_slist_copy(
				analog->meaning->channels);
		analog_copy->spec = spec_copy;
		(*copy)->payload = analog_copy;
		break;
	default:
		otc_err("Unknown packet type %d", packet->type);
		return OTC_ERR;
	}

	return OTC_OK;
}

OTC_API void otc_packet_free(struct otc_datafeed_packet *packet)
{
	const struct otc_datafeed_meta *meta;
	const struct otc_datafeed_logic *logic;
	const struct otc_datafeed_analog *analog;
	struct otc_config *src;
	GSList *l;

	switch (packet->type) {
	case OTC_DF_TRIGGER:
	case OTC_DF_END:
		/* No payload. */
		break;
	case OTC_DF_HEADER:
		/* Payload is a simple struct. */
		g_free((void *)packet->payload);
		break;
	case OTC_DF_META:
		meta = packet->payload;
		for (l = meta->config; l; l = l->next) {
			src = l->data;
			g_variant_unref(src->data);
			g_free(src);
		}
		g_slist_free(meta->config);
		g_free((void *)packet->payload);
		break;
	case OTC_DF_LOGIC:
		logic = packet->payload;
		g_free(logic->data);
		g_free((void *)packet->payload);
		break;
	case OTC_DF_ANALOG:
		analog = packet->payload;
		g_free(analog->data);
		g_free(analog->encoding);
		g_slist_free(analog->meaning->channels);
		g_free(analog->meaning);
		g_free(analog->spec);
		g_free((void *)packet->payload);
		break;
	default:
		otc_err("Unknown packet type %d", packet->type);
	}
	g_free(packet);
}

/** @} */
