/*
 * hako-launcher: Bluetooth through BlueZ on the system D-Bus (sd-bus).
 *
 * Lists the adapter's devices (org.freedesktop.DBus.ObjectManager), switches
 * the adapter on and off, searches, and pairs / connects / disconnects /
 * forgets devices. Pairing needs an agent: we register one with the
 * NoInputNoOutput capability that accepts every request, which is what game
 * pads and headphones expect ("just works" pairing). Long calls (Pair,
 * Connect) are asynchronous; bt_poll() runs their callbacks from the main loop.
 */
#include "launcher.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>

#define BLUEZ "org.bluez"
#define ADAPTER "/org/bluez/hci0"
#define AGENT_PATH "/org/hako/launcher/agent"

static sd_bus *bus;
static sd_bus_slot *agent_slot;
static bool changed;          /* something happened since the last bt_poll() */
static char message[256];     /* result of an asynchronous operation, for a toast */
static char busy[BT_PATH_MAX];  /* device with an operation in progress */
static BtBusy busy_op;

/* ------------------------------------------------------------------ agent */

static int agent_ok(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u; (void)e;
	return sd_bus_reply_method_return(m, "");
}

static int agent_pin(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u; (void)e;
	return sd_bus_reply_method_return(m, "s", "0000");
}

static int agent_passkey(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u; (void)e;
	return sd_bus_reply_method_return(m, "u", (uint32_t)0);
}

static const sd_bus_vtable agent_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Release", "", "", agent_ok, 0),
	SD_BUS_METHOD("RequestPinCode", "o", "s", agent_pin, 0),
	SD_BUS_METHOD("DisplayPinCode", "os", "", agent_ok, 0),
	SD_BUS_METHOD("RequestPasskey", "o", "u", agent_passkey, 0),
	SD_BUS_METHOD("DisplayPasskey", "ouq", "", agent_ok, 0),
	SD_BUS_METHOD("RequestConfirmation", "ou", "", agent_ok, 0),
	SD_BUS_METHOD("RequestAuthorization", "o", "", agent_ok, 0),
	SD_BUS_METHOD("AuthorizeService", "os", "", agent_ok, 0),
	SD_BUS_METHOD("Cancel", "", "", agent_ok, 0),
	SD_BUS_VTABLE_END
};

/* ------------------------------------------------------------------ setup */

static int on_signal(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)m; (void)u; (void)e;
	changed = true;
	return 0;
}

bool bt_open(void)
{
	if (bus)
		return true;
	int r = sd_bus_open_system(&bus);
	if (r < 0) {
		SDL_Log("bt: system bus: %s", strerror(-r));
		bus = NULL;
		return false;
	}
	r = sd_bus_add_object_vtable(bus, &agent_slot, AGENT_PATH, "org.bluez.Agent1", agent_vtable, NULL);
	if (r < 0)
		SDL_Log("bt: agent object: %s", strerror(-r));
	/* any change of devices or properties under BlueZ */
	sd_bus_match_signal(bus, NULL, BLUEZ, NULL, "org.freedesktop.DBus.Properties", "PropertiesChanged", on_signal, NULL);
	sd_bus_match_signal(bus, NULL, BLUEZ, "/", "org.freedesktop.DBus.ObjectManager", "InterfacesAdded", on_signal, NULL);
	sd_bus_match_signal(bus, NULL, BLUEZ, "/", "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved", on_signal, NULL);

	sd_bus_error err = SD_BUS_ERROR_NULL;
	r = sd_bus_call_method(bus, BLUEZ, "/org/bluez", "org.bluez.AgentManager1", "RegisterAgent", &err, NULL,
	                       "os", AGENT_PATH, "NoInputNoOutput");
	if (r < 0)
		SDL_Log("bt: RegisterAgent: %s", err.message ? err.message : strerror(-r));
	else {
		sd_bus_error_free(&err);
		r = sd_bus_call_method(bus, BLUEZ, "/org/bluez", "org.bluez.AgentManager1", "RequestDefaultAgent", &err,
		                       NULL, "o", AGENT_PATH);
		if (r < 0)
			SDL_Log("bt: RequestDefaultAgent: %s", err.message ? err.message : strerror(-r));
	}
	sd_bus_error_free(&err);
	return true;
}

void bt_close(void)
{
	if (!bus)
		return;
	sd_bus_call_method(bus, BLUEZ, ADAPTER, "org.bluez.Adapter1", "StopDiscovery", NULL, NULL, "");
	sd_bus_call_method(bus, BLUEZ, "/org/bluez", "org.bluez.AgentManager1", "UnregisterAgent", NULL, NULL,
	                   "o", AGENT_PATH);
	agent_slot = sd_bus_slot_unref(agent_slot);
	bus = sd_bus_flush_close_unref(bus);
}

bool bt_poll(void)
{
	if (!bus)
		return false;
	while (sd_bus_process(bus, NULL) > 0)
		;
	bool c = changed;
	changed = false;
	return c;
}

bool bt_take_message(char *buf, size_t n)
{
	if (!message[0])
		return false;
	snprintf(buf, n, "%s", message);
	message[0] = 0;
	return true;
}

/* --------------------------------------------------------------- listing */

static int skip_variant(sd_bus_message *m)
{
	return sd_bus_message_skip(m, "v");
}

static int read_string_variant(sd_bus_message *m, char *buf, size_t n)
{
	const char *s;
	int r = sd_bus_message_read(m, "v", "s", &s);
	if (r >= 0)
		snprintf(buf, n, "%s", s);
	return r;
}

static int read_bool_variant(sd_bus_message *m, bool *b)
{
	int v = 0, r = sd_bus_message_read(m, "v", "b", &v);
	if (r >= 0)
		*b = v;
	return r;
}

/* a{sv} of one interface */
static int read_props(sd_bus_message *m, BtState *st, BtDevice *d, bool adapter)
{
	int r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0)
		return r;
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
		const char *key;
		if ((r = sd_bus_message_read(m, "s", &key)) < 0)
			return r;
		if (adapter && !strcmp(key, "Powered"))
			r = read_bool_variant(m, &st->powered);
		else if (adapter && !strcmp(key, "Discovering"))
			r = read_bool_variant(m, &st->discovering);
		else if (!adapter && !strcmp(key, "Address"))
			r = read_string_variant(m, d->addr, sizeof d->addr);
		else if (!adapter && !strcmp(key, "Alias"))
			r = read_string_variant(m, d->name, sizeof d->name);
		else if (!adapter && !strcmp(key, "Name")) {
			r = read_string_variant(m, d->name, sizeof d->name);
			d->has_name = true;
		} else if (!adapter && !strcmp(key, "Icon"))
			r = read_string_variant(m, d->icon, sizeof d->icon);
		else if (!adapter && !strcmp(key, "Paired"))
			r = read_bool_variant(m, &d->paired);
		else if (!adapter && !strcmp(key, "Connected"))
			r = read_bool_variant(m, &d->connected);
		else if (!adapter && !strcmp(key, "Trusted"))
			r = read_bool_variant(m, &d->trusted);
		else if (!adapter && !strcmp(key, "RSSI")) {
			int16_t v = 0;
			r = sd_bus_message_read(m, "v", "n", &v);
			d->rssi = v;
			d->has_rssi = true;
		} else
			r = skip_variant(m);
		if (r < 0)
			return r;
		if ((r = sd_bus_message_exit_container(m)) < 0)
			return r;
	}
	if (r < 0)
		return r;
	return sd_bus_message_exit_container(m);
}

static int dev_order(const void *a, const void *b)
{
	const BtDevice *x = a, *y = b;
	int kx = x->connected ? 0 : x->paired ? 1 : 2, ky = y->connected ? 0 : y->paired ? 1 : 2;
	if (kx != ky)
		return kx - ky;
	if (kx == 2 && x->rssi != y->rssi)
		return y->rssi - x->rssi;  /* nearer first */
	return strcmp(x->name, y->name);
}

bool bt_refresh(BtState *st)
{
	st->available = false;
	st->ndev = 0;
	if (!bus && !bt_open())
		return false;
	sd_bus_message *reply = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	int r = sd_bus_call_method(bus, BLUEZ, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", &err,
	                           &reply, "");
	if (r < 0) {
		SDL_Log("bt: GetManagedObjects: %s", err.message ? err.message : strerror(-r));
		sd_bus_error_free(&err);
		return false;
	}
	r = sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}");
	while (r >= 0 && (r = sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}")) > 0) {
		const char *path;
		sd_bus_message_read(reply, "o", &path);
		BtDevice d = { 0 };
		bool is_dev = false, is_adapter = false;
		r = sd_bus_message_enter_container(reply, 'a', "{sa{sv}}");
		while (r >= 0 && (r = sd_bus_message_enter_container(reply, 'e', "sa{sv}")) > 0) {
			const char *iface;
			sd_bus_message_read(reply, "s", &iface);
			if (!strcmp(iface, "org.bluez.Device1") && !strncmp(path, ADAPTER "/", strlen(ADAPTER) + 1)) {
				is_dev = true;
				r = read_props(reply, st, &d, false);
			} else if (!strcmp(iface, "org.bluez.Adapter1") && !strcmp(path, ADAPTER)) {
				is_adapter = true;
				r = read_props(reply, st, &d, true);
			} else
				r = sd_bus_message_skip(reply, "a{sv}");
			if (r >= 0)
				r = sd_bus_message_exit_container(reply);
		}
		if (r >= 0)
			r = sd_bus_message_exit_container(reply); /* a{sa{sv}} */
		if (r >= 0)
			r = sd_bus_message_exit_container(reply); /* e */
		if (is_adapter)
			st->available = true;
		/* devices without a name that are not paired are just noise in a list */
		if (is_dev && (d.paired || d.has_name) && st->ndev < BT_MAX_DEVICES) {
			snprintf(d.path, sizeof d.path, "%s", path);
			if (!d.name[0])
				snprintf(d.name, sizeof d.name, "%s", d.addr);
			if (!d.has_rssi)
				d.rssi = -127;
			st->dev[st->ndev++] = d;
		}
	}
	sd_bus_message_unref(reply);
	qsort(st->dev, (size_t)st->ndev, sizeof st->dev[0], dev_order);
	for (int i = 0; i < st->ndev; i++)
		st->dev[i].busy = !strcmp(st->dev[i].path, busy) ? busy_op : BT_IDLE;
	return st->available;
}

/* ------------------------------------------------------------- operations */

static void set_bool(const char *path, const char *iface, const char *prop, bool v)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	int r = sd_bus_set_property(bus, BLUEZ, path, iface, prop, &err, "b", (int)v);
	if (r < 0)
		SDL_Log("bt: set %s %s: %s", path, prop, err.message ? err.message : strerror(-r));
	sd_bus_error_free(&err);
}

void bt_set_powered(bool on)
{
	if (bus || bt_open())
		set_bool(ADAPTER, "org.bluez.Adapter1", "Powered", on);
	changed = true;
}

void bt_scan(bool on)
{
	if (!bus && !bt_open())
		return;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	int r = sd_bus_call_method(bus, BLUEZ, ADAPTER, "org.bluez.Adapter1", on ? "StartDiscovery" : "StopDiscovery",
	                           &err, NULL, "");
	if (r < 0 && on)
		SDL_Log("bt: StartDiscovery: %s", err.message ? err.message : strerror(-r));
	sd_bus_error_free(&err);
	changed = true;
}

static const char *short_error(const sd_bus_error *e)
{
	const char *n = e && e->name ? e->name : "";
	const char *dot = strrchr(n, '.');
	return dot ? dot + 1 : n;
}

static char op_name[BT_NAME_MAX];

static void finish(const char *fmt, const char *arg)
{
	snprintf(message, sizeof message, fmt, op_name, arg);
	busy[0] = 0;
	busy_op = BT_IDLE;
	changed = true;
}

static int connect_done(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u; (void)e;
	const sd_bus_error *err = sd_bus_message_get_error(m);
	if (err) {
		SDL_Log("bt: Connect %s: %s", busy, err->message ? err->message : "");
		finish(T(T_BT_FAILED), short_error(err));
	} else
		finish(T(T_BT_CONNECTED_MSG), "");
	return 0;
}

static void start_connect(void)
{
	busy_op = BT_CONNECTING;
	int r = sd_bus_call_method_async(bus, NULL, BLUEZ, busy, "org.bluez.Device1", "Connect", connect_done, NULL, "");
	if (r < 0)
		finish(T(T_BT_FAILED), strerror(-r));
	changed = true;
}

static int pair_done(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u; (void)e;
	const sd_bus_error *err = sd_bus_message_get_error(m);
	if (err && !sd_bus_error_has_name(err, "org.bluez.Error.AlreadyExists")) {
		SDL_Log("bt: Pair %s: %s", busy, err->message ? err->message : "");
		finish(T(T_BT_FAILED), short_error(err));
		return 0;
	}
	/* trusted: reconnects by itself later when the device's button is pressed */
	set_bool(busy, "org.bluez.Device1", "Trusted", true);
	start_connect();
	return 0;
}

static bool begin(const BtDevice *d, BtBusy op)
{
	if (busy[0] || (!bus && !bt_open()))
		return false;
	snprintf(busy, sizeof busy, "%s", d->path);
	snprintf(op_name, sizeof op_name, "%s", d->name);
	busy_op = op;
	changed = true;
	return true;
}

void bt_activate(const BtDevice *d)
{
	if (d->connected) {
		if (!begin(d, BT_DISCONNECTING))
			return;
		sd_bus_error err = SD_BUS_ERROR_NULL;
		int r = sd_bus_call_method(bus, BLUEZ, d->path, "org.bluez.Device1", "Disconnect", &err, NULL, "");
		if (r < 0)
			finish(T(T_BT_FAILED), short_error(&err));
		else
			finish(T(T_BT_DISCONNECTED_MSG), "");
		sd_bus_error_free(&err);
	} else if (d->paired) {
		if (begin(d, BT_CONNECTING))
			start_connect();
	} else {
		if (!begin(d, BT_PAIRING))
			return;
		/* searching slows pairing down and is no longer needed */
		sd_bus_call_method(bus, BLUEZ, ADAPTER, "org.bluez.Adapter1", "StopDiscovery", NULL, NULL, "");
		int r = sd_bus_call_method_async(bus, NULL, BLUEZ, d->path, "org.bluez.Device1", "Pair", pair_done, NULL, "");
		if (r < 0)
			finish(T(T_BT_FAILED), strerror(-r));
	}
}

void bt_forget(const BtDevice *d)
{
	if (!bus && !bt_open())
		return;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	int r = sd_bus_call_method(bus, BLUEZ, ADAPTER, "org.bluez.Adapter1", "RemoveDevice", &err, NULL, "o", d->path);
	snprintf(op_name, sizeof op_name, "%s", d->name);
	snprintf(message, sizeof message, r < 0 ? T(T_BT_FAILED) : T(T_BT_FORGOTTEN_MSG), op_name, short_error(&err));
	sd_bus_error_free(&err);
	changed = true;
}
