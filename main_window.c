#include <windows.h>
#include <stdio.h>
#include <string.h> // for adding file extensions to filenames
#include "main.h"
#include "textbox.h"
#include "brushes_etc.h"
#include "main_window.h"
#include "drawing_window.h"
#include "sidebar.h"

HWND hwndmain;
RECT non_sidebar_rect;

static WINDOWPLACEMENT main_wnd_pos;

static void initial_drawing(void);
static void cls(void);
static HWND make_main_window(void);
LRESULT CALLBACK main_wnd_proc(HWND, UINT, WPARAM, LPARAM);
static void character(short);
static void click(short, short);
static void paint_window(void);
static void save_as(void);
static void save_bmp(HANDLE);

int main_window_init(void)
{
	int main_width;

	create_brushes_etc();
	
	hwndmain = make_main_window();

	if(!hwndmain)
		return 0;
	
	toggle_pseudo_fullscreen();

	GetClientRect(hwndmain, &non_sidebar_rect);

	init_sidebar_window();
	main_width = make_sidebar_window(non_sidebar_rect);

	non_sidebar_rect.right = main_width;

	init_drawing_window();
	make_drawing_window();

	ShowWindow(hwndmain, SW_SHOW);

	return 1;
}

void toggle_pseudo_fullscreen(void)
{
	DWORD style;
	RECT r;

	style = GetWindowLong(hwndmain, GWL_STYLE);
	SetWindowLong(hwndmain, GWL_STYLE, style ^ WS_OVERLAPPEDWINDOW);

	if(style & WS_OVERLAPPEDWINDOW){ // going into fullscreen
		GetWindowPlacement(hwndmain, &main_wnd_pos);
		r = screen_size();
		SetWindowPos(hwndmain, HWND_TOP, r.left, r.top,
			r.right - r.left, r.bottom - r.top,
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}else{ // leaving fullscreen
		SetWindowPlacement(hwndmain, &main_wnd_pos);
		SetWindowPos(hwndmain, NULL, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

static HWND make_main_window(void)
{
	HWND the_window;
	WNDCLASS wc;

	wc.style = 0;
	wc.lpfnWndProc = main_wnd_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon((HINSTANCE) NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor((HINSTANCE) NULL, IDC_ARROW);
	wc.hbrBackground = GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName =  MAKEINTRESOURCE(2001);
	wc.lpszClassName = "MainWndClass";

	if(!RegisterClass(&wc))
		return FALSE;

	the_window = CreateWindowEx(0, "MainWndClass",
		"Bernard the main window", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,  (HWND) NULL,
		(HMENU) NULL, hInstance, (LPVOID) NULL);

	if(!the_window)
		return FALSE;

	SetWindowLong(the_window, GWL_USERDATA, (long)1234);

	return the_window;
}

LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_CREATE:
			return 0;

		case WM_PAINT:
			paint_window();
			return 0;

		/*
		case WM_SIZE:
			return 0;
			*/

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_CHAR:
			character(wParam);
			return 0;

		case WM_MOUSEWHEEL:
			mousewheelaccumulator += (short)HIWORD(wParam);
			if(mousewheelaccumulator < 120 && mousewheelaccumulator > -120)
				return 0;
			/*
			if(mousewheelaccumulator / 120 > 0)
			else
			*/
			mousewheelaccumulator = 0;
			return 0;

		case WM_LBUTTONDOWN:
			click(LOWORD(lParam), HIWORD(lParam));
			return 0;

		case WM_COMMAND:
			if(HIWORD(wParam) == 0 || HIWORD(wParam) == 1){ // from a menu or accelerator, not a control
				switch(LOWORD(wParam)){
				case 6001: // quit
					PostMessage(hwndmain, WM_DESTROY, 0, 0);
					break;
				case 6002: // image size
					if(DialogBox(hInstance, MAKEINTRESOURCE(1001), hwndmain, imgsize_dlgproc) == IDOK)
						fit(&non_sidebar_rect);
					break;
				case 6003: // save
					save_as();
					break;
				case 6004: // save as
					save_as();
					break;
				case 6005: // grid
					if(grid_on){
						CheckMenuItem(GetMenu(hwndmain), 6005, MF_UNCHECKED);
						grid_on = false;
					}else{
						CheckMenuItem(GetMenu(hwndmain), 6005, MF_CHECKED);
						grid_on = true;
					}
					GetClientRect(drawing_window_h, &scratch_rect);
					InvalidateRect(drawing_window_h, &scratch_rect, FALSE);
					break;
				}
			}
			return 0;
		
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

static void save_as(void)
{
	OPENFILENAME ofn;
	char filter[] = "Bitmap file (*.bmp)\0*.bmp\0";
	char name_buf[512]; // How big should it be?
	int name_len = 0; // actual length of returned string.
	bool ext_OK = false;
	char *extension = NULL;
	char ext_str[5], bmp_str[] = ".bmp";
	HANDLE fileh;

	name_buf[0] = '\0'; // default save name should go here really
	
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwndmain;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = name_buf;
	ofn.nMaxFile = 512; // "bytes, or characters (unicode version)"...
	ofn.lpstrFileTitle = NULL; // just filename and extension - useful
	//ofn.nMaxFileTitle
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt = NULL;

	// This is clearly the wrong way to add the file extension.
	// If you enter "filename" with no extension, and "filename.bmp" exists,
	// "filename.bmp" will be overwritten, without any "file exists, overwrite?" warning.
	// I guess I have to write a proc and put it in ofn.lpfnHook, to get this basic functionality.
	// Though the modern way is apparently to use IFileOpenDialog. Looking into it.

	if(GetSaveFileName(&ofn)){
		// check there's space in buffer for 4 more characters
		name_len = strlen(name_buf);
		if(name_len < 508){
			extension = strrchr(name_buf, '\0'); // extension now points to terminator of string
			if(extension){ // Checks the string is null-terminated. Always will be?
				if(name_len > 4){ // if long enough that it might have the extension already
					strcpy(ext_str, extension - 4);
					if(ext_str[0] == '.'){
						if(strcmp(strtolower(ext_str), ".bmp") == 0)
							ext_OK = true;
					}
				}
				if(!ext_OK)
					strcpy(extension, bmp_str);
			}
		}
		fileh = CreateFile(name_buf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		save_bmp(fileh);
		CloseHandle(fileh);
	}
}

static void save_bmp(HANDLE fileh)
{
	BITMAPFILEHEADER h1;
	BITMAPINFOHEADER h2;
	RGBQUAD *bits;

	unsigned long pixel_data_size = 0, bytes_written = 0;
	int line_padding = 0;
	unsigned long *orig;
	unsigned char *tofile;
	int x, y, i;

	//"each scan line must be padded with zeroes to end on a LONG data-type boundary"
	line_padding = (4 - ((x_tiles * 3) % 4)) % 4;
	// uh, should I explain how that works?
	// OK, x_tiles * 3 is the bytes per line with no padding.
	// % 4 on that yields 0, 1, 2, or 3. This is the overspill beyond the 4-byte boundary.
	// So do 4 - overspill to find the padding needed.
	// but if there was 0 overspill, this yields 4 padding, when we really want 0.
	// So a final % 4 will turn a 4 into 0, while leaving a 1, 2 or 3 unaffected.

	pixel_data_size = (x_tiles * 3 + line_padding) * y_tiles;
	bits = malloc(pixel_data_size); // 3 bytes per pixel

	h1.bfType = 'MB'; // Bitmap file sig is just "BM". Reversed because endianness
	h1.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pixel_data_size;
	h1.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	h1.bfReserved1 = 0;
	h1.bfReserved2 = 0;

	h2 = pixels_pbmi->bmiHeader;
	h2.biBitCount = 24;
	h2.biSizeImage = pixel_data_size;

	orig = pixel_buffer;
	tofile = (unsigned char *)bits;
	for(y = 0; y < y_tiles; y++){
		// output one scanline
		for(x = 0; x < x_tiles; x++){
			*tofile++ = (*orig) & 0x000000FF;
			*tofile++ = ((*orig) & 0x0000FF00) >> 8;
			*tofile++ = ((*orig++) & 0x00FF0000) >> 16;
		}
		// pad it with as many zeroes as needed (if any)
		for(i = 0; i < line_padding; i++)
			*tofile++ = 0;
	}

	WriteFile(fileh, &h1, sizeof(BITMAPFILEHEADER), &bytes_written, NULL);
	WriteFile(fileh, &h2, sizeof(BITMAPINFOHEADER), &bytes_written, NULL);
	WriteFile(fileh, bits, pixel_data_size, &bytes_written, NULL);

	free(bits);
}

static void character(short c)
{
  switch(c)
  {
	case ' ':
		break;
	case 'i':
		//txblong(GetWindowLong(hwndmain, GWL_USERDATA));
		//txblong((long)defaultbmp);
		txbstring("<--"); txbnl();
		
		break;
	default:
		
		return;
  }
}

static void click(short x, short y)
{
	POINT p;
	p.x = x; p.y = y;

	//testing exceptions
	//p.x = x / 0;
}

BOOL APIENTRY imgsize_dlgproc(HWND window, UINT m, WPARAM wParam, LPARAM lParam)  
{
	unsigned short notification, ID;
	char numstring[21];
	int new_x, new_y;

	switch(m){

	case WM_INITDIALOG:
		snprintf(numstring, 21, "%d", x_tiles);
		SetDlgItemText(window, 4001, numstring);
		snprintf(numstring, 21, "%d", y_tiles);
		SetDlgItemText(window, 4002, numstring);
		snprintf(numstring, 21, "%d", magnify);
		SetDlgItemText(window, 4003, numstring);
		// set the focus to the first edit control
		SetFocus(GetDlgItem(window, 4001));
		SendDlgItemMessage(window, 4001, EM_SETSEL, 0, -1); // requires focus to work
		return FALSE;

	case WM_COMMAND:
		notification = HIWORD(wParam); // notification code 
		ID = LOWORD(wParam);         // item, control, or accelerator identifier
		if(notification == BN_CLICKED){
			switch(ID){
			case IDOK:
				GetDlgItemText(window, 4001, numstring, 21);
				new_x = atoi(numstring);
				GetDlgItemText(window, 4002, numstring, 21);
				new_y = atoi(numstring);
				pixels_bmp = resize_pixel_buffer(new_x, new_y);
				EndDialog(window, wParam); 
				return TRUE;
				case IDCANCEL:
				EndDialog(window, wParam); 
				return TRUE;
			}
		}
		return FALSE;

	default: 
	    return FALSE; 
	}
}

static void paint_window(void)
{
	HDC hdc;
	PAINTSTRUCT ps;

	// it's going to be completely covered by two child windows
	// so I don't need to do anything here really except for form's sake

	hdc = BeginPaint(hwndmain, &ps);
	EndPaint(hwndmain, &ps);
}

void main_window_cleanup(void)
{
	delete_brushes_etc();
   
	DestroyWindow(hwndmain); 
}
