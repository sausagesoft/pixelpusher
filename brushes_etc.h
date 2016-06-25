extern HPEN redp, lightgrayp;
extern HBRUSH redb, greenb, blueb, pinkb, bgdb, spareb, grayb, dkgrayb, defaultb;
extern COLORREF hiforecol, hibackcol;

void create_brushes_etc(void);
void delete_brushes_etc(void);
COLORREF grey_colorref(int);
COLORREF intercol(COLORREF, COLORREF, float);
