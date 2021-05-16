%{
    #include <collections/array_list.h>
    #include "ast.h"
    extern struct josh_line *parsed_line;
%}


%union {
    int token;
    char *string;
    struct josh_line *line;
}

%start line

%token <token> SEMICOLON
%token <string> STRING
%type <line> part_line
%type <line> line

%%

line:
    {
        parsed_line = NULL;
    }
    |
    part_line {
        parsed_line = $1;
    }
    |
    part_line SEMICOLON {
        parsed_line = $1;
    };

part_line:
    STRING {
        struct josh_line *new_line = malloc(sizeof(struct josh_line));
        array_list_init(&new_line->args, sizeof(char *));
        new_line->cmd = $1;
        $$ = new_line;
        //debug_printf("string\n");
    }
    |
    part_line STRING {
        $$ = $1;
        array_list_append(&$$->args, &$2);
        //debug_printf("multi-string: %s\n", $2);
    }
    ;
%%

int yyerror(const char *s) {
    //debug_printf("error parsing line %s", s);
    return 1;
}

int yywrap(void) {
    return 1;
}
