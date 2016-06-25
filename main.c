#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include "main.h"
#include "brushes_etc.h"
#include "main_window.h"
#include "textbox.h"

HINSTANCE hInstance;

RECT scratch_rect;
HACCEL acc_table;

long mousewheelaccumulator = 0;
unsigned long timerID;
unsigned long framecount = 0;
long line_count = 0;

char errstr[200];

int general_init(void);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void load_err_msg(DWORD);
void cleanup(void);
void frame(void);

void frame(void)
{
	framecount++;
	if(!(framecount % 100)){
		txbstring("Frame ");
		txbul(framecount);
		txbnl();
	}
}

int general_init(void)
{
	SYSTEMTIME thetime;
	unsigned int random_seed;

	InitCommonControls();

	txb_make_text_box(37, 0, 950, 200, NULL, WS_OVERLAPPED, NULL,
		LoadIcon((HINSTANCE) NULL, IDI_ASTERISK), NULL, NULL);
	//Return code zero means textbox failed to create: pick up on that if it matters.
	// The error alert is already taken care of, but it doesn't quit the progam.

	if(!main_window_init())
		return 0;

	acc_table = LoadAccelerators(hInstance, MAKEINTRESOURCE(3001));

	GetSystemTime(&thetime);
	random_seed = (thetime.wMilliseconds << 16) + thetime.wSecond;
	srand(random_seed);

	return 1;
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE prev, LPSTR cmdl, int show)
{
	MSG msg;

	hInstance = hi;

	if(!general_init()){
		MessageBox(NULL, "window creation failed", "UNFORTUNATE ERROR",
			MB_OK | MB_ICONINFORMATION | MB_APPLMODAL);
		return 0;
	}

	//testing exceptions

	while (GetMessage(&msg, (HWND)NULL, 0, 0))
	{			
		__try {
			if(!TranslateAccelerator(hwndmain, acc_table, &msg)){
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER){			
			load_err_msg( _exception_code());
			//strcat(errstr, "Continue?");
			MessageBox(hwndmain, errstr, "Error",
				MB_YESNO | MB_ICONERROR | MB_APPLMODAL);
		}
	}

	cleanup();

 	return msg.wParam;
}

void load_err_msg(DWORD Err)
{
	HMODULE Hdll = LoadLibrary("NTDLL.DLL");

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
		Hdll, Err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errstr, 100, 0);

	FreeLibrary(Hdll);
}

RECT screen_size(void)
{	// Simple ways to get the screen size aren't reliable
	// so here's the Raymond Chen way ... *and* a simple way as backup.
	MONITORINFO mi = {sizeof(mi)};
	RECT r = {0, 0, 640, 480}; //defaults if everything else fails, heh

	// main window has to exist (not necessarily visibly) before calling this.

	// I don't know what MonitorFromWindow does. Presumably it picks a "better" monitor
	// for the window sometimes, but that sounds like trying to be way too clever.

	if(GetMonitorInfo(MonitorFromWindow(hwndmain, MONITOR_DEFAULTTOPRIMARY), &mi))
		return mi.rcMonitor;
	else 
		GetClientRect(GetDesktopWindow(), &r);
	return r;
}

// parameters: a pointer to a pointer - windows allocates the actual buffer and hands back a pointer.
// Bit vague on what type the input should be. Unsigned long (32 bits at a time, 1 pixel) seems sensible.
// Plus a pointer to a pointer to a BITMAPINFO. Storage for that gets created here, must be freed on quit.
// HDC: pass a null HDC to accept a handle to the DC that gets created here.
// 		Example: the_bmp = DIB_maker(&the_bits, &the_pbmi, &the_dc, w, h);
HBITMAP DIB_maker(unsigned long **bits, PBITMAPINFO *ppbmi, HDC *dc, int w, int h)
{
	HBITMAP bmp;
	HDC hdcmain;
	PBITMAPINFO pbmi;

	*ppbmi = malloc(sizeof(BITMAPINFO) + sizeof(RGBQUAD));
	pbmi = *ppbmi;
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = w;
	pbmi->bmiHeader.biHeight = h;
	pbmi->bmiHeader.biPlanes = 1;
	pbmi->bmiHeader.biBitCount = 32;
	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biXPelsPerMeter = 2835; // This is DPI, which is hopefully irrelevant
	pbmi->bmiHeader.biYPelsPerMeter = 2835; // Except maybe for printing but I hope not even that.
	pbmi->bmiHeader.biClrUsed = 0;
	pbmi->bmiHeader.biClrImportant = 0;
	pbmi->bmiHeader.biSizeImage = 0; //"size, in bytes, of the image. This may be set to 0 for BI_RGB bitmaps."

	// I will save the DPI arithmetic here for posterity:
	// 2835 = 72 DPI * 39.3701 inches per meter, rounded up. That's apparently Mac DPI, and Windows uses 96. Whatever.

	hdcmain = GetDC(hwndmain);
	*dc = CreateCompatibleDC(hdcmain);
	ReleaseDC(hwndmain, hdcmain);

	bmp = CreateDIBSection(*dc, pbmi, DIB_RGB_COLORS, (void**)bits, NULL, 0);
	if(bmp){
		SelectObject(*dc, bmp);
		txbstring("DIB: success"); txbnl();
	}
	return bmp;
}

COLORREF RGB_to_BGR(unsigned long rgb)
{
	COLORREF bgr = 0, component;
	
	component = (rgb & 0x00FF0000) >> 16;
	bgr |= component; // red
	bgr |= (rgb & 0x0000FF00); // green
	component = (rgb & 0x000000FF) << 16;
	bgr |= component; // blue

	return bgr;
}

// turn one unsigned byte of input into two hex characters, 00 to FF
void to_hex(char *high, char *low, unsigned char in)
{
	*high = '0' + ((in & 0xf0) >> 4);
	*low = '0' + (in & 0xf);
	if(*high > '9')
		*high += 7;
	if(*low > '9')
		*low += 7;
}

// two hex characters to one unsigned char, 0 to 255
unsigned char from_hex(char high, char low)
{
	unsigned char out = 0;

	if(high > '9')
		high -= 7;
	high -= '0';
	if(low > '9')
		low -= 7;
	low -= '0';

	out = high;
	out = out << 4;
	out |= low;

	return out;
}

// slightly surprised this doesn't exist already
char *strtolower(char *string)
{
	char *orig;

	orig = string;
	while(*string = tolower(*string))
		string++;
	return orig;
}

void cleanup(void)
{
	//KillTimer(NULL, timerID);

	main_window_cleanup();

	txb_delete_text_box();
}
