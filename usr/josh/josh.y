%{
    #include <collections/array_list.h>
    #include "ast.h"
    extern struct josh_line *parsed_line;
%}


%union {
    int token;
    char *string;
    struct josh_command *command;
    struct josh_value *value;
    struct josh_line *line;
    struct josh_assignment *assignment;
}

%start line

%token <token> SEMICOLON
%token <token> DOUBLE_QUOT
%token <token> AT_SIGN
%token <token> EQUALS
%token <token> DOLLAR
%token <string> STRING
%type <command> part_command
%type <command> command
%type <value> value
%type <line> line
%type <assignment> assignment
%type <string> destination

%%

line:
    /* empty */ {
        parsed_line = NULL;
    }
    |
    command {
        parsed_line = malloc(sizeof(struct josh_line));
        parsed_line->type = JL_COMMAND;
        parsed_line->command = $1;
    }
    |
    assignment {
        parsed_line = malloc(sizeof(struct josh_line));
        parsed_line->type = JL_ASSIGNMENT;
        parsed_line->assignment = $1;
    }
    ;


command:
    part_command {
        $$ = $1;
    }
    |
    part_command SEMICOLON {
        $$ = $1;
    };

part_command:
    destination STRING {
        $$ = malloc(sizeof(struct josh_command));
        array_list_init(&$$->args, sizeof(struct josh_value *));
        $$->destination = $1;
        $$->cmd = $2;
    }
    |
    STRING {
        $$ = malloc(sizeof(struct josh_command));
        array_list_init(&$$->args, sizeof(struct josh_value *));
        $$->destination = NULL;
        $$->cmd = $1;
    }
    |
    part_command value {
        $$ = $1;
        array_list_append(&$$->args, &$2);
        //debug_printf("multi-string: %s\n", $2);
    }
    ;

assignment:
    STRING EQUALS value {
        $$ = malloc(sizeof(struct josh_assignment));
        $$->varname = $1;
        $$->value = $3;
    }
    ;

value:
    STRING {
        $$ = malloc(sizeof(struct josh_value));
        *$$ = (struct josh_value) {
            .type = JV_LITERAL,
            .val = $1
        };
    }
    |
    DOLLAR STRING {
        $$ = malloc(sizeof(struct josh_value));
        *$$ = (struct josh_value) {
            .type = JV_VARIABLE,
            .val = $2
        };
    }
    ;

destination:
    AT_SIGN STRING {
        $$ = $2;
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
