#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }

/* border & background color */
static const float color_focused[] = COLOR(0x7aacacff);
static const float color_default[] = COLOR(0xffffff00);
static const float color_background[] = COLOR(0x262626ff);

/* launcher */
static char terminal[] = "foot";
static char launcher[] = "wofi";

/* background color */

/* Trackpad */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 1;
static const int natural_scrolling = 0;
static const int disable_while_typing = 1;
static const int left_handed = 0;
static const int middle_button_emulation = 0;

static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;

static const double accel_speed = 0.0;

/*  gaps for maxvertleft & maxvertright */
static const unsigned int gapsize = 6;

/* exclusive zone*/
static const unsigned int exczone = 24;

typedef struct {
	const char *app_id;
	int x;
	int y;
} Rule;

static const Rule rules[] = {
	/* app_id, 		x,	y */

	{ "mpv", 		50, 	50 },
	{ "xfce4-appfinder", 	5, 	20 },
	{ "termbar", 	7, 	7 },
	{ "lxqt-panel", 	4, 	4 },

};
