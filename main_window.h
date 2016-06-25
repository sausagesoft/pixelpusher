int main_window_init(void);
void main_window_cleanup(void);
void toggle_pseudo_fullscreen(void);
BOOL APIENTRY imgsize_dlgproc(HWND, UINT, WPARAM, LPARAM);

extern HWND hwndmain;
extern RECT non_sidebar_rect;
