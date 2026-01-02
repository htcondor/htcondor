/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_debug.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <errno.h>
#include <inttypes.h>

#include <dbus/dbus.h>

#if !defined(DBUS_INT64_MODIFIER)
// An educated guess
#define DBUS_INT64_MODIFIER "l"
#endif

#include "MutterInterface.unix.h"
#include "condor_config.h"


namespace stdfs = std::filesystem;

static
bool
MutterSessionCompare(const MutterSession &a,
                     const MutterSession &b)
{
  return a.idle_time < b.idle_time;
}

static
uint64_t
GetIdleTime(struct MutterSession &session)
{
  DBusConnection *connection = NULL;
  DBusError error;
  DBusMessage *message = NULL;
  DBusMessage *reply = NULL;
  DBusMessageIter iter;
  dbus_uint64_t val = 0;

  const char *dest = "org.gnome.Mutter.IdleMonitor";
  const char *path = "/org/gnome/Mutter/IdleMonitor/Core";
  const char *iface = "org.gnome.Mutter.IdleMonitor";
  const char *method = "GetIdletime";

  dbus_error_init (&error);

  /* Become the user in the session we're given */
  set_user_ids(session.owner, session.group);
  {
	  TemporaryPrivSentry sentry(PRIV_USER);

	  /* Try and send message on that user's DBUS Session bus */

	  connection = dbus_connection_open_private(session.sesion_bus.c_str(), &error);

	  if (connection == NULL) {
		  dprintf(D_ERROR,"Failed to open connection: %s\n", error.message);
		  dbus_error_free(&error);
		  goto cleanup;
	  }

	  if (!dbus_bus_register(connection, &error)) {
		  dprintf(D_ERROR, "Failed to register connection: %s\n", error.message);
		  dbus_error_free(&error);
		  goto cleanup;
	  }

	  message = dbus_message_new_method_call(dest, path, iface, method);

	  if (!message) {
		  dprintf(D_ERROR, "Failed to allocate message!");
		  dbus_error_free(&error);
		  goto cleanup;
	  }

	  /* Get the reply and extract the value */

	  dbus_error_init(&error);
	  reply = dbus_connection_send_with_reply_and_block(connection,
			  message, 1000,
			  &error);
	  if (dbus_error_is_set(&error)) {
		  dprintf(D_ERROR, "Error %s: %s\n",
				  error.name,
				  error.message);
		  dbus_error_free(&error);
		  goto cleanup;
	  }

	  dbus_message_iter_init(reply, &iter);
	  dbus_message_iter_get_basic(&iter, &val);

	  dprintf(D_FULLDEBUG, "UID: %u Idle: %" DBUS_INT64_MODIFIER "u\n", session.owner, val);

cleanup:
	  if (message) {
		  dbus_message_unref(message);
	  }

	  if (reply) {
		  dbus_message_unref(reply);
	  }

	  if (connection) {
		  dbus_connection_close(connection);
		  dbus_connection_unref(connection);
	  }

	  return val;
  }
}

static
void
UpdateSessionIdle(MutterSession &a)
{
  a.idle_time = GetIdleTime(a);
}

MutterInterface::MutterInterface()
{
  last_min_idle = UINT64_MAX;

  CheckActivity();
}

bool
MutterInterface::CheckActivity()
{
  bool activity = false;

  if (GetSessions()) {
    std::for_each(sessions.begin(), sessions.end(), UpdateSessionIdle);

    std::vector<MutterSession>::iterator min_session =
      std::min_element(sessions.begin(), sessions.end(),
                       MutterSessionCompare);

    if (min_session->idle_time <= last_min_idle) {
      activity = true;
    }

    last_min_idle = min_session->idle_time;
  }

  return activity;
}

bool
MutterInterface::GetSessions()
{
  bool found_sessions = false;
  const stdfs::path proc_d{"/proc"};

  sessions.clear();

  /* Time for the big guns, try as root */
  {
	  TemporaryPrivSentry sentry(PRIV_ROOT);

	  /* Iterate through the running processes to find one named
		 "gnome-session-binary". This is the process that let's us know
		 there's an open session (or graphical login screen!) */

	  for (auto const& dir_entry : stdfs::directory_iterator{proc_d}) {
		  if (std::atoi(dir_entry.path().filename().c_str()) > 1) {
			  stdfs::path exe{dir_entry.path() / "exe"};
			  std::error_code ec;

			  if (stdfs::read_symlink(exe, ec).filename() == "gnome-session-binary") {
				  std::string line;
				  stdfs::path env_p{dir_entry.path() / "environ"};
				  std::ifstream environ;
				  environ.open(env_p, std::ios::in);

				  /* Find the DBUS_SESSION_BUS_ADDRESS variable in the
					 environment, we need it to connect later */
				  while (std::getline(environ, line, '\0')) {
					  if (!line.find("DBUS_SESSION_BUS_ADDRESS=")) {
						  StatInfo s = StatInfo(dir_entry.path().c_str());

						  /* Remove "DBUS_BUS_SESSION=" from string, we just want
							 the address portion */
						  line.erase(0, line.find_first_of('=') + 1);

						  sessions.emplace_back(0, s.GetOwner(), s.GetGroup(), line);
						  found_sessions = true;

						  dprintf(D_FULLDEBUG,
								  "Found session for PID %s, UID %u GID %u (%s)\n",
								  dir_entry.path().filename().c_str(),
								  s.GetOwner(), s.GetGroup(), line.c_str()
								 );
					  }
				  }

				  environ.close();
			  }
		  }
	  }

	  set_condor_priv();

	  return found_sessions;
  }
}
