%{
#include <cstdio>
#include <iostream>
#include <fstream>
#include <set>

using namespace std;

#define DEBUG
#ifdef DEBUG
#define IFD(x) (x)
#else
#define IFD(x) 
#endif


struct Preproc {
    fstream out;

    set<string> ranges;
    set<string> sets;
};



/* The following is executed before each rule's action. */
#define YY_USER_ACTION \
    do { \
	; \
    } while (0);


/* ==============================================================
   ============================================================== */
%}


DIGIT		[0-9]
LowerCaseID	[_a-z][_a-zA-Z0-9]*
UpperCaseID	[A-Z][_a-zA-Z0-9]*

/* We don't want to take a standard yywrap() from fl.so, and so we can
   avoid linking the executable with -lfl. */

%option outfile="preproc.cpp"
%option reentrant stack noyywrap
%option extra-type="struct Preproc *"

%%

%{
  /* This code, which appears before the first rule, is copied 
     verbatim at the beginning of yylex(). At each yylex() 
     invocation, therefore, we mark the current last position as the
     start of the next token.  */
%}


range[ \t\r\n]+{UpperCaseID} {
    string s;
    int i = 5;

    IFD(cout << yytext << "\n");
    while (yytext[i] == ' ' || yytext[i] == '\t' || yytext[i] == '\r'
	    || yytext[i] == '\n') {
	i++;
    }
    yyextra->ranges.insert(&yytext[i]);
    s = yytext;
    s.insert(i, "$r");
    yyextra->out << s;
}

set[ \t]+{UpperCaseID} {
    string s;
    int i = 3;

    IFD(cout << yytext << "\n");
    while (yytext[i] == ' ' || yytext[i] == '\t' || yytext[i] == '\r'
	    || yytext[i] == '\n') {
	i++;
    }
    yyextra->sets.insert(&yytext[i]);
    s = yytext;
    s.insert(i, "$s");
    yyextra->out << s;
    yyextra->out << yytext;
}

{UpperCaseID} {
    string s = yytext;

    IFD(cout << "UpperCaseID " << yytext << "\n");
    if (yyextra->ranges.count(s)) {
	s.insert(0, "$r");
    } else if (yyextra->sets.count(s)) {
	s.insert(0, "$s");
    }
    yyextra->out << s;
}

.|\n {
    // TODO don't filter out
    yyextra->out << yytext;
}

%%

int main()
{
    const char * input_name = "input.fsp";
    FILE *fin = fopen(input_name, "r");
    struct Preproc p;
    yyscan_t scanner;

    if (!fin) {
	cerr << "Input error: Can't open " << input_name << "\n";
	return -1;
    }
    p.out.open("o.fsp", ios::out);

    yylex_init_extra(&p, &scanner);
    yyset_in(fin, scanner);
    yyset_extra(&p, scanner);
    yylex(scanner);
    yylex_destroy(scanner);

    p.out.close();
    fclose(fin);

    return 0;
}

