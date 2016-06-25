RECT screen_size(void);
HBITMAP DIB_maker(unsigned long **, PBITMAPINFO *, HDC *, int, int);
COLORREF RGB_to_BGR(unsigned long);
void to_hex(char *, char *, unsigned char);
unsigned char from_hex(char, char);
char *strtolower(char *);

extern HINSTANCE hInstance;
extern HACCEL acc_table;
extern long mousewheelaccumulator;
extern HPEN defaultp;
extern HBRUSH defaultb;
extern HFONT default_font;
extern RECT scratch_rect;

#define pi2 6.283185307179586476
#define pi 3.141592653589793238
#define halfpi 1.570796326794896619
#define RND(range)	((rand()>>16)%range)
#define EQUITRIHEIGHT 0.866025

typedef struct{
	float x;
	float y;
} vertex;
