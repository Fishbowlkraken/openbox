#include "openbox.h"
#include "dock.h"
#include "xerror.h"
#include "prop.h"
#include "startup.h"
#include "timer.h"
#include "config.h"
#include "screen.h"
#include "client.h"
#include "frame.h"
#include "focus.h"
#include "dispatch.h"
#include "extensions.h"
#include "render/render.h"

#ifdef USE_LIBSN
#  define SN_API_NOT_YET_FROZEN
#  include <libsn/sn.h>
#endif

#include <X11/Xlib.h>
#ifdef HAVE_UNISTD_H
#  include <sys/types.h>
#  include <unistd.h>
#endif

/*! The event mask to grab on the root window */
#define ROOT_EVENTMASK (StructureNotifyMask | PropertyChangeMask | \
			EnterWindowMask | LeaveWindowMask | \
			SubstructureNotifyMask | SubstructureRedirectMask | \
			ButtonPressMask | ButtonReleaseMask | ButtonMotionMask)

guint    screen_num_desktops    = 0;
guint    screen_num_xin_areas   = 0;
guint    screen_desktop         = 0;
Size     screen_physical_size;
gboolean screen_showing_desktop;
DesktopLayout screen_desktop_layout;
char   **screen_desktop_names = NULL;

static Rect  **area = NULL; /* array of desktop holding array of
                               xinerama areas */
static Rect  *xin_areas = NULL;
static Window support_window = None;

#ifdef USE_LIBSN
static SnMonitorContext *sn_context;
static int sn_busy_cnt;
static Timer *sn_timer = NULL;

static void sn_event_func(SnMonitorEvent *event, void *data);
#endif

static void set_root_cursor();

static gboolean running;
static int another_running(Display *d, XErrorEvent *e)
{
    (void)d;(void)e;
    g_message("A window manager is already running on screen %d",
	      ob_screen);
    running = TRUE;
    return -1;
}

gboolean screen_annex()
{
    XErrorHandler old;
    pid_t pid;
    int i, num_support;
    guint32 *supported;

    running = FALSE;
    old = XSetErrorHandler(another_running);
    XSelectInput(ob_display, ob_root, ROOT_EVENTMASK);
    XSync(ob_display, FALSE);
    XSetErrorHandler(old);
    if (running)
	return FALSE;

    g_message("Managing screen %d", ob_screen);

    set_root_cursor();

    /* set the OPENBOX_PID hint */
    pid = getpid();
    PROP_SET32(ob_root, openbox_pid, cardinal, pid);

    /* create the netwm support window */
    support_window = XCreateSimpleWindow(ob_display, ob_root, 0,0,1,1,0,0,0);

    /* set supporting window */
    PROP_SET32(ob_root, net_supporting_wm_check, window, support_window);

    /* set properties on the supporting window */
    PROP_SETS(support_window, net_wm_name, "Openbox");
    PROP_SET32(support_window, net_supporting_wm_check, window,support_window);

    /* set the _NET_SUPPORTED_ATOMS hint */
    num_support = 61;
    i = 0;
    supported = g_new(guint32, num_support);
    supported[i++] = prop_atoms.net_current_desktop;
    supported[i++] = prop_atoms.net_number_of_desktops;
    supported[i++] = prop_atoms.net_desktop_geometry;
    supported[i++] = prop_atoms.net_desktop_viewport;
    supported[i++] = prop_atoms.net_active_window;
    supported[i++] = prop_atoms.net_workarea;
    supported[i++] = prop_atoms.net_client_list;
    supported[i++] = prop_atoms.net_client_list_stacking;
    supported[i++] = prop_atoms.net_desktop_names;
    supported[i++] = prop_atoms.net_close_window;
    supported[i++] = prop_atoms.net_desktop_layout;
    supported[i++] = prop_atoms.net_showing_desktop;
    supported[i++] = prop_atoms.net_wm_name;
    supported[i++] = prop_atoms.net_wm_visible_name;
    supported[i++] = prop_atoms.net_wm_icon_name;
    supported[i++] = prop_atoms.net_wm_visible_icon_name;
    supported[i++] = prop_atoms.net_wm_desktop;
    supported[i++] = prop_atoms.net_wm_strut;
    supported[i++] = prop_atoms.net_wm_window_type;
    supported[i++] = prop_atoms.net_wm_window_type_desktop;
    supported[i++] = prop_atoms.net_wm_window_type_dock;
    supported[i++] = prop_atoms.net_wm_window_type_toolbar;
    supported[i++] = prop_atoms.net_wm_window_type_menu;
    supported[i++] = prop_atoms.net_wm_window_type_utility;
    supported[i++] = prop_atoms.net_wm_window_type_splash;
    supported[i++] = prop_atoms.net_wm_window_type_dialog;
    supported[i++] = prop_atoms.net_wm_window_type_normal;
    supported[i++] = prop_atoms.net_wm_allowed_actions;
    supported[i++] = prop_atoms.net_wm_action_move;
    supported[i++] = prop_atoms.net_wm_action_resize;
    supported[i++] = prop_atoms.net_wm_action_minimize;
    supported[i++] = prop_atoms.net_wm_action_shade;
    supported[i++] = prop_atoms.net_wm_action_maximize_horz;
    supported[i++] = prop_atoms.net_wm_action_maximize_vert;
    supported[i++] = prop_atoms.net_wm_action_fullscreen;
    supported[i++] = prop_atoms.net_wm_action_change_desktop;
    supported[i++] = prop_atoms.net_wm_action_close;
    supported[i++] = prop_atoms.net_wm_state;
    supported[i++] = prop_atoms.net_wm_state_modal;
    supported[i++] = prop_atoms.net_wm_state_maximized_vert;
    supported[i++] = prop_atoms.net_wm_state_maximized_horz;
    supported[i++] = prop_atoms.net_wm_state_shaded;
    supported[i++] = prop_atoms.net_wm_state_skip_taskbar;
    supported[i++] = prop_atoms.net_wm_state_skip_pager;
    supported[i++] = prop_atoms.net_wm_state_hidden;
    supported[i++] = prop_atoms.net_wm_state_fullscreen;
    supported[i++] = prop_atoms.net_wm_state_above;
    supported[i++] = prop_atoms.net_wm_state_below;
    supported[i++] = prop_atoms.net_moveresize_window;
    supported[i++] = prop_atoms.net_wm_moveresize;
    supported[i++] = prop_atoms.net_wm_moveresize_size_topleft;
    supported[i++] = prop_atoms.net_wm_moveresize_size_top;
    supported[i++] = prop_atoms.net_wm_moveresize_size_topright;
    supported[i++] = prop_atoms.net_wm_moveresize_size_right;
    supported[i++] = prop_atoms.net_wm_moveresize_size_bottomright;
    supported[i++] = prop_atoms.net_wm_moveresize_size_bottom;
    supported[i++] = prop_atoms.net_wm_moveresize_size_bottomleft;
    supported[i++] = prop_atoms.net_wm_moveresize_size_left;
    supported[i++] = prop_atoms.net_wm_moveresize_move;
    supported[i++] = prop_atoms.net_wm_moveresize_size_keyboard;
    supported[i++] = prop_atoms.net_wm_moveresize_move_keyboard;
    g_assert(i == num_support);
/*
  supported[] = prop_atoms.net_wm_action_stick;
*/

    PROP_SETA32(ob_root, net_supported, atom, supported, num_support);
    g_free(supported);

    return TRUE;
}

void screen_startup()
{
    GSList *it;
    guint i;

    /* get the initial size */
    screen_resize(WidthOfScreen(ScreenOfDisplay(ob_display, ob_screen)),
                  HeightOfScreen(ScreenOfDisplay(ob_display, ob_screen)));

    /* set the names */
    screen_desktop_names = g_new(char*,
                                 g_slist_length(config_desktops_names) + 1);
    for (i = 0, it = config_desktops_names; it; ++i, it = it->next)
        screen_desktop_names[i] = it->data; /* dont strdup */
    screen_desktop_names[i] = NULL;
    PROP_SETSS(ob_root, net_desktop_names, screen_desktop_names);
    g_free(screen_desktop_names); /* dont free the individual strings */
    screen_desktop_names = NULL;

    screen_num_desktops = 0;
    screen_set_num_desktops(config_desktops_num);
    if (startup_desktop >= screen_num_desktops)
        startup_desktop = 0;
    screen_desktop = startup_desktop;
    screen_set_desktop(startup_desktop);

    /* don't start in showing-desktop mode */
    screen_showing_desktop = FALSE;
    PROP_SET32(ob_root, net_showing_desktop, cardinal, screen_showing_desktop);

    screen_update_layout();

#ifdef USE_LIBSN
    sn_context = sn_monitor_context_new(ob_sn_display, ob_screen,
                                        sn_event_func, NULL, NULL);
    sn_busy_cnt = 0;
#endif
}

void screen_shutdown()
{
    Rect **r;

    XSelectInput(ob_display, ob_root, NoEventMask);

    PROP_ERASE(ob_root, openbox_pid); /* we're not running here no more! */
    PROP_ERASE(ob_root, net_supported); /* not without us */
    PROP_ERASE(ob_root, net_showing_desktop); /* don't keep this mode */

    XDestroyWindow(ob_display, support_window);

    g_strfreev(screen_desktop_names);
    for (r = area; *r; ++r)
        g_free(*r);
    g_free(area);
}

void screen_resize(int w, int h)
{
    GList *it;
    guint32 geometry[2];

    /* Set the _NET_DESKTOP_GEOMETRY hint */
    screen_physical_size.width = geometry[0] = w;
    screen_physical_size.height = geometry[1] = h;
    PROP_SETA32(ob_root, net_desktop_geometry, cardinal, geometry, 2);

    if (ob_state == State_Starting)
	return;

    dock_configure();
    screen_update_areas();

    for (it = client_list; it; it = it->next)
        client_move_onscreen(it->data);
}

void screen_set_num_desktops(guint num)
{
    guint i, old;
    guint32 *viewport;
    GList *it;

    g_assert(num > 0);

    old = screen_num_desktops;
    screen_num_desktops = num;
    PROP_SET32(ob_root, net_number_of_desktops, cardinal, num);

    /* set the viewport hint */
    viewport = g_new0(guint32, num * 2);
    PROP_SETA32(ob_root, net_desktop_viewport, cardinal, viewport, num * 2);
    g_free(viewport);

    /* the number of rows/columns will differ */
    screen_update_layout();

    /* may be some unnamed desktops that we need to fill in with names */
    screen_update_desktop_names();

    /* update the focus lists */
    /* free our lists for the desktops which have disappeared */
    for (i = num; i < old; ++i)
        g_list_free(focus_order[i]);
    /* realloc the array */
    focus_order = g_renew(GList*, focus_order, num);
    /* set the new lists to be empty */
    for (i = old; i < num; ++i)
        focus_order[i] = NULL;

    /* move windows on desktops that will no longer exist! */
    for (it = client_list; it != NULL; it = it->next) {
        Client *c = it->data;
        if (c->desktop >= num && c->desktop != DESKTOP_ALL)
            client_set_desktop(c, num - 1, FALSE);
    }

    /* change our struts/area to match (after moving windows) */
    screen_update_areas();

    dispatch_ob(Event_Ob_NumDesktops, num, old);

    /* change our desktop if we're on one that no longer exists! */
    if (screen_desktop >= screen_num_desktops)
	screen_set_desktop(num - 1);
}

void screen_set_desktop(guint num)
{
    GList *it;
    guint old;
    XEvent e;
     
    g_assert(num < screen_num_desktops);

    old = screen_desktop;
    screen_desktop = num;
    PROP_SET32(ob_root, net_current_desktop, cardinal, num);

    if (old == num) return;

    g_message("Moving to desktop %d", num+1);

    /* show windows before hiding the rest to lessen the enter/leave events */

    /* show windows from top to bottom */
    for (it = stacking_list; it != NULL; it = it->next) {
        if (WINDOW_IS_CLIENT(it->data)) {
            Client *c = it->data;
            if (!c->frame->visible && client_should_show(c))
                frame_show(c->frame);
        }
    }

    /* hide windows from bottom to top */
    for (it = g_list_last(stacking_list); it != NULL; it = it->prev) {
        if (WINDOW_IS_CLIENT(it->data)) {
            Client *c = it->data;
            if (c->frame->visible && !client_should_show(c))
                frame_hide(c->frame);
        }
    }

    /* focus the last focused window on the desktop, and ignore enter events
       from the switch so it doesnt mess with the focus */
    while (XCheckTypedEvent(ob_display, EnterNotify, &e));
    g_message("switch fallback");
    focus_fallback(Fallback_Desktop);
    g_message("/switch fallback");

    dispatch_ob(Event_Ob_Desktop, num, old);
}

void screen_update_layout()
{
    guint32 *data = NULL;
    guint num;

    /* defaults */
    screen_desktop_layout.orientation = prop_atoms.net_wm_orientation_horz;
    screen_desktop_layout.start_corner = prop_atoms.net_wm_topleft;
    screen_desktop_layout.rows = 1;
    screen_desktop_layout.columns = screen_num_desktops;

    if (PROP_GETA32(ob_root, net_desktop_layout, cardinal, &data, &num)) {
        if (num == 3 || num == 4) {
            if (data[0] == prop_atoms.net_wm_orientation_vert)
                screen_desktop_layout.orientation = data[0];
            if (num == 3)
                screen_desktop_layout.start_corner =
                    prop_atoms.net_wm_topright;
            else {
                if (data[3] == prop_atoms.net_wm_topright)
                    screen_desktop_layout.start_corner = data[3];
                else if (data[3] == prop_atoms.net_wm_bottomright)
                    screen_desktop_layout.start_corner = data[3];
                else if (data[3] == prop_atoms.net_wm_bottomleft)
                    screen_desktop_layout.start_corner = data[3];
            }

            /* fill in a zero rows/columns */
            if (!(data[1] == 0 && data[2] == 0)) { /* both 0's is bad data.. */
                if (data[1] == 0) {
                    data[1] = (screen_num_desktops +
                               screen_num_desktops % data[2]) / data[2];
                } else if (data[2] == 0) {
                    data[2] = (screen_num_desktops +
                               screen_num_desktops % data[1]) / data[1];
                }
                screen_desktop_layout.columns = data[1];
                screen_desktop_layout.rows = data[2];
            }

            /* bounds checking */
            if (screen_desktop_layout.orientation ==
                prop_atoms.net_wm_orientation_horz) {
                if (screen_desktop_layout.rows > screen_num_desktops)
                    screen_desktop_layout.rows = screen_num_desktops;
                if (screen_desktop_layout.columns >
                    ((screen_num_desktops + screen_num_desktops %
                      screen_desktop_layout.rows) /
                     screen_desktop_layout.rows))
                    screen_desktop_layout.columns =
                        (screen_num_desktops + screen_num_desktops %
                         screen_desktop_layout.rows) /
                        screen_desktop_layout.rows;
            } else {
                if (screen_desktop_layout.columns > screen_num_desktops)
                    screen_desktop_layout.columns = screen_num_desktops;
                if (screen_desktop_layout.rows >
                    ((screen_num_desktops + screen_num_desktops %
                      screen_desktop_layout.columns) /
                     screen_desktop_layout.columns))
                    screen_desktop_layout.rows =
                        (screen_num_desktops + screen_num_desktops %
                         screen_desktop_layout.columns) /
                        screen_desktop_layout.columns;
            }
        }
	g_free(data);
    }
}

void screen_update_desktop_names()
{
    guint i;

    /* empty the array */
    g_strfreev(screen_desktop_names);
    screen_desktop_names = NULL;

    if (PROP_GETSS(ob_root, net_desktop_names, utf8, &screen_desktop_names))
        for (i = 0; screen_desktop_names[i] && i <= screen_num_desktops; ++i);
    else
        i = 0;
    if (i <= screen_num_desktops) {
        screen_desktop_names = g_renew(char*, screen_desktop_names,
                                       screen_num_desktops + 1);
        screen_desktop_names[screen_num_desktops] = NULL;
        for (; i < screen_num_desktops; ++i)
            screen_desktop_names[i] = g_strdup("Unnamed Desktop");
    }
}

void screen_show_desktop(gboolean show)
{
    GList *it;
     
    if (show == screen_showing_desktop) return; /* no change */

    screen_showing_desktop = show;

    if (show) {
	/* bottom to top */
	for (it = g_list_last(stacking_list); it != NULL; it = it->prev) {
            if (WINDOW_IS_CLIENT(it->data)) {
                Client *client = it->data;
                if (client->frame->visible && !client_should_show(client))
                    frame_hide(client->frame);
            }
	}
    } else {
        /* top to bottom */
	for (it = stacking_list; it != NULL; it = it->next) {
            if (WINDOW_IS_CLIENT(it->data)) {
                Client *client = it->data;
                if (!client->frame->visible && client_should_show(client))
                    frame_show(client->frame);
            }
	}
    }

    if (show) {
        /* focus desktop */
        for (it = focus_order[screen_desktop]; it; it = it->next)
            if (((Client*)it->data)->type == Type_Desktop &&
                client_focus(it->data))
                break;
    } else {
        focus_fallback(Fallback_NoFocus);
    }

    show = !!show; /* make it boolean */
    PROP_SET32(ob_root, net_showing_desktop, cardinal, show);

    dispatch_ob(Event_Ob_ShowDesktop, show, 0);
}

void screen_install_colormap(Client *client, gboolean install)
{
    XWindowAttributes wa;

    if (client == NULL) {
	if (install)
	    XInstallColormap(RrDisplay(ob_rr_inst), RrColormap(ob_rr_inst));
	else
	    XUninstallColormap(RrDisplay(ob_rr_inst), RrColormap(ob_rr_inst));
    } else {
	if (XGetWindowAttributes(ob_display, client->window, &wa) &&
            wa.colormap != None) {
            xerror_set_ignore(TRUE);
	    if (install)
		XInstallColormap(RrDisplay(ob_rr_inst), wa.colormap);
	    else
		XUninstallColormap(RrDisplay(ob_rr_inst), wa.colormap);
            xerror_set_ignore(FALSE);
	}
    }
}

void screen_update_areas()
{
    guint i, x;
    guint32 *dims;
    Rect **old_area = area;
    Rect **rit;
    GList *it;

    g_free(xin_areas);
    extensions_xinerama_screens(&xin_areas, &screen_num_xin_areas);

    if (area) {
        for (i = 0; area[i]; ++i)
            g_free(area[i]);
        g_free(area);
    }

    area = g_new(Rect*, screen_num_desktops + 2);
    for (i = 0; i < screen_num_desktops + 1; ++i)
        area[i] = g_new(Rect, screen_num_xin_areas + 1);
    area[i] = NULL;
     
    dims = g_new(guint32, 4 * screen_num_desktops);

    rit = old_area;
    for (i = 0; i < screen_num_desktops + 1; ++i) {
        Strut s;
        int l, r, t, b;

        /* calc the xinerama areas */
        for (x = 0; x < screen_num_xin_areas; ++x) {
            area[i][x] = xin_areas[x];
            if (x == 0) {
                l = xin_areas[x].x;
                t = xin_areas[x].y;
                r = xin_areas[x].x + xin_areas[x].width - 1;
                b = xin_areas[x].y + xin_areas[x].height - 1;
            } else {
                l = MIN(l, xin_areas[x].x);
                t = MIN(t, xin_areas[x].y);
                r = MAX(r, xin_areas[x].x + xin_areas[x].width - 1);
                b = MAX(b, xin_areas[x].y + xin_areas[x].height - 1);
            }
        }
        RECT_SET(area[i][x], l, t, r - l + 1, b - t + 1);

        /* apply struts */
        STRUT_SET(s, 0, 0, 0, 0);
        for (it = client_list; it; it = it->next)
            STRUT_ADD(s, ((Client*)it->data)->strut);
        STRUT_ADD(s, dock_strut);

        if (s.left) {
            int o;

            /* find the left-most xin heads, i do this in 2 loops :| */
            o = area[i][0].x;
            for (x = 1; x < screen_num_xin_areas; ++x)
                o = MIN(o, area[i][x].x);

            for (x = 0; x < screen_num_xin_areas; ++x) {
                int edge = o + s.left - area[i][x].x;
                if (edge > 0) {
                    area[i][x].x += edge;
                    area[i][x].width -= edge;
                }
            }

            area[i][screen_num_xin_areas].x += s.left;
            area[i][screen_num_xin_areas].width -= s.left;
        }
        if (s.top) {
            int o;

            /* find the left-most xin heads, i do this in 2 loops :| */
            o = area[i][0].y;
            for (x = 1; x < screen_num_xin_areas; ++x)
                o = MIN(o, area[i][x].y);

            for (x = 0; x < screen_num_xin_areas; ++x) {
                int edge = o + s.top - area[i][x].y;
                if (edge > 0) {
                    area[i][x].y += edge;
                    area[i][x].height -= edge;
                }
            }

            area[i][screen_num_xin_areas].y += s.top;
            area[i][screen_num_xin_areas].height -= s.top;
        }
        if (s.right) {
            int o;

            /* find the bottom-most xin heads, i do this in 2 loops :| */
            o = area[i][0].x + area[i][0].width - 1;
            for (x = 1; x < screen_num_xin_areas; ++x)
                o = MAX(o, area[i][x].x + area[i][x].width - 1);

            for (x = 0; x < screen_num_xin_areas; ++x) {
                int edge = (area[i][x].x + area[i][x].width - 1) -
                    (o - s.right);
                if (edge > 0)
                    area[i][x].width -= edge;
            }

            area[i][screen_num_xin_areas].width -= s.right;
        }
        if (s.bottom) {
            int o;

            /* find the bottom-most xin heads, i do this in 2 loops :| */
            o = area[i][0].y + area[i][0].height - 1;
            for (x = 1; x < screen_num_xin_areas; ++x)
                o = MAX(o, area[i][x].y + area[i][x].height - 1);

            for (x = 0; x < screen_num_xin_areas; ++x) {
                int edge = (area[i][x].y + area[i][x].height - 1) -
                    (o - s.bottom);
                if (edge > 0)
                    area[i][x].height -= edge;
            }

            area[i][screen_num_xin_areas].height -= s.bottom;
        }

        /* XXX when dealing with partial struts, if its in a single
           xinerama area, then only subtract it from that area's space
        for (x = 0; x < screen_num_xin_areas; ++x) {
	    GList *it;


               do something smart with it for the 'all xinerama areas' one...

	    for (it = client_list; it; it = it->next) {

                XXX if gunna test this shit, then gotta worry about when
                the client moves between xinerama heads..

                if (RECT_CONTAINS_RECT(((Client*)it->data)->frame->area,
                                       area[i][x])) {

                }            
            }
        }
        */

        /* XXX optimize when this is run? */

        /* the area has changed, adjust all the maximized 
           windows */
        for (it = client_list; it; it = it->next) {
            Client *c = it->data; 
            if (i < screen_num_desktops) {
                if (c->desktop == i)
                    client_reconfigure(c);
            } else if (c->desktop == DESKTOP_ALL)
                client_reconfigure(c);
        }
        if (i < screen_num_desktops) {
            /* don't set these for the 'all desktops' area */
            dims[(i * 4) + 0] = area[i][screen_num_xin_areas].x;
            dims[(i * 4) + 1] = area[i][screen_num_xin_areas].y;
            dims[(i * 4) + 2] = area[i][screen_num_xin_areas].width;
            dims[(i * 4) + 3] = area[i][screen_num_xin_areas].height;
        }
    }
    PROP_SETA32(ob_root, net_workarea, cardinal,
		dims, 4 * screen_num_desktops);

    g_free(dims);
}

Rect *screen_area(guint desktop)
{
    return screen_area_xinerama(desktop, screen_num_xin_areas);
}

Rect *screen_area_xinerama(guint desktop, guint head)
{
    if (head > screen_num_xin_areas)
        return NULL;
    if (desktop >= screen_num_desktops) {
	if (desktop == DESKTOP_ALL)
	    return &area[screen_num_desktops][head];
	return NULL;
    }
    return &area[desktop][head];
}

Rect *screen_physical_area()
{
    return screen_physical_area_xinerama(screen_num_xin_areas);
}

Rect *screen_physical_area_xinerama(guint head)
{
    if (head > screen_num_xin_areas)
        return NULL;
    return &xin_areas[head];
}

static void set_root_cursor()
{
#ifdef USE_LIBSN
        if (sn_busy_cnt)
            XDefineCursor(ob_display, ob_root, ob_cursors.busy);
        else
#endif
            XDefineCursor(ob_display, ob_root, ob_cursors.ptr);
}

#ifdef USE_LIBSN
static void sn_timeout(void *data)
{
    timer_stop(sn_timer);
    sn_timer = NULL;
    sn_busy_cnt = 0;

    set_root_cursor();
}

static void sn_event_func(SnMonitorEvent *ev, void *data)
{
    SnStartupSequence *seq;
    const char *seq_id, *bin_name;
    int cnt = sn_busy_cnt;

    if (!(seq = sn_monitor_event_get_startup_sequence(ev)))
        return;

    seq_id = sn_startup_sequence_get_id(seq);
    bin_name = sn_startup_sequence_get_binary_name(seq);
    
    if (!(seq_id && bin_name))
        return;

    switch (sn_monitor_event_get_type(ev)) {
    case SN_MONITOR_EVENT_INITIATED:
        ++sn_busy_cnt;
        if (sn_timer)
            timer_stop(sn_timer);
        /* 30 second timeout for apps to start */
        sn_timer = timer_start(30 * 1000000, sn_timeout, NULL);
        break;
    case SN_MONITOR_EVENT_CHANGED:
        break;
    case SN_MONITOR_EVENT_COMPLETED:
        if (sn_busy_cnt) --sn_busy_cnt;
        if (sn_timer) {
            timer_stop(sn_timer);
            sn_timer = NULL;
        }
        break;
    case SN_MONITOR_EVENT_CANCELED:
        if (sn_busy_cnt) --sn_busy_cnt;
        if (sn_timer) {
            timer_stop(sn_timer);
            sn_timer = NULL;
        }
    };

    if (sn_busy_cnt != cnt)
        set_root_cursor();
}
#endif
