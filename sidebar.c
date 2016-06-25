#include <windows.h>
#include <stdbool.h>
#include <commctrl.h>
#include "main.h"
#include "main_window.h"
#include "brushes_etc.h"
#include "drawing_window.h"
#include "textbox.h"
#include "sidebar.h"

#define NO_DRAG	0
#define PICKER_DRAG	1
#define SPECTRUM_DRAG	2

HWND sidebar_window_h = NULL;
static int drag_type = NO_DRAG;
unsigned long fore_pick_RGB = 0x00000000;
unsigned long back_pick_RGB = 0x00FFFFFF;
static unsigned long *current_pick_RGB;
static unsigned long picked_spec_RGB = 0x00FF0000;
static HBITMAP c_picker, defaultbmp;
static HDC picker_dc;
static unsigned long *picker_bits;
static PBITMAPINFO picker_pbmi;
static RECT picker_rect, fore_text_rect, back_text_rect, fore_pick_rect, back_pick_rect,
	fore_union_rect, back_union_rect;
static int spec_top, picker_width, marker_size;
static RECT picked_pos;

static int margin = 0, picker_width = 300; // Sidebar metrics, arbitrary init values.

static HWND edit_fore_h = NULL, edit_back_h = NULL;
WNDPROC orig_edit_proc; // for subclassing the edit control
bool user_edit = false; // slightly silly way to distinguish user text entry from me setting the text myself

static LRESULT CALLBACK sidebar_proc(HWND, UINT, WPARAM, LPARAM);
static void click(short, short);
static void pick(short, short);
static void spec_pick(short);
static void paint_sidebar_window(void);
static unsigned long x_to_hue(float);
static void update_picker(void);
static unsigned long xy_to_colour(float, float);
static int edit_input(int);
static LRESULT CALLBACK edit_proc(HWND, UINT, WPARAM, LPARAM);
static void set_hex(unsigned long);
static void set_colour_from_hex(void);

bool init_sidebar_window(void)
{
	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)sidebar_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon((HINSTANCE) NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor((HINSTANCE) NULL, IDC_ARROW);
	wc.hbrBackground = bgdb;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "sidebar_wc";
	wc.hIconSm = NULL;

	if (!RegisterClassEx(&wc))
			return false;
	return true;
}

int make_sidebar_window(RECT client_r)
{
	// using 1/2 window height as width for sidebar
	int height, opposite_width, middle, bh;
	int i, x; // put colours in the spectrum bmp
	unsigned long hue; // more of that
	float xf; // more
	int below;	// This is, I don't know, a position to draw stuff below the picker.
	SIZE string_dims; // This is for measuring text.
	HDC hdc;

	// picker_width and margin are stored permanently in static (i.e. file-local) globals.
	// (must remember to update them on resize, when I write that bit.)

	height = (client_r.bottom - client_r.top);
	picker_width = height / 2;
	opposite_width = (client_r.right - client_r.left) - picker_width;

	sidebar_window_h = CreateWindowEx(0, "sidebar_wc", "Tools", WS_CHILD | WS_VISIBLE,
		client_r.right - picker_width, client_r.top, picker_width, height,
		hwndmain, (HMENU)42, hInstance, NULL);
	// 42 = ID tag for this window, sent with WM_PARENTNOTIFY when this window is destroyed

	if(!sidebar_window_h)
			return false;

	middle = height / 2;
	margin = picker_width / 16;
	bh = height / 16;
	bh = bh * 3 / 4;

	picker_width -= 2 * margin;
	marker_size = margin / 2;

	// I probably shouldn't be positioning things vertically based on the horizontal margin.
	// Need a height-based unit to use instead.

	// The other thing to think about is enforcing a minimum sidebar width at small window sizes.

	//hwndButton =
	CreateWindow("BUTTON", "Quit", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		margin, middle + 3 * margin + bh * 3, picker_width, bh, sidebar_window_h, (HMENU)3, hInstance, NULL);

	picker_rect.left = margin;
	picker_rect.right = picker_rect.left + picker_width;
	picker_rect.top = margin;
	picker_rect.bottom = picker_rect.top + picker_width + margin;

	spec_top = picker_rect.bottom - margin;
	picker_width = picker_rect.right - picker_rect.left;

	picked_pos.left = picker_rect.left - marker_size;
	picked_pos.top = spec_top - marker_size;
	picked_pos.right = picker_rect.left + marker_size;
	picked_pos.bottom = spec_top + marker_size;

	// choose which colour pick thing is selected by default
	current_pick_RGB = &fore_pick_RGB;

	// figure out rects for the boxes that show the current fore and back colours.
	below = picker_rect.bottom + 30;
	hdc = GetDC(sidebar_window_h);
	GetTextExtentPoint32(hdc, "Front:", 6, &string_dims); // This assumes the other text ("Back:") is shorter.
	ReleaseDC(sidebar_window_h, hdc);
	// rect for "Front:" label
	SetRect(&fore_text_rect, margin + 2, below, margin + string_dims.cx + 2, below + string_dims.cy);
	// rect for foreground colour box
	below += string_dims.cy / 2;
	SetRect(&fore_pick_rect, fore_text_rect.right + 10, below - margin + 2,
		fore_text_rect.right + 10 + margin * 2, below + margin);
	// rect for "Back:" label
	SetRect(&back_text_rect, fore_text_rect.left, fore_text_rect.top + margin * 2, fore_text_rect.right,
		fore_text_rect.bottom + margin * 2);
	// rect for background colour box
	below += margin * 2;
	SetRect(&back_pick_rect, back_text_rect.right + 10, below - margin + 2,
		back_text_rect.right + 10 + margin * 2, below + margin);
	// rects for highlighting, the big ones you can click on to select fore or back
	UnionRect(&fore_union_rect, &fore_text_rect, &fore_pick_rect);
	fore_union_rect.left -= 2;
	InflateRect(&fore_pick_rect, -2, -2);
	UnionRect(&back_union_rect, &back_text_rect, &back_pick_rect);
	back_union_rect.left -= 2;
	InflateRect(&back_pick_rect, -2, -2);
	// move the labels in a bit
	OffsetRect(&fore_text_rect, 5, 0);
	OffsetRect(&back_text_rect, 5, 0);

	// Hex value boxes
	hdc = GetDC(sidebar_window_h);
	GetTextExtentPoint32(hdc, "CCCCCCi", 7, &string_dims);
	ReleaseDC(sidebar_window_h, hdc);

	edit_fore_h = CreateWindow("EDIT", "000000", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_UPPERCASE,
		fore_pick_rect.right + 10, fore_text_rect.top,
		string_dims.cx + 4, string_dims.cy + 4, sidebar_window_h, (HMENU)1, hInstance, NULL);

	orig_edit_proc = (WNDPROC)SetWindowLong(edit_fore_h, GWL_WNDPROC, (long)edit_proc);
	SendMessage(edit_fore_h, EM_SETLIMITTEXT, 6, 0); // limit text to 6 characters (bytes)

	edit_back_h = CreateWindow("EDIT", "FFFFFF", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_UPPERCASE,
		back_pick_rect.right + 10, back_text_rect.top,
		string_dims.cx + 4, string_dims.cy + 4, sidebar_window_h, (HMENU)2, hInstance, NULL);

	SetWindowLong(edit_back_h, GWL_WNDPROC, (long)edit_proc);
	SendMessage(edit_back_h, EM_SETLIMITTEXT, 6, 0); // limit text to 6 characters (bytes)

	// Zoom buttons

	CreateWindow("BUTTON", "+", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		margin, back_pick_rect.bottom + 4, bh, bh, sidebar_window_h, (HMENU)4, hInstance, NULL);
	CreateWindow("BUTTON", "-", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		margin + bh + 4, back_pick_rect.bottom + 4, bh, bh, sidebar_window_h, (HMENU)5, hInstance, NULL);
	CreateWindow("BUTTON", "Fit to window", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		margin + bh * 2 + 8, back_pick_rect.bottom + 4, bh * 6, bh, sidebar_window_h, (HMENU)6, hInstance, NULL);

	/*
	CreateWindow("BUTTON", "Begin Game", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		margin, middle + margin, width, bh * 2, sidebar_window_h, (HMENU)1, hInstance, NULL);
	CreateWindow("BUTTON", "Settings", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		margin, middle + 2 * margin + bh * 2, width, bh, sidebar_window_h, (HMENU)2, hInstance, NULL);
	*/

	// picker_width + margin is used as height of picker (extra margin is for spectrum strip at bottom)
	c_picker = DIB_maker(&picker_bits, &picker_pbmi, &picker_dc, picker_width, picker_width + margin);
	defaultbmp = SelectObject(picker_dc, c_picker);

	for(i = 0; i < picker_width * margin; i++){
		x = i % picker_width;
		xf = x / (float)picker_width;
		hue = x_to_hue(xf);
		*(picker_bits + i) = hue;
	}

	update_picker();
	
	return opposite_width; // so the main window can occupy the other part of the space
}

static void update_picker(void)
{
	int i, x, y;
	float xf, yf;

	// Update the picker to a new hue, in line with spectrum pick changes.

	for(i = picker_width * margin, y = 0; i < picker_width * (picker_width + margin); i++){
		x = i % picker_width;
		xf = (float)x / (float)picker_width;
		// xf should range from 0 at the left edge to 1 at the right edge.
		y = (i - picker_width * margin) / picker_width + 1;
		yf = ((float)y) / (float)picker_width; // using width here because height = width
		// yf should now be in the range 0 to 1.

		*(picker_bits + i) = xy_to_colour(xf, yf);
	}
}

static unsigned long xy_to_colour(float x, float y)
{
	// x and y are in the range 0 to 1. This returns a colour picked from the picker.
	float dist;
	unsigned long hue, colour, de_sat, r, g, b, *hi, *lo, *mid;
	
// First, reduce saturation, in the horizontal direction.
// Don't share out the values in channels; flatten the difference by adding to the lower two.
// i.e. keep the highest where it is, and call that the brightness. So 255 red is as bright as white.
// 1) You take the distance of each non-highest channel from the highest.
// 2) You multiply those two distances by the 0-to-1 x coordinate, which makes them shorter.
// 3) You subtract those distances from the highest channel to find the new values for the two lower ones.

	de_sat = picked_spec_RGB;
	r = (de_sat & 0x00FF0000) >> 16;
	g = (de_sat & 0x0000FF00) >> 8;
	b = (de_sat & 0x000000FF);
	hi = (r > g ? &r : &g);
	hi = (*hi > b ? hi : &b); // hi is the brightest channel
	lo = (r < g ? &r : &g);
	lo = (*lo < b ? lo : &b); // lo is a non-brightest channel (the darkest)
	if(hi == &r || lo == &r){
		if(hi == &b || lo == &b) // mid is the other one (the middle)
			mid = &g;
		else
			mid = &b;
	}else
		mid = &r;

	// shorten distances by increasing the two least bright channels
	dist = *hi - *mid;
	dist *= x;
	*mid = *hi - dist;
	dist = *hi - *lo;
	dist *= x;
	*lo = *hi - dist;

	de_sat = 0; // clean slate
	de_sat |= (r << 16);
	de_sat |= (g << 8);
	de_sat |= b;

	// darken every component, depending how close y is to 0.
	colour = 0;
	hue = (de_sat & 0x00FF0000) >> 16;
	hue *= y;
	colour |= hue << 16; // red
	hue = (de_sat & 0x0000FF00) >> 8; 
	hue *= y;
	colour |= hue << 8; // green
	hue = (de_sat & 0x000000FF); 
	hue *= y;
	colour |= hue; // blue

	return colour;
}

static unsigned long x_to_hue(float x)
{
	// x is in the range 0 to 1.
	unsigned long hue;
	int sixx;
	float sixxf, rem;
	unsigned long reml;

	sixx = x * 6.0f; // rounded down
	if(sixx == 6)
		sixx = 5;
	sixxf = x * 6.0f; // not rounded
	rem = sixxf - (float)sixx; // 0 to 1
	reml = rem * 255.0f;
	
	switch(sixx){
		case 0:
			hue = 0x00FF0000;
			hue |= (reml << 8);
			break;
		case 1:
			hue = 0x0000FF00;
			hue |= ((255 - reml) << 16);
			break;
		case 2:
			hue = 0x0000FF00;
			hue |= reml;
			break;
		case 3:
			hue = 0x000000FF;
			hue |= ((255 - reml) << 8);
			break;
		case 4:
			hue = 0x000000FF;
			hue |= (reml << 16);
			break;
		case 5:
			hue = 0x00FF0000;
			hue |= (255 - reml);
			break;
		default:
			hue = 0; break;
	}

	return hue;
}

static LRESULT CALLBACK sidebar_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT:
			paint_sidebar_window();
			return 0;
/*
		case WM_SIZE:
			adjust_window_size();
			return 0;
*/

		case WM_COMMAND:
			if(HIWORD(wParam) == BN_CLICKED){
				switch(LOWORD(wParam)){
				case 1:
					break;
				case 2:
					break;
				case 3:
					DestroyWindow(hwndmain);
					break;
				case 4:
					zoom(++pixel_size);
					break;
				case 5:
					zoom(--pixel_size);
					break;
				case 6:
					fit(&non_sidebar_rect);
					break;
				}
			}else if(HIWORD(wParam) == EN_CHANGE){ // edit control notification
				// interpret value as hex (if it's six characters long), and set colour to match
				if(user_edit) // don't react if I changed the text myself by sending a WM_SETTEXT
					set_colour_from_hex();
			}
			return 0;

		case WM_DESTROY:
			SelectObject(picker_dc, defaultbmp);
			DeleteDC(picker_dc);
			picker_dc = NULL;
			free(picker_pbmi);
			picker_pbmi = NULL;
			DeleteObject(c_picker);
			c_picker = NULL;
			return 0;

		case WM_LBUTTONDOWN:
			click(LOWORD(lParam), HIWORD(lParam)); // may begin drag
			return 0;

		case WM_LBUTTONUP:
			ReleaseCapture();
			drag_type = NO_DRAG;
			return 0;

		case WM_MOUSEMOVE:
			switch(drag_type){
				case PICKER_DRAG:
					pick(LOWORD(lParam), HIWORD(lParam));
					break;
				case SPECTRUM_DRAG:
					spec_pick(LOWORD(lParam));
					break;
				default:
					break;
			}
			return 0;
			

		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

static void click(short x_in, short y_in)
{
	// Here's where I realise the picker ought to have been a window in its own right.
	// Maybe. I dunno, it's not so hard to establish whether clicks are inside its rect.
	POINT pt = {x_in, y_in};

	// I *think* the right and bottom borders of rects are generally excluded from drawing.
	// So I think the picker doesn't cover those locations.

	if(x_in >= picker_rect.left && x_in < picker_rect.right &&
		y_in >= picker_rect.top && y_in < picker_rect.bottom){
	//if(PtInRect(&picker_rect, pt)){
		if(y_in >= spec_top){ // in spectrum strip
			SetCapture(sidebar_window_h);
			drag_type = SPECTRUM_DRAG;
			spec_pick(x_in);
		}else{ // in main part of picker
			SetCapture(sidebar_window_h);
			drag_type = PICKER_DRAG;
			pick(x_in, y_in);
		}
	}else if(PtInRect(&fore_union_rect, pt)){
		current_pick_RGB = &fore_pick_RGB;
		InvalidateRect(sidebar_window_h, &fore_union_rect, FALSE);
		InvalidateRect(sidebar_window_h, &back_union_rect, TRUE);
	}else if(PtInRect(&back_union_rect, pt)){
		current_pick_RGB = &back_pick_RGB;
		InvalidateRect(sidebar_window_h, &fore_union_rect, TRUE);
		InvalidateRect(sidebar_window_h, &back_union_rect, FALSE);
	}
}

static void pick(short x_in, short y_in)
{
	int x, y;

	// clamp
	if(x_in < picker_rect.left)
		x_in = picker_rect.left;
	if(y_in < picker_rect.top)
		y_in = picker_rect.top;
	if(x_in >= picker_rect.left + picker_width)
		x_in = picker_rect.left + picker_width - 1;
	if(y_in >= picker_rect.top + picker_width)
		y_in = picker_rect.top + picker_width - 1;

	x = x_in - picker_rect.left;
	y = picker_width - (y_in - picker_rect.top); // invert y relative to bitmap, bitmaps are upside down

	// seems like xy_to_colour actually likes y in the range 1 to max, but x in range 0 to max-1.

	*current_pick_RGB = xy_to_colour((float)x / (float)picker_width, (float)y / (float)picker_width);

	user_edit = false; // to avoid triggering stuff (says it's not a user edit)
	set_hex(*current_pick_RGB);

	// position marker on picker
	InvalidateRect(sidebar_window_h, &picked_pos, TRUE); //erase old one
	picked_pos.left = x_in - marker_size;
	picked_pos.top = y_in - marker_size;
	picked_pos.right = x_in + marker_size;
	picked_pos.bottom = y_in + marker_size;
	InvalidateRect(sidebar_window_h, &picked_pos, FALSE);

	if(current_pick_RGB == &fore_pick_RGB){
		InvalidateRect(sidebar_window_h, &fore_pick_rect, FALSE);
	}else{
		InvalidateRect(sidebar_window_h, &back_pick_rect, FALSE);
	}
}

static void spec_pick(short x)
{
	x -= picker_rect.left;

	if(x < 0)
		x = 0;
	if(x > picker_width)
		x = picker_width;

	picked_spec_RGB = x_to_hue((float)x / (picker_width));
	update_picker();
	InvalidateRect(sidebar_window_h, &picker_rect, FALSE);
	/*
	GetClientRect(sidebar_window_h, &scratch_rect);
	scratch_rect.top = spec_top;
	scratch_rect.bottom = picker_rect.bottom;
	InvalidateRect(sidebar_window_h, &scratch_rect, TRUE);
	*/
}

static void set_colour_from_hex(void)
{ // set the currently selected colour (fore or back) to the hex value in its associated edit box.
	char hex[7];
	unsigned long r, g, b;

	if(current_pick_RGB == &fore_pick_RGB){
		SendMessage(edit_fore_h, WM_GETTEXT, 7, (long)hex);
	}else{
		SendMessage(edit_back_h, WM_GETTEXT, 7, (long)hex);
	}

	if(strlen(hex) < 6)
		return;
	r = from_hex(hex[0], hex[1]);
	g = from_hex(hex[2], hex[3]);
	b = from_hex(hex[4], hex[5]);

	*current_pick_RGB = (r << 16) | (g << 8) | b;
	InvalidateRect(sidebar_window_h, &fore_pick_rect, FALSE);
}

static void set_hex(unsigned long colour)
{
	// convert input to hex string and put it in the relevant edit control
	char hex[7];
	unsigned long channel;

	hex[6] = '\0';

	channel = (colour & 0x00FF0000) >> 16;
	to_hex(&hex[0], &hex[1], channel);
	channel = (colour & 0x0000FF00) >> 8;
	to_hex(&hex[2], &hex[3], channel);
	channel = (colour & 0x000000FF);
	to_hex(&hex[4], &hex[5], channel);

	if(current_pick_RGB == &fore_pick_RGB){
		SendMessage(edit_fore_h, WM_SETTEXT, 0, (long)hex);
	}else{
		SendMessage(edit_back_h, WM_SETTEXT, 0, (long)hex);
	}
}

static void paint_sidebar_window(void)
{
	PAINTSTRUCT ps;
	HDC hdc;
	RECT r;

	hdc = BeginPaint(sidebar_window_h, &ps);

	GetClientRect(sidebar_window_h, &r);
	
	BitBlt(hdc, picker_rect.left, picker_rect.top, picker_rect.right - picker_rect.left,
		picker_rect.bottom - picker_rect.top, picker_dc, 0, 0, SRCCOPY);

	SelectObject(hdc, GetStockObject(NULL_BRUSH));
	SelectObject(hdc, GetStockObject(WHITE_PEN));
	Ellipse(hdc, picked_pos.left, picked_pos.top,
		picked_pos.right, picked_pos.bottom);

	if(current_pick_RGB == &fore_pick_RGB){
		FillRect(hdc, &fore_union_rect, grayb);
		FrameRect(hdc, &fore_union_rect, dkgrayb);
	}else{
		FillRect(hdc, &back_union_rect, grayb);
		FrameRect(hdc, &back_union_rect, dkgrayb);
	}

	SelectObject(hdc, GetStockObject(BLACK_PEN));
	SetBkMode(hdc, TRANSPARENT);

	DrawText(hdc, "Front:", 6, &fore_text_rect, DT_SINGLELINE);

	spareb = CreateSolidBrush(RGB_to_BGR(fore_pick_RGB));
	SelectObject(hdc, spareb);
	RoundRect(hdc, fore_pick_rect.left, fore_pick_rect.top, fore_pick_rect.right, fore_pick_rect.bottom, 10, 10);
	SelectObject(hdc, GetStockObject(BLACK_BRUSH));
	DeleteObject(spareb);

	DrawText(hdc, "Back:", 5, &back_text_rect, DT_SINGLELINE);
	spareb = CreateSolidBrush(RGB_to_BGR(back_pick_RGB));
	SelectObject(hdc, spareb);
	RoundRect(hdc, back_pick_rect.left, back_pick_rect.top, back_pick_rect.right, back_pick_rect.bottom, 10, 10);
	SelectObject(hdc, GetStockObject(BLACK_BRUSH));
	DeleteObject(spareb);

	MoveToEx(hdc, 0, 0, NULL);
	LineTo(hdc, 0, r.bottom);

	EndPaint(sidebar_window_h, &ps);
}

static LRESULT CALLBACK edit_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{	// subclassing the edit control in order to allow only valid hexadecimal keystrokes through

	// One drawback of this technique is that I have to whitelist every character I'll allow.
	// Forward delete is allowed through because it doesn't produce a WM_CHAR.
	// Backspace apparently does, so I have to allow 8.
	// The arrow keys don't. But what other keys might I have not thought of?
	// So yeah, it's OK, but a bit inflexible.

	switch(uMsg){
		case WM_CHAR:
			if((wParam >= '0' && wParam <= '9') || (wParam >= 'a' && wParam <= 'f') ||
				(wParam >= 'A' && wParam <= 'F') || wParam == 8){ // valid hex or edit char
				user_edit = true;
				return CallWindowProc(orig_edit_proc, hwnd, uMsg, wParam, lParam);
			}else
				return 0;

		default:
			return CallWindowProc(orig_edit_proc, hwnd, uMsg, wParam, lParam); 
	}
}
