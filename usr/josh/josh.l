%{
    #include "josh.tab.h"
%}

%%

[ \t\n]           ;
[a-zA-Z0-9_]+      { yylval.string = strdup(yytext); return STRING; }
%%