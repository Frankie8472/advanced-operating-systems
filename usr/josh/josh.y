%{
    #include <collections/array_list.h>
    #include "ast.h"
    extern struct josh_command *parsed_line;
%}


%union {
    int token;
    char *string;
    struct josh_command *command;
}

%start command

%token <token> SEMICOLON
%token <token> DOUBLE_QUOT
%token <token> AT_SIGN
%token <token> EQUALS
%token <string> STRING
%type <command> part_command
%type <command> command
%type <string> destination

%%

command:
    {
        parsed_line = NULL;
    }
    |
    part_command {
        parsed_line = $1;
    }
    |
    part_command SEMICOLON {
        parsed_line = $1;
    };

part_command:
    destination STRING {
        $$ = malloc(sizeof(struct josh_command));
        array_list_init(&$$->args, sizeof(char *));
        $$->destination = $1;
        $$->cmd = $2;
        //debug_printf("string\n");
    }
    |
    STRING {
        $$ = malloc(sizeof(struct josh_command));
        array_list_init(&$$->args, sizeof(char *));
        $$->destination = NULL;
        $$->cmd = $1;
        //debug_printf("string\n");
    }
    |
    part_command STRING {
        $$ = $1;
        array_list_append(&$$->args, &$2);
        //debug_printf("multi-string: %s\n", $2);
    }
    ;

destination:
    AT_SIGN STRING {
        $$ = $2;
    }



%%

int yyerror(const char *s) {
    //debug_printf("error parsing line %s", s);
    return 1;
}

int yywrap(void) {
    return 1;
}
