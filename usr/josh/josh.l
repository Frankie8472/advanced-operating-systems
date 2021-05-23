%option stack
%option 8bit
%option yylineno nounput noinput

%{
    #include "josh.tab.h"
%}

%x QUOT
%x COMMENT

%%

<QUOT>"\""        { yy_pop_state(); }
<QUOT>([^\"])+    { yylval.string = strdup(yytext); return STRING; }


<COMMENT>"\n"     { yy_pop_state(); }
<COMMENT>(\s|.)*  ;


"\""              { yy_push_state(QUOT); }
"#"               { yy_push_state(COMMENT); }


[ \t\n]           ;
";"               return SEMICOLON;
"@"               return AT_SIGN;
"="               return EQUALS;
([^ \t\n;@#\"=])+              { yylval.string = strdup(yytext); return STRING; }
%%
