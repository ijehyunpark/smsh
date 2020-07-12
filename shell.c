#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include <string.h>

#define BUFFER_SIZE 1025
#define ARGV_SIZE 1024
#define BUILTIN_SIZE 3
#define FLAG_SIZE 4
#define PIPE_SIZE 1024
#define FILE_MODE 0644

#define CONSTRUCTION2				\
*ptr_b = '\0';					\
if(ptr_f != ptr_b){				\
    if(red_mode > 0){				\
        file[ptr_file] = ptr_f;			\
        file_mod_list[ptr_file++] = red_mode;	\
        red_mode = -1;				\
    }						\
    else					\
        myargv[ptr_argv++] = ptr_f;		\
}						\
ptr_f = ptr_b + 1;


#define CONSTRUCTION(x) 						\
int temp = red_mode; 							\
red_mode = x; 								\
if(ptr_f != ptr_b && temp < 1){ 					\
    *ptr_b = '\0';							\
    myargv[ptr_argv++] = ptr_f; 					\
} 									\
else if(temp > 0){							\
    buffer_clear("syntax error: mulitple redirection", ptr_b);		\
    return;								\
}									\
else if(ptr_argv == 0){    						\
    buffer_clear("syntax error: no command for redirection", ptr_b);	\
    return;								\
}									\
*ptr_b = '\0';								\
ptr_f = ptr_b + 1;

void execute(int amp); // execute cmd by end token (;&) using fork before exec
void cmdmak(); // load history trace 
void cmdtoken(); // divide cmd by token

/*builtin cmd */
int cmd_history(); 
int cmd_cd();
int cmd_set();

int backtrace(char *ptr_b); // history trace cmd like !n 

struct history{
    char *data;
    //struct history *prev;
    struct history *next;
};
struct history *base;

struct builtin{
    char* cmd;
    int (*func)(int ptr_argv, char **myargv);
};
struct builtin list[BUILTIN_SIZE] =
{
    {"history", cmd_history},
    {"cd", cmd_cd},
    {"set", cmd_set}
};

int file_flag[FLAG_SIZE] =
{
    0,
    O_WRONLY | O_CREAT | O_TRUNC,
    O_WRONLY | O_CREAT | O_APPEND,
    O_RDONLY
};

/* cmd struct (ignore struct name) */
struct pipe_save{
    char *myargv[ARGV_SIZE]; // argv
    char *file[BUFFER_SIZE]; // file argv
    int file_mod_list[BUFFER_SIZE]; // file redirection mode
    int ptr_argv; // argc
    int ptr_file; // file argc
    int red_mode; // redirection mode
  //int ifpipe    // pipe bit means "pipe_save | pipe_save -> next" (Implementation in different forms, To be implemented)
    int ifgroup;  // group bit if true myargv[0] is group cmd
    struct pipe_save *next;
};
struct pipe_save *head;

int sub_execute(struct pipe_save *world); // real exec

char buffer[BUFFER_SIZE]; // input
char curdir[BUFFER_SIZE];
int noclobber = 1;
int ifpipe;
int fd_stdout = STDOUT_FILENO;
int fd_stdin = STDIN_FILENO;

int bfatal(char *msg){
    printf("%s\n",msg);
    return 1;
}
int fatal(char *msg){
    perror(msg);
    return 1;
}
void buffer_clear(char *msg,char *ptr){
    printf("%s\n",msg);
    while(*ptr != '\0'){
	if(*ptr == ';' || *ptr == '&')
	    break;
	ptr++;
    }
    strcpy(buffer,ptr + 1);
}

int main(){
    /* history,cmd link-list head mounted in memory */
    base = malloc(sizeof(struct history));
    head = malloc(sizeof(struct pipe_save));
    while(1){
	/* get shell command input */
	getcwd(curdir,BUFFER_SIZE);
	printf("[MYSHELL]%s$",curdir);
	bzero(buffer,BUFFER_SIZE);
	fgets(buffer,BUFFER_SIZE - 1, stdin);

	/* load history trace !n */
	buffer[strlen(buffer) -1] = '\0';
	cmdmak();
	
	/* adding history */
	struct history *cmd = malloc(sizeof(struct history));
	if(*(buffer) != '\0'){
	    cmd->data = malloc(sizeof(char) * BUFFER_SIZE);
            strcpy(cmd->data,buffer);
	    struct history *his_ptr = base;
	    while(his_ptr -> next != NULL)
		his_ptr = his_ptr -> next;
	    his_ptr -> next = cmd;
	    //cmd -> prev = his_ptr;  // not used
	}

	/* start interpret buffer */
	buffer[strlen(buffer)] = ';';
        while(*buffer != '\0')
            cmdtoken();
    }
    return 0;
}

void cmdmak(){
    char *ptr = buffer;
    buffer[strlen(buffer)] =';';
    while(*ptr != '\0'){
        if(*ptr == '!' && backtrace(ptr)){
	    bzero(buffer,BUFFER_SIZE);
	    return;
	}
        ptr++;
    }
    buffer[strlen(buffer) -1] ='\0';
}

void cmdtoken(){
    char *myargv[ARGV_SIZE];
    char *file[BUFFER_SIZE];
    int file_mod_list[BUFFER_SIZE];
    int ptr_argv = 0;
    int ptr_file = 0;
    int red_mode = 0;
    int ifgroup = 0;
    int type = 0;
    int i;
    char *ptr_f = buffer;
    char *ptr_b = buffer;
    bzero(myargv,ARGV_SIZE);
    bzero(file,BUFFER_SIZE);
    bzero(file_mod_list,BUFFER_SIZE);
    ifpipe = 0;
    head -> next = NULL;
    while(*ptr_b != ';' && *ptr_b != '&'){
	while(*ptr_f == ' '){
	    *ptr_f = '\0';
	     ptr_f++;
	     ptr_b++;
	}
	if(*ptr_b == '\0'){ //impossible
	    buffer_clear("buffer is corruped", ptr_b);
	    return;
	}
	if(*ptr_b == ' '){
	    CONSTRUCTION2
	}
	if(*ptr_b == '>'){
	    if(*(ptr_b + 1) == '>'){
		*ptr_b++ ='\0';		
	        if(ptr_f == ptr_b - 1)
		    ptr_f++;
		CONSTRUCTION(2)
	    }
	    else if(*(ptr_b + 1) == '|'){
		*ptr_b++ ='\0';
	        if(ptr_f == ptr_b - 1)
		    ptr_f++;
		CONSTRUCTION(4)
	    }
	    else{
	        CONSTRUCTION(1)
	    }
	}

	if(*ptr_b == '<'){
	    CONSTRUCTION(3)
	}

	if(*ptr_b == '|'){
	    ifpipe++;
	    CONSTRUCTION(-1)
	    //start make cmd_struct(named pipe_save)
	    struct pipe_save *hello = malloc(sizeof(struct pipe_save));
	    for(i=0;i<ptr_argv;i++)
		hello->myargv[i] = myargv[i];
	    for(i=0;i<ptr_file;i++){
		hello->file[i] = file[i];
		hello->file_mod_list[i] = file_mod_list[i];
	    }
	    hello->ptr_argv = ptr_argv;
	    hello->ptr_file = ptr_file;
	    hello->red_mode = red_mode;
	    hello->ifgroup = ifgroup;
	    struct pipe_save *exp = head;
	    while(exp->next != NULL)
		exp = exp-> next;
	    exp -> next = hello;
	    //end make cmd_struct
	    //start init
	    bzero(myargv,ARGV_SIZE);
    	    bzero(file,BUFFER_SIZE);
    	    bzero(file_mod_list,BUFFER_SIZE);
    	    ptr_argv = 0;
    	    ptr_file = 0;
	    ifgroup = 0;
	    //end init
	}
        if(*ptr_b == '('){
	    if(*ptr_b != *ptr_f || ifgroup){
	        buffer_clear("syntax error: command group is not ended", ptr_b);
	        return;
	    }
	    ifgroup = 1;
	    *ptr_b++ = '\0';
	    ptr_f = ptr_b;
	    int group_count = 1;
	    while(1){
		if(*ptr_b == '\0'){
	            break;
		}
		if(*ptr_b == '(')
		    group_count++;
		if(*ptr_b == ')')
		    group_count--;
		if(group_count == 0)
		    break;
		ptr_b++;
	    }
	    *ptr_b = '\0';
            myargv[ptr_argv++] = ptr_f;
	    ptr_f = ptr_b + 1;
	    if(group_count > 0){
	        buffer_clear("syntax error: command group is not closed", ptr_b);
	        return;
	    }
        }
	if(*ptr_b == ')'){
            buffer_clear("syntax error: command group is not opened", ptr_b);
	    return;
	}
	if(*ptr_b == ';' || *ptr_b == '&')
	    break;
	ptr_b++;
    }

    if(*ptr_b == ';')
        type = 0;
    else if(*ptr_b == '&')
        type = 1;
    CONSTRUCTION2
    if(ifgroup && ptr_argv > 1){
        buffer_clear("syntax error: group is not ended, use end token", ptr_b);
        return;
    }
    else if(ptr_argv != 0){
        //start make cmd_struct(named pipe_save)
	struct pipe_save *hello = malloc(sizeof(struct pipe_save));
	for(i=0;i<ptr_argv;i++)
	    hello->myargv[i] = myargv[i];
	for(i=0;i<ptr_file;i++){
	    hello->file[i] = file[i];
	    hello->file_mod_list[i] = file_mod_list[i];
	}
	hello->ptr_argv = ptr_argv;
	hello->ptr_file = ptr_file;
	hello->red_mode = red_mode;
	hello->ifgroup = ifgroup;
	struct pipe_save *exp = head;
	while(exp->next != NULL)
	    exp = exp-> next;
	exp -> next = hello;
        //end make cmd_struct
	//skip init(state : fin)
        execute(type);
    }
    else if(ifpipe > 0){
	buffer_clear("syntax error: no finish command for pipe", ptr_b);
	return;
    }
    strcpy(buffer,ptr_b + 1);
}

void execute(int amp){
    int pid;
    int status;
    int i;
    struct pipe_save *exp = head -> next;

    /* this thread should change parent process. (not use IPC) */
    if(amp == 0 && !exp->ifgroup){
        if(strcmp(exp -> myargv[0],"cd") == 0){
	    list[1].func(exp -> ptr_argv,exp -> myargv);
	    return;
        }
        if(strcmp(exp -> myargv[0],"set") == 0){
	    list[2].func(exp -> ptr_argv,exp -> myargv);
	    return;
        }
    }

    if((pid = fork()) < 0)
	fatal("fork error");
    else if(pid == 0){
	if(ifpipe > 0){ // pipe execution
	    int pid2;
	    int status2;
	    int fd[PIPE_SIZE];
	    int pipecount = 0;
	    for(i = 0;i<ifpipe;i++)
	        if(pipe(fd + i*2) < 0)
		    exit(fatal("pipe error"));
	    while(exp != NULL){
		if((pid2 = fork()) < 0)
		    exit(fatal("fork error"));
		else if(pid2 == 0){
		    if(pipecount != 0){
		        close(fd_stdin);
		        if(dup(fd[2*(pipecount-1)]) < 0)
			    exit(fatal("dup error"));
		    }
		    if(exp -> next != NULL){
		        close(fd_stdout);
		        if(dup(fd[2*pipecount+1]) < 0)
			    exit(fatal("dup error"));
		    }
	            for(i = 0;i<ifpipe*2;i++)
		        close(fd[i]);
		    exit(sub_execute(exp));
		}
		pipecount++;
	        exp = exp -> next;
	    }    
	    for(i = 0;i<ifpipe*2;i++)
		close(fd[i]);
	    for(i=0;i<ifpipe+1;i++)
		waitpid(-1,&status2,0);
	    exit(0);
	}
	else
	    exit(sub_execute(exp));
    }
    else{
	if(amp)
	    waitpid(pid,&status,WNOHANG);
	else
	    waitpid(pid,&status,0);
    }
}

int sub_execute(struct pipe_save *world){
    char **myargv = world -> myargv;
    char **file = world -> file;
    int *file_mod_list = world -> file_mod_list;
    int ptr_argv = world -> ptr_argv;
    int ptr_file = world -> ptr_file;
    int red_mode = world -> red_mode;
    int ifgroup = world -> ifgroup;
    int red_fd, i;
    if(red_mode > 0){
	return bfatal("syntax error");
    }
    if(red_mode == -1){ // redirection execution
        for(i=0;i<ptr_file;i++){
	    if(file_mod_list[i] == 1 && noclobber)
	        return bfatal("cannot overwrite exsting file");
	    if(file_mod_list[i] < 3 || file_mod_list[i] == 4){
	        int temp = file_mod_list[i] == 4 ? 1 : file_mod_list[i];
                if((red_fd = open(file[i], file_flag[temp], FILE_MODE)) < 0)
	            return fatal("creat error");
	        close(fd_stdout);
	        if(dup(red_fd) < 0)
		    return fatal("dup error");
	        close(red_fd);
	    }
	    else if(file_mod_list[i] == 3){
	        if((red_fd = open(file[i], file_flag[file_mod_list[i]], FILE_MODE)) < 0)
		    return fatal("open error");
	        close(fd_stdin);
	        if(dup(red_fd) < 0)
		    return fatal("dup error");
	        close(red_fd);
	    }
        }
    }
    if(ifgroup){ // group execution
	int pid;
	int status;
	if((pid = fork()) < 0)
	    return fatal("fork error");
	else if(pid == 0){
	    strcpy(buffer,myargv[0]);
	    buffer[strlen(buffer)+1] = '\0';
	    buffer[strlen(buffer)] = ';';
	    while(*buffer != '\0')
	        cmdtoken();
	    exit(0);
	}
	waitpid(pid,&status,0);
	return 0;
    }
    for(i=0;i<BUILTIN_SIZE;i++) // builtin_cmd execution
        if(strcmp(myargv[0],list[i].cmd) == 0)
	    return list[i].func(ptr_argv, myargv);
    execvp(myargv[0],myargv); //exec execution
    return bfatal("command is not found");
}

int cmd_history(int ptr_argv,char **myargv){
    int n = 1;
    struct history *his_ptr = base -> next;
    if(ptr_argv > 1)
        return bfatal("history: invalid option");
    while(his_ptr != NULL){
	printf("%d:%s\n",n++, his_ptr -> data);
	his_ptr = his_ptr -> next;
    }
    return 0;
}

int cmd_cd(int ptr_argv,char **myargv){
    if(ptr_argv == 1){
        if(chdir(getenv("HOME")))
	    return bfatal("cd: enviroment variable \"HOME=VALUE\" is uncorrect");
    }
    else if(ptr_argv == 2){
	if(chdir(myargv[1]))
	    return bfatal("cd: No such file or directory");
    }
    else
	return bfatal("cd: too many arguments");
    return 0;
}

int cmd_set(int ptr_argv,char **myargv){
    if(ptr_argv == 2){
        if(strcmp(myargv[1],"+C") == 0)
	    noclobber = 0;
	else if(strcmp(myargv[1],"-C") == 0)
	    noclobber = 1;
        else
	    return bfatal("set: [USAGE]: set [+-C]");
    }
    else if(ptr_argv == 3 && strcmp(myargv[2],"noclobber") == 0){
        if(strcmp(myargv[1], "+o") == 0)
	    noclobber = 0;
	else if(strcmp(myargv[1], "-o") == 0)
	    noclobber = 1;
	else
	    return bfatal("set: [USAGE]: set [+-o] noclobber");
    }
    else
	return bfatal("set: wrong format / only support noclobber option");
    return 0;
}

int backtrace(char *ptr){
    char num[BUFFER_SIZE];
    int num_ptr = 0;
    int number = 0;
    int n = 1;
    struct history *his_ptr = base;
    *ptr++ = '\0';
    bzero(num,BUFFER_SIZE);
    while(*ptr >= '0' && *ptr <= '9')
        num[num_ptr++] = *ptr++;
    if((number = atoi(num)) <= 0)
	return fatal("ERROR(super number or not number?)");
    while(his_ptr -> next != NULL){
	if(number == n++){
	    char temp[BUFFER_SIZE];
	    strcpy(temp,ptr);
	    if(strlen(buffer) + strlen((his_ptr -> next) -> data) + strlen(temp) > BUFFER_SIZE - 2)
		return bfatal("BUFFER OVERFLOW");
	    sprintf(buffer,"%s%s%s",buffer,(his_ptr -> next) -> data, temp);
            return 0;
	}
	his_ptr = his_ptr -> next;
    }
    return bfatal("no history about that number!");
}
