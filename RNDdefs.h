#define HALFRAND (RAND_MAX / 2)
// Putting that in because it ought to be a faster way to get a random yes/no.
// Really, I should write my own fast RND function. With less % and fewer operations.
// Maybe have a version where you set the range in advance, then use repeatedly.
#define RND(range)	((rand()>>16)%range)
// So RND(1) returns 0 every time.
// Note that RND(0) causes a divide by 0 (actually, % 0) error.
// (The >> 16 is to prevent odd-even patterns or something.)
