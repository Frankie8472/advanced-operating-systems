%{
    #include "josh.tab.h"
%}

%%

[ \t\n]           ;
";"                  return SEMICOLON;
[a-zA-Z0-9_]+      { yylval.string = strdup(yytext); return STRING; }
%%