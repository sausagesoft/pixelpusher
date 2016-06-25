extern HWND drawing_window_h;
extern int pixel_size, magnify;
extern int x_tiles, y_tiles;
extern unsigned long *pixel_buffer;
extern HBITMAP pixels_bmp;
extern PBITMAPINFO pixels_pbmi;
extern bool grid_on;

bool init_drawing_window(void);
bool make_drawing_window(void);
HBITMAP resize_pixel_buffer(int, int);
void zoom(int);
void fit(RECT *);
