#include <windows.h>
#include <stdbool.h>
#include "main.h"
#include "main_window.h"
#include "brushes_etc.h"
#include "sidebar.h"
#include "textbox.h"
#include "drawing_window.h"

HWND drawing_window_h = NULL;
int pixel_size = 8, magnify = 1;
int x_tiles = 32, y_tiles = 70;
bool grid_on = true;
static bool draw_drag = false;
unsigned long *pixel_buffer = NULL; // system-allocated, as part of a DIB
HBITMAP pixels_bmp = NULL; // the DIB
PBITMAPINFO pixels_pbmi = NULL; // its BITMAPINFO struct (possibly the same address?)
static HBITMAP defaultbmp = NULL;
static HDC pixels_dc = NULL; // its DC

static LRESULT CALLBACK drawwnd_proc(HWND, UINT, WPARAM, LPARAM);
static void click(short, short, bool);
static void paint_drawing_window(void);
static void calc_drawing_window(RECT *, RECT *);

bool init_drawing_window(void)
{
	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)drawwnd_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon((HINSTANCE) NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(hInstance, MAKEINTRESOURCE(8001));//LoadCursor((HINSTANCE) NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "drawing_wc";
	wc.hIconSm = NULL;

	if (!RegisterClassEx(&wc))
			return false;
	return true;
}

bool make_drawing_window(void) // input old buffer, if resizing
{
	RECT r;

	// Make buffer
	pixels_bmp = resize_pixel_buffer(x_tiles, y_tiles);

	calc_drawing_window(&non_sidebar_rect, &r);

	drawing_window_h = CreateWindowEx(0, "drawing_wc", "Pixels", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPSIBLINGS,
		r.left, r.top, r.right - r.left, r.bottom - r.top,
		hwndmain, (HMENU)NULL, hInstance, NULL);

	if(!drawing_window_h)
		return false;

	SetWindowPos(drawing_window_h, sidebar_window_h, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	return true;
}

HBITMAP resize_pixel_buffer(int new_x_tiles, int new_y_tiles)
{
	HBITMAP new_bmp = NULL;
	HDC new_DC = NULL;
	PBITMAPINFO new_pbmi = NULL;
	unsigned long *new_buffer = NULL;
	int i, x, y, min_x, min_y;

	new_bmp = DIB_maker(&new_buffer, &new_pbmi, &new_DC, new_x_tiles, new_y_tiles);

	if(!new_bmp || !new_buffer){ // don't really need to check both, to be honest.
		MessageBox(hwndmain, "Couldn't create pixel buffer", "Error", MB_OK | MB_ICONERROR | MB_APPLMODAL);
		// then do I don't know what other sensible stuff to gracefully fail
		// but of course I haven't thought about that properly yet
		return false;
	}

	for(i = 0; i < new_x_tiles * new_y_tiles; i++)
		*(new_buffer + i) = 0x00FFFFFF;

	min_x = (new_x_tiles < x_tiles ? new_x_tiles : x_tiles);
	min_y = (new_y_tiles < y_tiles ? new_y_tiles : y_tiles);

	if(pixel_buffer){
		// copy stuff over to new buffer first
		for(x = 0; x < min_x; x++){
			for(y = 0; y < min_y; y++){
				*(new_buffer + x + y * new_x_tiles) = *(pixel_buffer + x + y * x_tiles);
			}
		}
		SelectObject(pixels_dc, defaultbmp);
		DeleteDC(pixels_dc);
		free(pixels_pbmi);		
		DeleteObject(pixels_bmp);
	}

	pixels_dc = new_DC;
	pixels_pbmi = new_pbmi;
	pixel_buffer = new_buffer;

	x_tiles = new_x_tiles;
	y_tiles = new_y_tiles;

	return new_bmp; // inevitably is assigned to pixels_bmp
}

static void calc_drawing_window(RECT *input, RECT *output)
{
	// input: the rect to fit the drawing window into.
	// output: a centered window rect, sized according to the current pixel size.
	int left, top, w, h, wrem, hrem;

	w = input->right - input->left;
	h = input->bottom - input->top;

	// Resize (inner) window to fit tiles

	wrem = w - x_tiles * pixel_size;
	hrem = h - y_tiles * pixel_size;
	left = wrem / 2; // try adding input->left here
	top = hrem / 2; // and input->top. Don't want to hazard this yet.
	w -= wrem;
	h -= hrem;

	output->left = left;
	output->right = left + w;
	output->top = top;
	output->bottom = top + h;
	// make window rect bigger, accounting for frame, so that client rect can have the specified size:
	AdjustWindowRect(output, WS_CHILD | WS_BORDER, FALSE);
}

void fit(RECT *r)
{// zoom to fit to rect
	int x, y, sz;

	x = (r->right - r->left) / x_tiles;
	y = (r->bottom - r->top) / y_tiles;
	sz = (x < y ? x : y);

	zoom(sz);
}

void zoom(int new_pixel_size)
{// * Change pixel_size, resize the drawing window, and redraw.
	RECT r;

	if(new_pixel_size < 1)
		new_pixel_size = 1;

	pixel_size = new_pixel_size;
	calc_drawing_window(&non_sidebar_rect, &r);
	SetWindowPos(drawing_window_h, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top,
		SWP_NOZORDER | SWP_NOCOPYBITS);

	// SWP triggers repaint anyway, unless new size is the same as old size.
	// But we might be going from e.g. 10x10 to 20x20, and the window size would stay the same,
	// but we would still want a repaint. So ask for one:
	GetClientRect(drawing_window_h, &r);
	InvalidateRect(drawing_window_h, &r, FALSE);
	// Note that SetWindowPos seems to work via the message queue, which means that
	// it takes effect after the end of this function, which further means that
	// if it sets the update region itself, it overrules this InvalidateRect call.
	// So this part only really has an effect if the window size didn't change.
}

static LRESULT CALLBACK drawwnd_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT:
			paint_drawing_window();
			return 0;
/*
		case WM_SIZE:
			adjust_window_size();
			return 0;
*/

		case WM_DESTROY:
			SelectObject(pixels_dc, defaultbmp);
			DeleteDC(pixels_dc);
			pixels_dc = NULL;
			free(pixels_pbmi);
			pixels_pbmi = NULL;
			DeleteObject(pixels_bmp);
			pixels_bmp = NULL;
			return 0;

		case WM_LBUTTONDOWN:
			SetCapture(drawing_window_h);
			draw_drag = true;
			click(LOWORD(lParam), HIWORD(lParam), true);
			return 0;

		case WM_RBUTTONDOWN:
			SetCapture(drawing_window_h);
			draw_drag = true;
			click(LOWORD(lParam), HIWORD(lParam), false);
			return 0;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			ReleaseCapture();
			draw_drag = false;
			return 0;

		case WM_MOUSEMOVE:
			if(draw_drag){
				if(wParam & MK_LBUTTON) // left button trumps right button
					click(LOWORD(lParam), HIWORD(lParam), true);
				else if(wParam & MK_RBUTTON)
					click(LOWORD(lParam), HIWORD(lParam), false);
			}
			return 0;

		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

static void click(short x, short y, bool left)
{
	unsigned long which;

	if(left)
		which = fore_pick_RGB;
	else
		which = back_pick_RGB;

	x /= pixel_size;
	y /= pixel_size;

	if(x >= x_tiles || y >= y_tiles || x < 0 || y < 0)
		return;

	// put the pixel in the bitmap's bits
	// (invert y because bitmaps are bottom-up)
	*(pixel_buffer + x + (y_tiles - y - 1) * x_tiles) = which;

	x *= pixel_size;
	y *= pixel_size; // results are rounded compared to original x and y
	scratch_rect.left = x;
	scratch_rect.top = y;
	scratch_rect.right = x + pixel_size;
	scratch_rect.bottom = y + pixel_size;

	//SetCursor(LoadCursor(NULL, IDC_HAND));
	//SetCursor(LoadCursor(hInstance, MAKEINTRESOURCE(8001)));

	InvalidateRect(drawing_window_h, &scratch_rect, FALSE);
}

static void paint_drawing_window(void)
{
	PAINTSTRUCT ps;
	HDC hdc;
	int x, y;

	hdc = BeginPaint(drawing_window_h, &ps);

	// Draw the pixel-tiles
	/*
	for(x = 0; x < x_tiles; x++){
		for(y = 0; y < y_tiles; y++){
			// here I fiddle with the RGB order. COLORREF has it 0BGR, DIB_RGB_COLORS has it 0RGB.
			// there is also the option of using masks (compression type 3) to change the order in the bitmap,
			// which might be what Windows defaults to when you CreateCompatibleBitmap. Anyway, for now, I've
			// got 0RGB format for the pixel_buffer.
			spareb = CreateSolidBrush(RGB_to_BGR(*(pixel_buffer + x + y * x_tiles)));
			//spareb = CreateSolidBrush(0x00BDBDBD); // useful for debugging
			scratch_rect.left = x * pixel_size;
			scratch_rect.top = y * pixel_size;
			scratch_rect.right = scratch_rect.left + pixel_size;
			scratch_rect.bottom = scratch_rect.top + pixel_size;
			FillRect(hdc, &scratch_rect, spareb);
			SelectObject(hdc, GetStockObject(BLACK_BRUSH));
			DeleteObject(spareb);
		}
	}*/

	GetClientRect(drawing_window_h, &scratch_rect);

	StretchBlt(hdc, 0, 0, scratch_rect.right - scratch_rect.left, scratch_rect.bottom - scratch_rect.top,
		pixels_dc, 0, 0, x_tiles, y_tiles, SRCCOPY);

	if(grid_on && pixel_size > 1){
	// Draw the gridlines
		SelectObject(hdc, lightgrayp);
		for(x = 0; x < x_tiles; x++){ // it's necessary to draw the 0th gridline, for consistent pixel size.
			MoveToEx(hdc, x * pixel_size, scratch_rect.top, NULL);
			LineTo(hdc, x * pixel_size, scratch_rect.bottom);
		}
		for(y = 0; y < y_tiles; y++){
			MoveToEx(hdc, scratch_rect.left, y * pixel_size, NULL);
			LineTo(hdc, scratch_rect.right, y * pixel_size);
		}
	}

	EndPaint(drawing_window_h, &ps);
}
