/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#ifdef ENABLE_NLS
#include <libintl.h>
#undef _
#define _(String)            dgettext(GETTEXT_PACKAGE, String)
#define C_(CString, MString) dpgettext(GETTEXT_PACKAGE, CString, MString)
#define Q_(String)           g_strip_context((String), gettext(String))
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#else
#define textdomain(String)                (String)
#define gettext(String)                   (String)
#define dgettext(Domain, Message)         (Message)
#define bindtextdomain(Domain, Directory) (Domain)
#define _(String)                         (String)
#define Q_(String)                        g_strip_context((String), (String))
#define N_(String)                        (String)
#endif

G_END_DECLS
