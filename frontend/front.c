#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DEL		(\x1B\x5B\x33\x7E)

#define CURup		(\x1B\x5B\x41)
#define CURdown		(\x1B\x5B\x42)
#define CURright	(\x1B\x5B\x43)
#define CURLeft		(\x1B\x5B\x44)
#define CURhome		(\x1B\x4F\x48)
#define CURend		(\x1B\x4F\x46)
#define ETX ((char)0x03)
#define EOT ((char)0x04)
#define ESC ((char)0x1B)
#define BS()	{putchar(ESC);\
		putchar('[');\
		putchar('1');\
		putchar('D');}
#define CLRtoEND()	{putchar(ESC);\
			putchar('[');\
			putchar('K');}

#define CtrlD EOT
#define CtrlC ETX

typedef struct _exchar{
	char ch;
	char color;
} exchar;

typedef struct _exexpr{
	struct _exexpr * parent ;
	struct _exexpr * child ;
	exchar *ech ;
	int position ;
	int len ;
} exexpr ;

struct termios * original_ter;
struct termios * newter;
exexpr * cur_expr ;

int backspace(){
	if( cur_expr->len > cur_expr->position){
		exchar* src = cur_expr->ech + cur_expr->len - cur_expr->position;
		memmove(src - 1, src, (cur_expr->position+1) * sizeof(exchar)) ;
		cur_expr->len -= 1;
		cur_expr->ech = realloc(cur_expr->ech, (cur_expr->len+1)*sizeof(exchar));
		BS();
		CLRtoEND();
		if(cur_expr->position != 0){
		}
	}
}
int valid_char(char ch){
	if(cur_expr->position == 0) {
		cur_expr->ech[cur_expr->len].ch = ch ;
		cur_expr->len += 1;
		cur_expr->ech = realloc(cur_expr->ech, (cur_expr->len+1)*sizeof(exchar));
		cur_expr->ech[cur_expr->len].ch = '\0' ;
	}
}
int main(int argc, char** argv){
	char c;
	newter=(struct termios *)malloc(sizeof(struct termios)) ;
	original_ter=(struct termios *)malloc(sizeof(struct termios)) ;
	{
		cur_expr = (exexpr*)malloc(sizeof(exexpr));
		cur_expr->len = 0 ;
		cur_expr->position = 0 ;
		cur_expr->ech = (exchar*)malloc(sizeof(exchar));
		cur_expr->ech[0].ch = '\0';
	}

	tcgetattr(STDIN_FILENO, original_ter);
	cfmakeraw(newter);
	//newter->c_lflag |=  ;
	tcsetattr(STDIN_FILENO, TCSANOW, newter);
//	tcsetattr(STDIN_FILENO, TCSANOW, original_ter);
	setvbuf(stdin,NULL,_IONBF,0);
	

	int my_pipe[2];
	if(pipe(my_pipe) == -1)
	{
		fprintf(stderr, "Error creating pipe\n");
	}

	
	pid_t child_pid;
	child_pid = fork();
	if(child_pid == -1)
	{
		fprintf(stderr, "Fork error\n");
	}
	if(child_pid == 0){ // child process
	}else{
		while(1){
			c=getchar();
			switch(c){
			case EOT:
			safe_exit();
			break;
			
			case '\x7F':
			backspace();
			break;
			
			case ESC:
			escape() ;
			break;		
			
			default:
			valid_char(c);
			printf("%x ", c);
			break;
			}
		}
	}
}
int safe_exit(){
	exexpr *temp1 ;
	exexpr *temp2 ;
	temp1 = cur_expr->parent;
	while(temp1 != NULL){
		temp2 = temp1 ;
		temp1 = temp1->parent ;
		free(temp2->ech );
		free(temp2);
	}
	temp1 = cur_expr->child;
	while(temp1 != NULL){
		temp2 = temp1 ;
		temp1 = temp1->child ;
		free(temp2->ech );
		free(temp2);
	}
	free(cur_expr);
	
	tcsetattr(STDIN_FILENO, TCSANOW, original_ter);
	free(original_ter);
	free(newter);
	exit(0);
}
int escape(){
	static int state = 0;
	static char chbuf[4]={'\0','\0','\0','\0'};
	char c = getchar() ;
	switch(state){
	case 0:
		switch(c){
		case '[':
		chbuf[state] = c ;
		state = 1;
		escape();
		break;
		
		case 'O':
		chbuf[state] = c ;
		state = 1;
		escape();
		break;
		}
	break;

	case 1:
		switch(c){
		case '3':
		chbuf[state]= c ;
		state = 2;
		escape();
		break;

		case '\x41':
		if(chbuf[state-1] == '['){
//			up ();
			printf("up ");
		}
		break;
		
		case '\x42':
		if(chbuf[state-1] == '['){
//			down ();
			printf("down ");
		}
		break;
		
		case '\x43':
		if(chbuf[state-1] == '['){
//			right ();
			printf("right ");
		}
		break;
		
		case '\x44':
		if(chbuf[state-1] == '['){
//			left ();
			printf("left ");
		}
		break;
		
		case '\x46':
		if(chbuf[state-1] == 'O'){
//			end ();
			printf("end ");
		}
		break;

		case '\x48':
		if(chbuf[state-1] == 'O'){
//			end ();
			printf("home ");
		}
		break;
		}
	break;

	case 2:
		switch(c){
		case '~':
		if(chbuf[state-1] == '3' && chbuf[state-2] == '['){
//			delete() ;
			printf("del ");
		}
		break;
		}
	break;
	}
	chbuf[0]=chbuf[1]=chbuf[2]=chbuf[3]=state=0;
}
