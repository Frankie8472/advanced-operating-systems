%{
    #include <collections/array_list.h>
    #include "ast.h"
    extern struct josh_line *parsed_line;
    struct josh_line *line_iter;
%}


%union {
    int token;
    char *string;
    struct josh_command *command;
    struct josh_value *value;
    struct josh_line *line;
    struct josh_assignment *assignment;
}

%start lines

%token <token> SEMICOLON
%token <token> DOUBLE_QUOT
%token <token> AT_SIGN
%token <token> EQUALS
%token <token> DOLLAR
%right <token> PIPE
%token <token> GREATER_THAN LESS_THAN
%token <token> VAR
%token <string> STRING
%type <command> command
%type <value> value
%type <line> line
%type <assignment> assignment
%type <string> destination


%%

lines:
    /* empty */ {
        parsed_line = NULL;
    }
    |
    lines line {
        if (parsed_line == NULL) {
            parsed_line = $2;
            line_iter = parsed_line;
        }
        else {
            line_iter->next = $2;
            line_iter = line_iter->next;
        }
    }
    |
    lines SEMICOLON line {
        if (parsed_line == NULL) {
            parsed_line = $3;
            line_iter = parsed_line;
        }
        else {
            line_iter->next = $3;
            line_iter = line_iter->next;
        }
    }
    ;

line:
    /* empty */ {
        $$ = NULL;
    }
    |
    command {
        $$ = malloc(sizeof(struct josh_line));
        $$->next = NULL;
        $$->type = JL_COMMAND;
        $$->command = $1;
    }
    |
    assignment {
        $$ = malloc(sizeof(struct josh_line));
        $$->next = NULL;
        $$->type = JL_ASSIGNMENT;
        $$->assignment = $1;
    }
    ;

command:
    destination STRING {
        $$ = malloc(sizeof(struct josh_command));
        array_list_init(&$$->args, sizeof(struct josh_value *));
        $$->destination = $1;
        $$->cmd = $2;
        $$->piped_into = NULL;
        $$->file_redir = NULL;
    }
    |
    STRING {
        $$ = malloc(sizeof(struct josh_command));
        array_list_init(&$$->args, sizeof(struct josh_value *));
        $$->destination = NULL;
        $$->cmd = $1;
        $$->piped_into = NULL;
        $$->file_redir = NULL;
    }
    |
    command value {
        $$ = $1;
        array_list_append(&$$->args, &$2);
        //debug_printf("multi-string: %s\n", $2);
    }
    |
    command PIPE command {
        $$ = $1;
        $$->piped_into = $3;
    }
    |
    command GREATER_THAN STRING {
        $$ = $1;
        if ($$->file_redir) {
            free($$->file_redir);
        }
        $$->file_redir = $3;
    }
    ;

assignment:
    STRING EQUALS value {
        $$ = malloc(sizeof(struct josh_assignment));
        $$->is_shell_var = false;
        $$->varname = $1;
        $$->value = $3;
    }
    |
    VAR STRING EQUALS value {
        $$ = malloc(sizeof(struct josh_assignment));
        $$->is_shell_var = true;
        $$->varname = $2;
        $$->value = $4;
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
