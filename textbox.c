#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "textbox.h"
#include "main.h"

//---------------------------------------------------------------------------

#define	BUFFERSIZE 120
#define	HALFBUFFERSIZE 60
#define	LINESIZE 101

typedef struct hexletstruct{	// '00' to 'FF'
	unsigned char high;
	unsigned char low;
} hexlet;

struct syntactically_2D_array{
	char data[BUFFERSIZE][LINESIZE];
};

static void tohex(hexlet *);
static unsigned char fromhex(unsigned char, unsigned char);
static void newline_mechanism(void);
static bool get_index(long, long *);
static LRESULT CALLBACK txb_wnd_proc(HWND, UINT, WPARAM, LPARAM);
static void txb_paint_window(void);
static void txb_hit(short x, short y, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK scrollbar_wnd_proc(HWND, UINT, WPARAM, LPARAM);
static void txb_refresh_text(HDC);
static void txb_popmenu(short, short);
static void txb_popcommand(short);
static void txb_click_off(void);
static void txb_scroll(WPARAM);

void (*txb_char_function)(short) = NULL;
static WNDPROC origproc = NULL;
static bool txb_has_focus = false;
static HWND parent_window = NULL;

static struct syntactically_2D_array buffer;
static struct syntactically_2D_array *output;
static long curline = 0, curchar = 0, \
	oldest_line = 0, lines_extant = 1, hiliteline;
static bool hilite_on = false;
static HWND hwndscroll = 0;
static HMENU editpopmenu = 0, clickedmenu = 0;
static HFONT tempfont;
static RECT txb_box, scrollbar_box;
static RECT txb_text_area;
static short boxheight;
static int scrollbar_width;
static SCROLLINFO scrollsettings;
static long scroll_end = 0;
static long CH;
static bool newline_needed = false;
static bool show_scrollbar_yet = false;
static HFONT txb_font = NULL;
static HWND txb_window = NULL;
static HBRUSH txb_bgd_brush = NULL;
static char errstr[200];
static char errstr2[100];

void txbstring(char *c)
{
	if(newline_needed){
		newline_needed = false;
		newline_mechanism();
	}

	while ((output->data[curline][curchar] = *c++) != '\0'){
		if(curchar == LINESIZE-1){
			output->data[curline][LINESIZE-1] = '\0';
			break;
		}else
			curchar++;
	}
	InvalidateRect(txb_window, &txb_text_area, FALSE);
}

void txbrgb(COLORREF rgb4)
{
/* to display a colour:
	 write the string "rgb#" to the start of a line;
	 set the next 8 bytes to the colorref in hex.
*/
	hexlet h;

	txbstring("rgb#");

	output->data[curline][curchar++] = '0';
	output->data[curline][curchar++] = '0';

	h.low = (rgb4 & 0x000000FF);
	tohex(&h);
	output->data[curline][curchar++] = h.high;
	output->data[curline][curchar++] = h.low;
	
	h.low = (rgb4 & 0x0000FF00) >> 8;
	tohex(&h);
	output->data[curline][curchar++] = h.high;
	output->data[curline][curchar++] = h.low;
	
	h.low = (rgb4 & 0x00FF0000) >> 16;
	tohex(&h);
	output->data[curline][curchar++] = h.high;
	output->data[curline][curchar++] = h.low;

	// relying on txbstring to invalidate the text area, even though
	// we add the number to the text buffer afterward. This works
	// because the refresh is only triggered after we return.
}

static void tohex(hexlet *h) // input in h.low
{
	unsigned char high, low;

	high = ((h->low & 0xf0) >> 4) + 48;
	low = (h->low & 0xf) + 48;
	if(high > 57)
		high += 7;
	if(low > 57)
		low += 7;
	h->low = low;
	h->high = high;
}

static unsigned char fromhex(unsigned char high, unsigned char low)
{
	unsigned char result;

	if(high > 57)
		high-=7;
	high-=48;
	result = high << 4;

	if(low > 57)
		low-=7;
	low-=48;
	result += low;

	return result;
}

void txblong(long l)
{
	char strout[40];

 	_ltoa(l, &strout[0], 10);
	txbstring(&strout[0]);
}

void txblongs(long l1, long l2)
{
	txblong(l1);
	txbstring(", ");
	txblong(l2);
}

void txbul(unsigned long ul)
{
   char strout[40];

   _ultoa(ul, &strout[0], 10);
	txbstring(&strout[0]);
}

void txbflt(float f)
{
  char strout[40];

  snprintf(strout, 39, "%.6f", f);
	txbstring(&strout[0]);
}

void txbshtflt(float f)
{
  char strout[40];

  snprintf(strout, 39, "%.1f", f);
	txbstring(&strout[0]);
}

void txblngflt(float f)
{
  char strout[40];

  snprintf(strout, 39, "%.10f", f);
	txbstring(&strout[0]);
}

void txbbin8(unsigned char b)
{
  char strout[40];

	strout[7] = 48 + ((b & 1) != 0);
	strout[6] = 48 + ((b & 2) != 0);
	strout[5] = 48 + ((b & 4) != 0);
	strout[4] = 48 + ((b & 8) != 0);
	strout[3] = 48 + ((b & 16) != 0);
	strout[2] = 48 + ((b & 32) != 0);
	strout[1] = 48 + ((b & 64) != 0);
	strout[0] = 48 + ((b & 128) != 0);

	strout[8] = '\0';

	txbstring(&strout[0]);
}

void txbbin32(unsigned long l)
{
  char strout[40];
	int i;

	for(i = 0; i < 32; i++){
		strout[31 - i] = 48 + ((l & (1 << i)) != 0);
	}

	strout[32] = '\0';

	txbstring(&strout[0]);
}

void txbhex(unsigned char h)
{
  char strout[40];

  snprintf(strout, 39, "%.2hhx", h);
	txbstring(&strout[0]);
}

void txbmatrix(float *m)
{
	txbstring("ROW MAJOR order");	txbnl();
	txbflt(m[0]);	txbstring(" ");
	txbflt(m[4]);	txbstring(" ");
	txbflt(m[8]);	txbstring(" ");
	txbflt(m[12]);	txbstring(" ");	txbnl();
	txbflt(m[1]);	txbstring(" ");
	txbflt(m[5]);	txbstring(" ");
	txbflt(m[9]);	txbstring(" ");
	txbflt(m[13]);	txbstring(" ");	txbnl();
	txbflt(m[2]);	txbstring(" ");
	txbflt(m[6]);	txbstring(" ");
	txbflt(m[10]);	txbstring(" ");
	txbflt(m[14]);	txbstring(" ");	txbnl();
	txbflt(m[3]);	txbstring(" ");
	txbflt(m[7]);	txbstring(" ");
	txbflt(m[11]);	txbstring(" ");
	txbflt(m[15]);	txbstring(" ");	txbnl();
}

void txbnl(void)
{
	// Unusual case (where this newline has directly followed another)
	if(newline_needed){
		newline_mechanism();
		InvalidateRect(txb_window, &txb_text_area, FALSE);
	}
	// Usual case:
	// terminate the line, in case nothing was written to it
	output->data[curline][curchar] = '\0';
	// do nothing else; set a flag to say the next text will create a new line
	newline_needed = true;
	// txbstring() will then call newline_mechanism() on any output,
	// and will set the flag to false.
}

static void newline_mechanism(void)
{
	// current line increases
	curline++;
	if(curline == BUFFERSIZE)
		curline -= HALFBUFFERSIZE;

	if(lines_extant < BUFFERSIZE){
		lines_extant++;	// line count increases

		if(lines_extant == HALFBUFFERSIZE){
			curchar = 0;
			txbstring("___Recent messages follow___");
			curchar = 0;
			curline++;
			lines_extant++;
		}
		
		// show scrollbar for the first time:
		if(!show_scrollbar_yet && lines_extant > scrollsettings.nPage){
			ShowScrollBar(hwndscroll, SB_CTL, TRUE);
			txb_text_area.right -= scrollbar_width;
			show_scrollbar_yet = true;
		}

		// increase scroll range:
		scrollsettings.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		//	If I don't set the page size every bloody time,
		//	nMax gets set to some large, incorrect number.
		//	No idea why. Blame Microsoft.

		scrollsettings.nMax = lines_extant - 1;
		scroll_end = scrollsettings.nMax + 1 - scrollsettings.nPage;
		if(scroll_end < 0)
			scroll_end = 0;
	}else{
		oldest_line++;	// once buffer is full, oldest line increases too
		oldest_line %= HALFBUFFERSIZE;
		if(curline == hiliteline){
			hilite_on = false; // remove highlight if overwriting highlighted line
			EnableMenuItem(editpopmenu, 1, MF_GRAYED);
		}
		scrollsettings.fMask = SIF_POS;		// overfussy pointlessness FTW
	}

	curchar = 0;		// current char resets to start of line

	// autoscroll to output line:

	scrollsettings.nPos = scroll_end;
	SetScrollInfo(hwndscroll, SB_CTL, &scrollsettings, TRUE);
}

static bool get_index(long i, long *index)
{
	long line;

	line = i + scrollsettings.nPos;

	if(line >= HALFBUFFERSIZE){
		*index = oldest_line + line;
		if(*index >= BUFFERSIZE)
			*index -= HALFBUFFERSIZE; 
		return true;
	}else if(line < lines_extant){
		*index = line;
		return true;
	}else
		return false;
}

static void txb_refresh_text(HDC hdc)
{
	long i, ii;
	RECT r;
	unsigned long* csig;
	COLORREF setc, hiforecol, hibackcol;
	unsigned char red, green, blue;

	r.left = txb_text_area.left + 2; r.top = txb_text_area.top;
	r.right = txb_text_area.right; r.bottom = r.top + CH;

	hiforecol = GetSysColor(COLOR_HIGHLIGHTTEXT);
	hibackcol = GetSysColor(COLOR_HIGHLIGHT);

	for(i = 0; i < scrollsettings.nPage; i++){
		FillRect(hdc, &r, txb_bgd_brush);
		if(get_index(i, &ii)){
			csig = (unsigned long*) &output->data[ii][0];
			if(ii == hiliteline && hilite_on){
				SetTextColor(hdc, hiforecol);
				SetBkColor(hdc, hibackcol);
				SetBkMode(hdc, OPAQUE);
			}else if(*csig == '#bgr'){	// lines starts "rgb#"
				SetTextColor(hdc, 0);
				// next two bytes are '0' ... may be alpha later
				red = fromhex(output->data[ii][6], output->data[ii][7]);
				green = fromhex(output->data[ii][8], output->data[ii][9]);
				blue = fromhex(output->data[ii][10], output->data[ii][11]);
				setc = red + (green << 8) + (blue << 16);
				SetBkColor(hdc, setc);
				SetBkMode(hdc, OPAQUE);
			}else{
				SetTextColor(hdc, 0);
				SetBkColor(hdc, 0x00FFFFFF); // shouldn't be seen, but hey
				SetBkMode(hdc, TRANSPARENT);
			}

			DrawText(hdc, &output->data[ii][0], -1, &r, DT_SINGLELINE);
		}
		r.top += CH;
		r.bottom += CH;
	}

	// fill remaining space with white
	r.bottom = txb_text_area.bottom;
	FillRect(hdc, &r, txb_bgd_brush);
	// fill sliver at left
	r.left = txb_text_area.left; r.right = txb_text_area.left + 2;
	r.top = txb_text_area.top; r.bottom = txb_text_area.bottom;
	FillRect(hdc, &r, txb_bgd_brush);

	//FrameRect(hdc, &txb_box, redb);
}

static void txb_popmenu(short x, short y)
{
	TrackPopupMenuEx(editpopmenu, TPM_VERTICAL, x, y, txb_window, 0);
}

static void txb_popcommand(short item)
{
	HGLOBAL data;
	LPVOID datap;
	unsigned char *bodge;
	long i, ii, i2;

	switch(item){
		case 1:
		if(OpenClipboard(0) && EmptyClipboard()){
			data = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, LINESIZE);
			datap = GlobalLock(data);
			CopyMemory(datap, &output->data[hiliteline][0], LINESIZE);
			GlobalUnlock(data);
			SetClipboardData(CF_TEXT, data);
			CloseClipboard();
		}
		break;
		case 2:
		if(OpenClipboard(0) && EmptyClipboard()){
			data = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, (LINESIZE + 2) * BUFFERSIZE + 1);
			// each line +cr+lf
			datap = GlobalLock(data);
			bodge = datap;
			// customised for this two-part buffer version of textbox
			for(i = 0; i < HALFBUFFERSIZE && i < lines_extant; i++){
				i2 = 0;
				while(output->data[i][i2] && i2 < LINESIZE){
					*bodge++ = output->data[i][i2];
					i2++;
				}
				*bodge++ = '\r';
				*bodge++ = '\n';
			}
			if(lines_extant > HALFBUFFERSIZE){
				for(i = 0; i < HALFBUFFERSIZE; i++){
					if(i + HALFBUFFERSIZE == lines_extant)
						break;
					ii = HALFBUFFERSIZE + oldest_line + i;
					if(ii >= BUFFERSIZE)
						ii -= HALFBUFFERSIZE;
					i2 = 0;
					while(output->data[ii][i2] && i2 < LINESIZE){
						*bodge++ = output->data[ii][i2];
						i2++;
					}
					*bodge++ = '\r';
					*bodge++ = '\n';
				}
			}
			*bodge++ = '\0';
			GlobalUnlock(data);
			SetClipboardData(CF_TEXT, data);
			CloseClipboard();
		}
		break;
		default:
		break;
	}
}

static void txb_hit(short x, short y, WPARAM wParam, LPARAM lParam)
{
	RECT r;
	short rounddown;
	long test;

	if(hilite_on){
		txb_click_off();
		return;
	}

	r.top = 0;
	r.bottom = 0;
 	r.left = txb_text_area.left + 2;
	r.right = txb_text_area.right;

	y -= txb_text_area.top;
	
	// work out which line of the buffer it is
	if(get_index(y / CH, &test)){
		hiliteline = test;
		hilite_on = true;
	
		// rect to highlight is based directly on click position
		rounddown = y / CH;
		rounddown *= CH;
		r.top = txb_text_area.top + rounddown;
		r.bottom = r.top + CH;
		EnableMenuItem(editpopmenu, 1, MF_ENABLED);
		InvalidateRect(txb_window, &r, FALSE);
	}
}

static void txb_click_off(void)
{
	RECT r;
	long pos;

	if(!hilite_on)
		return;

	if(hiliteline < HALFBUFFERSIZE)
		pos = hiliteline;
	else{
		pos = hiliteline - oldest_line;
 		if(pos < HALFBUFFERSIZE)
 			pos += HALFBUFFERSIZE;
	}
		
	if(pos >= scrollsettings.nPos && \
	pos < scrollsettings.nPos + scrollsettings.nPage){
		// highlighted line is visible on the screen

	 	r.left = txb_text_area.left + 2;
		r.right = txb_text_area.right;
		r.top = (pos - scrollsettings.nPos) * CH + txb_text_area.top;
		r.bottom = r.top + CH;
		InvalidateRect(txb_window, &r, FALSE);
	}

	hilite_on = false; // remove highlight marker from any line it's on
	EnableMenuItem(editpopmenu, 1, MF_GRAYED);
}

static void txb_scroll(WPARAM wParam)
{
	int nScrollCode;
	short nPos;

	nScrollCode = (int) LOWORD(wParam); // scroll bar value 
	nPos = (short) HIWORD(wParam); 

	switch (nScrollCode){
		case	SB_LINEDOWN:
			if(scrollsettings.nPos < scroll_end)
			scrollsettings.nPos++;
			break;
		case SB_LINEUP:
			if(scrollsettings.nPos > 0)
				scrollsettings.nPos--;
			break;
		case SB_PAGEDOWN:
			scrollsettings.nPos+=scrollsettings.nPage;
			if(scrollsettings.nPos > scroll_end)
				scrollsettings.nPos = scroll_end;
			break;
		case SB_PAGEUP:
			scrollsettings.nPos-=scrollsettings.nPage;
			if(scrollsettings.nPos < 0)
				scrollsettings.nPos = 0;
			break;
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			scrollsettings.nPos = nPos;
			break;
		default: break;
	}
	scrollsettings.fMask = SIF_POS;
	SetScrollInfo(hwndscroll, SB_CTL, &scrollsettings, TRUE);
	InvalidateRect(txb_window, &txb_text_area, FALSE);
}

static LRESULT CALLBACK scrollbar_wnd_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
		// Can pass messages to the main textbox window if needed:
		//CallWindowProc(txb_wnd_proc, hwnd, uMsg, wParam, lParam); 

	switch(uMsg)
	{
		case WM_MOUSEMOVE:			
			if(!txb_has_focus){
				if(IsChild(GetActiveWindow(), txb_window)
					|| GetActiveWindow() == txb_window){
					SetFocus(txb_window);
					txb_has_focus = true;
				}
			}
		break;
/*
     	case WM_DESTROY: 
            // Remove the subclass from the control. 
            SetWindowLong(hwnd, GWL_WNDPROC, 
                (LONG) wpOrigProc); 
            // 

	*/
	}

 	return CallWindowProc(origproc, hwnd, uMsg, wParam, lParam); 
}

static LRESULT CALLBACK txb_wnd_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT:
			txb_paint_window(); // wparam is supposed to be the HDC but ignore it?
			return 0; 		 // use HDC returned by beginpaint instead?
/*
		case WM_SIZE:
			adjust_window_size();
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
*/

		case WM_CHAR:
			if(txb_char_function)
				(*txb_char_function)(wParam);
			else if(parent_window)
				PostMessage(parent_window, uMsg, wParam, lParam);
			return 0;

		case WM_MOUSEMOVE:
			if(txb_has_focus)
				return 0;
			if(IsChild(GetActiveWindow(), txb_window)
				|| GetActiveWindow() == txb_window){
					SetFocus(txb_window);
					txb_has_focus = true;
			}
			return 0;

		case WM_KILLFOCUS:
			txb_has_focus = false;
			return 0;

		case WM_MOUSEWHEEL:
			mousewheelaccumulator += (short)HIWORD(wParam);
			if(mousewheelaccumulator < 120 && mousewheelaccumulator > -120)
				return 0;
			if(mousewheelaccumulator / 120 > 0)
				txb_scroll(SB_LINEUP);
			else
				txb_scroll(SB_LINEDOWN);
			mousewheelaccumulator = 0;
			return 0;

		case WM_VSCROLL:
			if(hwndscroll && (HWND)lParam == hwndscroll)
				txb_scroll(wParam);
			return 0;

		case WM_LBUTTONDOWN:
			txb_hit(LOWORD(lParam), HIWORD(lParam), wParam, lParam);
			return 0;

		case WM_CONTEXTMENU:
			txb_popmenu(LOWORD(lParam), HIWORD(lParam));
			return 0;

		case WM_INITMENU:
			clickedmenu = (HMENU)wParam;
			return 0;

		case WM_COMMAND:
			if(clickedmenu == editpopmenu)
				txb_popcommand(LOWORD(wParam));
			return 0;

		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

static void txb_paint_window(void)
{
	HDC hdc;
	PAINTSTRUCT ps;

	hdc = BeginPaint(txb_window, &ps);
	tempfont = SelectObject(hdc, txb_font);
	txb_refresh_text(hdc);
	SelectObject(hdc, tempfont);
	EndPaint(txb_window, &ps);
}

bool txb_make_text_box(int left, int top, int width, int height, char *name, DWORD style, HWND parent, HICON icon, HBRUSH bgd, HFONT font)
{
	WNDCLASSEX wc;
	DWORD errnum;

	char *default_name = "Log";

	if(!bgd)
		bgd = GetSysColorBrush(COLOR_INFOBK);

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)txb_wnd_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = icon;
	wc.hCursor = LoadCursor((HINSTANCE) NULL, IDC_ARROW);
	wc.hbrBackground = bgd;
	wc.lpszMenuName = NULL; // "MainMenu";
	wc.lpszClassName = "TxbWndClass";
	wc.hIconSm = NULL;

	if(!name)
		name = default_name;

	if (!RegisterClassEx(&wc)){
		errnum = GetLastError();
		strcpy(errstr, "Couldn't register \"");
			goto reg_create_err;
	}

	txb_window = CreateWindow("TxbWndClass", name, \
		style, left, top, width, height, parent, \
		(HMENU) NULL, hInstance, (LPVOID) NULL);

	if (!txb_window){
		errnum = GetLastError();
		strcpy(errstr, "Couldn't create \"");
			goto reg_create_err;
	}

	parent_window = parent;
	
	txb_bgd_brush = wc.hbrBackground;

	GetClientRect(txb_window, &txb_box);

	if(font)
		txb_font = font;
	else{
		txb_font = CreateFont(14, 0, 0, 0, 0, 0, 0, 0, \
			ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, \
			DEFAULT_QUALITY, VARIABLE_PITCH | FF_DONTCARE, "Microsoft Sans Serif");
	}

	if(!txb_font)
		txb_font = GetStockObject(SYSTEM_FONT);

	// dummy values in much of the following:
	hwndscroll = CreateWindowEx(0, "SCROLLBAR", (LPSTR)NULL, \
		WS_CHILD | SBS_VERT | SBS_RIGHTALIGN, \
		txb_box.right - 14, txb_box.top, 14, 100, txb_window, \
		(HMENU)NULL, hInstance, (LPVOID)NULL);
	// will be set up properly in txb_resize().

	origproc = (WNDPROC) SetWindowLong(hwndscroll, GWL_WNDPROC, (long)scrollbar_wnd_proc); 

	scrollsettings.nPage = 10; // dummy, gets set up in txb_resize().
	scrollsettings.nMin = 0;
	scrollsettings.cbSize = sizeof(SCROLLINFO);
	scrollsettings.nPos = 0;
	scrollsettings.fMask = SIF_ALL;
	SetScrollInfo(hwndscroll, SB_CTL, &scrollsettings, TRUE);
	
	txb_resize();

	ShowScrollBar(hwndscroll, SB_CTL, FALSE);

	output = &buffer;

	output->data[0][0] = '\0'; // just making sure -
	// the first line gets drawn before there's any data, so let's terminate it.
	// This could be used to display a welcome message or something.

	editpopmenu = CreatePopupMenu();
	AppendMenu(editpopmenu, MF_STRING + MF_GRAYED, 1, "Copy");
	AppendMenu(editpopmenu, MF_STRING, 2, "Copy all");

	ShowWindow(txb_window, SW_SHOW);
	
	return true;

	//	ERRORS

reg_create_err:
	strcat(errstr, name);
	strcat(errstr, "\" window. ");
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | 100, 0, errnum,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errstr2, 100, 0);
	strcat(errstr, errstr2);
	MessageBox(parent, errstr, "UNFORTUNATE ERROR",
		MB_OK | MB_ICONINFORMATION | MB_APPLMODAL);
	return false;
}

void txb_resize(void)
{
	TEXTMETRIC unwieldystruct;
	HDC hdcmain;
	short height_diff;
	RECT w_rect;

	hdcmain = GetDC(txb_window);
	// Note - GetDC resets all the DCs settings, as it has no storage,
	// unless we used CS_OWNDC to create a private DC for the window.
	// So now we have to select the font, can't just store it there.
	tempfont = SelectObject(hdcmain, txb_font);
	GetTextMetrics(hdcmain, &unwieldystruct);
	SelectObject(hdcmain, tempfont);
	ReleaseDC(txb_window, hdcmain);
	CH = unwieldystruct.tmHeight;

	scrollbar_width = GetSystemMetrics(SM_CXVSCROLL);
	boxheight = txb_box.bottom - txb_box.top;
	height_diff = boxheight;
	boxheight /= CH;
	boxheight *= CH; // rounding height down to multiples of line height
	txb_box.bottom = txb_box.top + boxheight;
	txb_text_area = txb_box; // text_area is the non-scroll-bar area
	height_diff -= boxheight; // this holds the remainder after rounding down

	MoveWindow(hwndscroll, txb_box.right - scrollbar_width, txb_box.top, \
		scrollbar_width, boxheight, TRUE);

	GetWindowRect(txb_window, &w_rect);
	MoveWindow(txb_window, w_rect.left, w_rect.top, w_rect.right - w_rect.left, \
		w_rect.bottom - w_rect.top - height_diff, TRUE);

	GetWindowRect(hwndscroll, &scrollbar_box);
	MapWindowPoints(NULL, txb_window, (POINT *)&scrollbar_box, 2);

	scrollsettings.nPage = boxheight / CH;
	scrollsettings.fMask = SIF_PAGE;
	SetScrollInfo(hwndscroll, SB_CTL, &scrollsettings, TRUE);
}

void txb_delete_text_box(void)
{
// not necessary to delete child windows (i.e. scroll bars)
	DestroyMenu(editpopmenu);
}

