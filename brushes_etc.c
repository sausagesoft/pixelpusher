#include <windows.h>
#include "brushes_etc.h"

HPEN redp, lightgrayp;
HBRUSH redb, greenb, blueb, pinkb, bgdb, grayb, dkgrayb, spareb;
COLORREF hiforecol, hibackcol;

void create_brushes_etc(void)
{
	redp = CreatePen(PS_SOLID, 1, 0x000000FF);
	lightgrayp = CreatePen(PS_SOLID, 1, 0x00D0D0D0);
	redb = CreateSolidBrush(0x000000FF);
	greenb = CreateSolidBrush(0x0000FF00);
	blueb = CreateSolidBrush(0x00FF0000);
	pinkb = CreateSolidBrush(0x00CCCCFF);
	bgdb = CreateSolidBrush(0x00F0F0F0);
	grayb = CreateSolidBrush(0x00D0D0D0);
	dkgrayb = CreateSolidBrush(0x00909090);

	hiforecol = GetSysColor(COLOR_HIGHLIGHTTEXT);
	hibackcol = GetSysColor(COLOR_HIGHLIGHT);

	spareb = NULL;
}

void delete_brushes_etc(void)
{
	DeleteObject(redp);
	DeleteObject(lightgrayp);
	DeleteObject(redb);
	DeleteObject(greenb);
	DeleteObject(blueb);
	DeleteObject(pinkb);
	DeleteObject(bgdb);
	DeleteObject(grayb);
	DeleteObject(dkgrayb);
	// spare brush "spareb" is created and deleted at time of use
}

COLORREF grey_colorref(int shade)
{
	return shade << 16 | shade << 8 | shade;
}

COLORREF intercol(COLORREF c1, COLORREF c2, float f)
{
	unsigned char r, g, b;

	r = GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * f;
	g = GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * f;
	b = GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * f;

	return RGB(r, g, b);
}
