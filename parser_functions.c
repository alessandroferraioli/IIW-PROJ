#include "basic.h"
#include "parser_functions.h"
#include "timer_functions.h"


void move_pointer(char**string,int n){//sposta un puntatore di n posizioni
    if(string==NULL || *string==NULL){
        handle_error_with_exit("error in move_pointer\n");
    }
    else if(n>0) {
        for (int i = 0; i < n; i++) {
            (*string)++;
        }
    }
    else if(n<0){
        for (int i = 0; i < n; i++) {
            (*string)--;
        }
    }
    return;
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int skip_space(char**string){//sposta un puntatore finquando trova caratteri uguali allo spazio
    if(*string==NULL || string==NULL){
        handle_error_with_exit("error in move_pointer\n");
    }
    int count=0;
    while(**string==' '){
        (*string)++;
        count++;
    }
    return count;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
char is_blank(char*string){//ritorna vero se la stringa è composta solo da spazi
    if(string==NULL){
        handle_error_with_exit("error in is_blank\n");
    }
    while(*string!='\0'){
        if(*string==' '){
            string++;
        }
        else{
            return 0;
        }
    }
    return 1;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int parse_integer_and_move(char**string) {//fa il parse dell'intero e sposta il puntatore al carattere dopo l'intero
    if(*string==NULL || string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    int value;
    errno = 0;
    value= (int) strtol(*string, &errptr, 0);
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ' && *errptr!='\n')) {
        handle_error_with_exit("invalid number\n");
    }
    *string=errptr;//sposta il puntatore
    return value;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

long parse_long_and_move(char**string) {
    if(*string==NULL || string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    long value;
    errno = 0;
    value=strtol(*string, &errptr, 0);
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ' && *errptr!='\n')) {
        handle_error_with_exit("invalid number\n");
    }
	*string=errptr;//sposta il puntatore
    return value;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
double parse_double_and_move(char**string){
    if(*string==NULL || string==NULL){
        handle_error_with_exit("error in parse_double\n");
    }
    char*errptr;
    double value;
    errno = 0;
    value=strtod(*string, &errptr);
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ' && *errptr!='\n')) {
        handle_error_with_exit("invalid number\n");
    }
    *string=errptr;//sposta il puntatore
    return value;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long parse_long(char*string) {//fa il parse per ottenere un long
    if(string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    long value;
    errno = 0;
    value= (int) strtol(string, &errptr, 0);
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ')) {
        handle_error_with_exit("invalid number\n");
    }
    return value;
}
int parse_integer(char*string) {
    if(string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    int value;//
    errno = 0;
    value= (int) strtol(string, &errptr, 0);
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ')) {
        handle_error_with_exit("invalid number\n");
    }
    return value;
}
double parse_double(char*string){
    if(string==NULL){
        handle_error_with_exit("error in parse_double\n");
    }
    char*errptr;
    double value;
    errno = 0;
    value=strtod(string, &errptr);
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ')) {
        handle_error_with_exit("invalid number\n");
    }
    return value;
}

void check_and_parse_command(char*command,char*filename){//verifica che i comandi che digita l'utente siano validi
    //e inizializza command e filename necessari per eseguire il comando
    if(command==NULL || filename==NULL){
        handle_error_with_exit("error in check and parse\n");
    }
    char*main_command,*temp_command;
    int moved=0;
    size_t lenght;
    temp_command=alloca(sizeof(char)*(MAXCOMMANDLINE+1));
    main_command=alloca(sizeof(char)*11);//8 local list+\0
    while(1){
        if(fgets(temp_command,MAXCOMMANDLINE,stdin)==NULL){//fgets aggiunge automaticamente
            // newline e il terminatore di stringa!
            handle_error_with_exit("error in read_line\n");
        }
        temp_command[strlen(temp_command)-1]='\0';
        moved=skip_space(&temp_command);
        if(strlen(temp_command)<4){
            printf(RED"Invalid command\n"RESET);
            temp_command=temp_command-moved;
            continue;
        }
        strncpy(main_command,temp_command,10);//copio in main_command i 4 byte del comando
        main_command[10]='\0';//aggiungo terminatore,probabilmente l'errore è qui
        if(strncmp(main_command,"list",4)==0){
            move_pointer(&temp_command,4);
            moved+=4;
            if(is_blank(temp_command)){
                better_strcpy(filename,"");
                better_strcpy(command,"list");
                break;
            }
            else{
                printf(RED"list doesn't allow parameters\n"RESET);
                temp_command=temp_command-moved;
                continue;
            }
        }
        else if(strncmp(main_command,"get ",4)==0){
            move_pointer(&temp_command,4);
            moved+=4;
            lenght=strlen(temp_command);
            if(lenght==0){
                printf(RED"Invalid filename\n"RESET);
                temp_command=temp_command-moved;
                continue;
            }
            else{
                better_strcpy(command,"get");
                better_strcpy(filename,temp_command);
                break;
            }
        }
        else if(strncmp(main_command,"put ",4)==0){
            move_pointer(&temp_command,4);
            moved+=4;
            lenght=strlen(temp_command);
            if(lenght==0){
                printf(RED"Invalid filename\n"RESET);
                temp_command=temp_command-moved;
                continue;
            }
            else{
                better_strcpy(command,"put");
                better_strcpy(filename,temp_command);
                break;
            }
        }
        else if(strncmp(main_command,"local list",10)==0){
            move_pointer(&temp_command,10);
            moved+=10;
            if(is_blank(temp_command)){
                better_strcpy(filename,"");
                better_strcpy(command,"local list");
                break;
            }
            else{
                printf(RED"local list doesn't allow parameters\n"RESET);
                temp_command=temp_command-moved;
                continue;
            }
        }
        else if(strncmp(main_command,"exit",4)==0){
            move_pointer(&temp_command,4);
            moved+=4;
            if(is_blank(temp_command)){
                better_strcpy(filename,"");
                better_strcpy(command,"exit");
                break;
            }
            else{
                printf(RED"exit doesn't allow parameters\n"RESET);
                temp_command=temp_command-moved;
                continue;
            }
        }
        temp_command=temp_command-moved;
        printf(RED"Invalid command\n"RESET);
    }
    return;
}
