#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;

int debug = 1;

struct QUOTE_T {
    char name[16];
    int used;
    int sz;
    byte *opcodes;
};

typedef struct QUOTE_T QUOTE;
typedef struct QUOTE_T *QUOTE_PTR;

int dumpStacks(QUOTE_PTR, int);
void exec(QUOTE_PTR q);
int parseLine(char *, int);


struct NAMEDQ_Q {
    struct NAMEDQ_Q *prev;
    struct NAMEDQ_Q *next;
    QUOTE_PTR pQuote;
};

struct NAMEDQ_Q *firstWord = NULL;
struct NAMEDQ_Q *lastWord = NULL;


// execution stack
#define eSTK_SZ 256
byte eStk[eSTK_SZ+1]; int eSP = 0;
void ePush(byte op) { if (eSP < eSTK_SZ) eStk[eSP++] = op; }
byte ePop() { return (eSP > 0) ? eStk[--eSP] : 0; }

// integer stack
#define iSTK_SZ 256
long iStk[iSTK_SZ+1]; int iSP = 0;
void iPush(long op) { if (iSP < iSTK_SZ) iStk[iSP++] = op; }
long iPop() { return (iSP > 0) ? iStk[--iSP] : 0; }

// boolean stack
#define bSTK_SZ 256
char bStk[bSTK_SZ+1]; int bSP = 0;
void bPush(char op) { if (bSP < bSTK_SZ) bStk[bSP++] = op; }
char bPop() { return (bSP > 0) ? bStk[--bSP] : 0; }

// string stack
#define sSTK_SZ 256
char *sStk[sSTK_SZ+1]; int sSP = 0;
void sPush(char *op) { if (sSP < sSTK_SZ) sStk[sSP++] = op; }
char *sPop() { return (sSP > 0) ? sStk[--sSP] : 0; }

// quote stack
#define qSTK_SZ 256
QUOTE_PTR qStk[qSTK_SZ+1]; int qSP = 0;
void qPush(QUOTE_PTR op) { if (qSP < qSTK_SZ) qStk[qSP++] = op; }
QUOTE_PTR qPop() { return (qSP > 0) ? qStk[--qSP] : 0; }

// all quotes
#define qSTKALL_SZ 1024
QUOTE_PTR allQuotes[qSTKALL_SZ+1]; int qSPAll = 0;
void qPushAll(QUOTE_PTR op) { if (qSPAll < qSTKALL_SZ) allQuotes[qSPAll++] = op; }
QUOTE_PTR qPopALL() { return (qSPAll > 0) ? allQuotes[--qSPAll] : 0; }

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
QUOTE_PTR addQuote(int initialSize) {
    QUOTE_PTR q = (QUOTE_PTR)malloc(sizeof(QUOTE));
    q->sz = initialSize == 0 ? 32 : initialSize;
    strcpy(q->name, "-none-");
    q->used = 0;
    q->opcodes = (byte *)malloc(sizeof(byte) * q->sz);
    qPushAll(q);
    return q;
}

void dumpQuote(QUOTE_PTR q) {
    printf("\n%08lx (%s): %d, %d - ", q, q->name, q->sz, q->used);
    for (int i = 0; i < q->used; i++) printf("%d ", q->opcodes[i]);
}

void dumpQuotes() {
    printf("\nquotes: (%d)", qSPAll);
    for (int i = 0; i < qSPAll; i++) dumpQuote(allQuotes[i]);
}

void addOpToQuote(QUOTE_PTR q, byte op) {
    q->used++;
    if (q->sz < q->used)  {
        q->sz += 32;
        q->opcodes = (BYTE *)realloc(q->opcodes, q->sz * sizeof(byte));
    }
    q->opcodes[q->used-1] = op;
    // dumpQuote(q);
    return;
}

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
int noop(QUOTE_PTR q, int pc) { return pc; }
int iAdd(QUOTE_PTR q, int pc) { long x = iPop(), y = iPop(); iPush(y+x); return pc; } // printf("-%ld+%ld=%ld-", x, y, x+y); return pc; }
int iSub(QUOTE_PTR q, int pc) { long x = iPop(), y = iPop(); iPush(y-x); return pc; }
int iMul(QUOTE_PTR q, int pc) { long x = iPop(), y = iPop(); iPush(y*x); return pc; }
int iDiv(QUOTE_PTR q, int pc) { long x = iPop(), y = iPop(); iPush(y/x); return pc; }
int iMod(QUOTE_PTR q, int pc) { long x = iPop(), y = iPop(); iPush(y%x); return pc; }
int iEq(QUOTE_PTR q, int pc)  { long x = iPop(), y = iPop(); bPush(y==x); return pc; }
int iLT(QUOTE_PTR q, int pc)  { long x = iPop(), y = iPop(); bPush(y<x); return pc; }
int iGT(QUOTE_PTR q, int pc)  { long x = iPop(), y = iPop(); bPush(y>x); return pc; }
int iPrint(QUOTE_PTR q, int pc)  { printf("%ld ", iPop()); return pc; }
int iEmit(QUOTE_PTR q, int pc)  { printf("%c", (byte)iPop()); return pc; }
int iLit(QUOTE_PTR q, int pc)  {
    long x = q->opcodes[pc++];
    x = x + (q->opcodes[pc++] << 8);
    x = x + (q->opcodes[pc++] << 16);
    x = x + (q->opcodes[pc++] << 24);
    if (debug > 1) printf("-iLit: push(%ld)-", x);
    iPush(x);
    return pc;
}

int sAdd(QUOTE_PTR q, int pc) { char *x = sPop(), *y = sPop(); strcat(y, x); sPush(y); return pc; }
int sSub(QUOTE_PTR q, int pc) { long x = iPop(), y = iPop(); iPush(y-x); return pc; }
int sMul(QUOTE_PTR q, int pc) { long x = iPop(), y = iPop(); iPush(y*x); return pc; }
int sDiv(QUOTE_PTR q, int pc) { long x = iPop(), y = iPop(); iPush(y/x); return pc; }
int sEq(QUOTE_PTR q, int pc)  { long x = iPop(), y = iPop(); bPush(y==x); return pc; }
int sLT(QUOTE_PTR q, int pc)  { long x = iPop(), y = iPop(); bPush(y<x); return pc; }
int sGT(QUOTE_PTR q, int pc)  { long x = iPop(), y = iPop(); bPush(y>x); return pc; }
int sParse(QUOTE_PTR q, int pc)  { char *x = sPop(); parseLine(x, 0); free(x); return pc; }

int (*ops[])(QUOTE_PTR, int) = {
/*         0        1        2        3        4        5        6        7        8        9 */
/*   0 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/*  10 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/*  20 */ iAdd,    iSub,    iMul,    iDiv,    iMod,    iEq,     iLT,     iGT,     iPrint,  iEmit,
/*  30 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/*  40 */ noop,    noop,    noop,    noop,    iLit,    noop,    noop,    noop,    noop,    noop,    
/*  50 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/*  60 */ sAdd,    sSub,    sMul,    sDiv,    sEq,     iLT,     iGT,     noop,    noop,    noop,    
/*  70 */ sParse,  noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/*  80 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/*  90 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 100 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 110 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 120 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 130 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 140 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 150 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 160 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 170 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 180 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 190 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 200 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 210 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 220 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 230 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 240 */ noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    noop,    
/* 250 */ noop,    noop,    noop,    noop,    noop,    dumpStacks
};

void exec(QUOTE_PTR q) {
    if (debug > 1) { printf("exec() - "); dumpQuote(q); }
    int pc = 0;
    while (pc < q->used) {
        byte ir = q->opcodes[pc++];
        if (debug > 1) printf("-pc=%d, ir=%d-", pc-1, ir);
        pc = ops[ir](q, pc);
    }
}

byte isNumber(char *word) {
    long num = 0;
    int base = 10;
    char *cp = word;
    while (*cp) {
        char c = *(cp++);
        if (('0' <= c) && (c <= '9')) {
            num = (num * base) + (c-'0');
        } else {
            return 0;
        }
        
    }
    iPush(num);
    return 1;
}

QUOTE_PTR curr_q = NULL;
int parseWord(char *word, int state) {
    if (state == 256) return state;
    if (debug == 2) printf("[(%d)%s]", state, word);
    if (strcmp(word, "//") == 0) return 256;
    if (isNumber(word)) {
        long x = iPop();
        addOpToQuote(curr_q, 44);
        addOpToQuote(curr_q, (x>> 0)&0xff);
        addOpToQuote(curr_q, (x>> 8)&0xff);
        addOpToQuote(curr_q, (x>>16)&0xff);
        addOpToQuote(curr_q, (x>>14)&0xff);
        return state; }
    if (strcmp(word, "{") == 0) {
        qPush(curr_q);
        curr_q = addQuote(0);
    }
    if (strcmp(word, "}") == 0) {
        if (debug > 1) dumpQuote(curr_q);
        curr_q = qPop();
    }
    if (strcmp(word, "iAdd") == 0) { addOpToQuote(curr_q, 20); return state; }
    if (strcmp(word, "iMul") == 0) { addOpToQuote(curr_q, 22); return state; }
    if (strcmp(word, "iPrint") == 0) { addOpToQuote(curr_q, 28); return state; }
    if (strcmp(word, "iEmit") == 0) { addOpToQuote(curr_q, 29); return state; }
    if (strcmp(word, "dumpStacks") == 0) { addOpToQuote(curr_q, 255); return state; }

    return state;
}

int parseLine(char *line, int state) {
    char *cp = line;
    while (*cp) {
        char word[64] = {0};
        char *wp = word;
        while ((*cp) && (*cp < 33)) { cp++; }
        while (*(cp) > 32) { *(wp++) = *(cp++); *wp = 0; }
        state = parseWord(word, state);
    }
    state = (state == 256) ? 0 : state;
    return state;
}

void readProgram(FILE *fp)
{
    int state = 0;
    char buf[256];
    while (fgets(buf, 256, fp) == buf) {
        int l = strlen(buf);
        while (l && (buf[l-1] < 33)) buf[--l] = 0;
        char *x = malloc(strlen(buf)+1);
        strcpy(x, buf);
        sPush(x);
        sParse(0, 0);
    }
}

int dumpStacks(QUOTE_PTR q, int pc) {
    printf("\nint: (%d) ", iSP);
    for (int i = 0; i < iSP; i++) printf("%ld ", iStk[i]);

    printf("\nbool: (%d) ", bSP);
    for (int i = 0; i < bSP; i++) printf("%d ", bStk[i]);

    printf("\nstring: (%d) ", sSP);
    for (int i = 0; i < sSP; i++) printf("'%s' ", sStk[i]);

    printf("\nexec: (%d) ", eSP);
    for (int i = 0; i < eSP; i++) printf("%d ", eStk[i]);

    return pc;
}

void init_words() {
    // firstWord = (struct WORD_T *)malloc(sizeof(WORD));
    // firstWord->name = 0;
    // firstWord->pQuote = addQuote(0);
    // firstWord->next = 0;
    // firstWord->prev = 0;
    // lastWord = firstWord;
}

void parse_arg(char *arg)
{
    if (*arg == 'd') debug = 1;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
int main (int argc, char **argv)
{
	char fn[64]; strcpy(fn, "push.txt");
    init_words();

    for (int i = 1; i < argc; i++)
    {
        char *cp = argv[i];
        if (*cp == '-') parse_arg(++cp);
        else strcpy(fn, cp);
    }

    curr_q = addQuote(0);
    // the push program file ...
    FILE *fp = fopen(fn, "rt");
	if (fp) {
        readProgram(fp);
        fclose(fp);
    }

    if (debug > 0) dumpStacks(0, 0);
    exec(curr_q);
    if (debug > 0) dumpStacks(0, 0);
    if (debug > 0) dumpQuotes();

    return 0;
}
