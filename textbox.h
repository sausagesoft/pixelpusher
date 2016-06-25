#include <stdbool.h>

void txbstring(char *);
void txbrgb(COLORREF);
void txblong(long);
void txblongs(long, long);
void txbul(unsigned long);
void txbflt(float);
void txbshtflt(float);
void txblngflt(float);
void txbbin8(unsigned char);
void txbbin32(unsigned long);
void txbhex(unsigned char);
void txbmatrix(float *);
void txbnl(void);
bool txb_make_text_box(int, int, int, int, char *, DWORD, HWND, HICON, HBRUSH, HFONT);
void txb_resize(void);

void txb_delete_text_box(void);

extern void (*txb_char_function)(short);

/*
1)	OPTIONAL:
	if you want to accept keypresses while the mouse is over the textbox:
	You can set the function pointer txb_char_function, which should
	point to a function that has one short (wParam) as its parameter;
	or, if the textbox is a child window and that pointer is NULL, the
	keypresses will go to its parent window.

2) Call txb_make_text_box with left, top, width, height, name, style, parent,
	icon, background brush, font.
	Examples:

	txb_make_text_box(750, 20, 200, 200, "Whoopsie", WS_OVERLAPPEDWINDOW, \
		NULL, LoadIcon((HINSTANCE) NULL, IDI_ASTERISK), whiteb, somefont);

	txb_make_text_box(750, 20, 200, 200, NULL, WS_CHILD | WS_BORDER, \
		hwndmain, NULL, NULL, NULL);

	It gets given a default name if you pass NULL.
	Icon, background brush amd font are likewise optional.
	(Icon would be the default application icon,
	Background defaults to system button face colour).

Note that the textbox shortens itself to a whole multiple of the line height, which
varies depending on the font. (No point having partially-visible lines, I figured.)

3)
	To make the mousewheel work: 
		In main:
		#define _WIN32_WINNT 0x0500
		short mousewheelaccumulator = 0; (make external in main.h)

4) If the textbox is a child window, it will grab focus on the mouse moving over it.
	So if the parent window wants to get mousewheel or keyboard events, it has to
	grab focus back again, likewise. That means some window proc stuff:

		case WM_MOUSEMOVE:
			if(got_focus)
				return 0;
			SetFocus(this_window);
			got_focus = true;
			return 0;

		case WM_KILLFOCUS:
			got_focus = false;
			return 0;

	Quite painless to set up, I think?
	You also have to create this:

		static bool got_focus = false;

	 of course.

5) You might want to put this in the clean-up function:

	txb_delete_text_box();

	Though it doesn't do much, just destroys the right-click menu.
	Dunno if those leak memory or not, probably not anyway.
*/
