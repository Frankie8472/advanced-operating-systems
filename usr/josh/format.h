 #ifndef JOSH_FORMAT_H
 #define JOSH_FORMAT_H

#define JF_ESC "\033"
#define JF_ST JF_ESC"["

#define JF_BOLD JF_ST"1m"
#define JF_RESET JF_ST"0m"

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

 #endif // JOSH_FORMAT_H
