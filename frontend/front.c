#include <sys/types.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DEL		("\x1B\x5B\x33\x7E")

#define CURup		("\x1B\x5B\x41")
#define CURdown		("\x1B\x5B\x42")
#define CURright	("\x1B\x5B\x43")
#define CURleft		("\x1B\x5B\x44")
#define CURsave		("\x1B\x37")
#define CURrestore	("\x1B\x38")
#define CURposition	("\x1B[6n")
#define CURhome		("\x1B\x4F\x48")
#define CURend		("\x1B\x4F\x46")
#define CLRtoEnd	("\x1B\x5B\x4B")
#define BACKspace	("\x1B\x5B\x31\x44")
#define CURhide		("\x1B[?25l")
#define CURshow		("\x1B[?25h")


#define ETX ((char)0x03)
#define EOT ((char)0x04)
#define ESC ((char)0x1B)
#define BS()		{fwrite( BACKspace, sizeof(BACKspace),1,stdout);}
#define CLRtoEND()	{fwrite( CLRtoEnd, sizeof(CLRtoEnd),1,stdout);}
#define saveCUR()	{fwrite( CURsave, sizeof(CURsave),1,stdout);}
#define restoreCUR()	{fwrite( CURrestore, sizeof(CURrestore),1,stdout);}
#define leftCUR()	{fwrite( CURleft, sizeof(CURleft),1,stdout);}
#define rightCUR()	{fwrite( CURright, sizeof(CURright),1,stdout);}
#define CtrlD EOT
#define CtrlC ETX

#define UTF8TRAILING(ch)	((((ch)>>6) & 0x0003) == 0x02)
#define UTF8LEADING(ch,no)	((ch & ((0xFF00 >> ((no)+1)) & 0x00FF) ) == ((0xFF00 >> (no)) & 0x00FF))

typedef struct _curP{
	int x ;
	int y ;
} curP ;
typedef struct _exchar{
	char ch;
	char color;
	unsigned char curwidth;
} exchar;

typedef struct _exexpr{
	struct _exexpr * parent ;
	struct _exexpr * child ;
	exchar *ech ;
	int position ;
	int len ;
	curP cur_start;
} exexpr ;

struct termios * original_ter;
struct termios * newter;
exexpr * cur_expr ;
curP win ;
pid_t child_pid;
void updatewin(){
	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	win.x = w.ws_col;
	win.y = w.ws_row ;
}
curP getCUR(){
	char buf[12];
	char *curposition = "\x1B[6n";
	int y,x,i,xp,yp,n;
	curP cur ;
	n = fwrite(curposition, strlen(curposition),1,stdout);
	for(i = 0;i < 12; i++){
		buf[i]=getchar();
		if(buf[i] == '['){
			yp = i + 1;
		}else if(buf[i] == ';'){
			buf[i] = '\0';
			xp = i + 1 ;
		}else if(buf[i] == 'R'){
			buf[i] = '\0';
			break;
		}
	}
	x = atoi(buf + xp) ;
	y = atoi(buf + yp) ;
	{cur.x=x;cur.y=y;};
	return cur;
}
curP initcurP(){
	updatewin();
	if(cur_expr->len == 0 ){
		return getCUR();
	}
}

void CURmoveleft(int x){
	char buf[12];
	char xch[5];
	strcpy(buf, "\x1B[");
	sprintf(xch, "%d", x);
	strcat(buf, xch);
	strcat(buf, "D");
	fwrite(buf, strlen(buf), 1, stdout);
}
void CURmoveright(int x){
	char buf[12];
	char xch[5];
	strcpy(buf, "\x1B[");
	sprintf(xch, "%d", x);
	strcat(buf, xch);
	strcat(buf, "C");
	fwrite(buf, strlen(buf), 1, stdout);
}
int CURmoveto(int x, int y){
	char buf[12];
	char xch[5];
	char ych[5];
	strcpy(buf, "\x1B[");
	sprintf(xch, "%d", x);
	sprintf(ych, "%d", y);
	strcat(buf, ych);
	strcat(buf, ";");
	strcat(buf, xch);
	strcat(buf, "H");
	fwrite( buf, strlen(buf),1,stdout);
}
int utf8bytes(char ch){
	unsigned char no = 2 ;
	if((ch & 0x80) == 0)return 1 ;
	while(no <= 6){
		if(UTF8LEADING(ch, no))return no ;
		no += 1;
	}
	return -1 ;
}

int home(){
	while(left(1) != -1);
}
int left(int no){
	exchar* curpoint ;
	unsigned char ch;
	if( cur_expr->len > cur_expr->position){
		cur_expr->position += 1;
		curpoint = cur_expr->ech + cur_expr->len - cur_expr->position;
		ch = curpoint->ch ;
		if(UTF8TRAILING(ch)){
			left(no+1);
		}else{
			if(curpoint->curwidth == 0){
				curpoint->curwidth = getwidth(curpoint);
			}
			CURmoveleft(curpoint->curwidth);
		}
		return 0 ;
	}else{
		return -1 ;
	}
}
int end(){
	while(right() != -1);
}
int right(){
	exchar* curpoint ;
	unsigned char ch = '\0';
	if( cur_expr->position > 0){
		curpoint = cur_expr->ech + cur_expr->len - cur_expr->position;
		ch = curpoint->ch ;
		cur_expr->position -= 1;
		if(UTF8TRAILING(ch)){
			right();
		}else{
			if(curpoint->curwidth == 0){
				curpoint->curwidth = getwidth(curpoint);
			}
			CURmoveright(curpoint->curwidth);
		}
		return 0;
	}else{
		return -1;
	}
}

int backspace(int no){
	unsigned char ch ;
	if( cur_expr->len > cur_expr->position){
		static exchar curbuf[8] ;
		exchar* curpoint ;
		exchar* src = cur_expr->ech + cur_expr->len - cur_expr->position;
		curbuf[8-no] = *(src - 1) ;
		ch = (src-1)->ch ;
		memmove(src - 1, src, (cur_expr->position+1) * sizeof(exchar)) ;
		cur_expr->len -= 1;
		cur_expr->ech = realloc(cur_expr->ech, (cur_expr->len+1)*sizeof(exchar));
		curpoint = curbuf + 8 - no ;
		if(UTF8TRAILING(ch)){
			backspace(no+1) ;
		}else{
			{
			//	CLRtoEND();
				/*
				if(cur_expr->position != 0){//error
					exchar* startpoint ;
					curP oldp, newp ;
					oldp = getCUR() ;
					startpoint = cur_expr->ech + cur_expr->len - cur_expr->position ;
	//				putchar('\n');putchar('\r');
					while(startpoint->ch != '\0'){
						putchar(startpoint->ch);
						startpoint = startpoint + 1;
					}
					CURmoveto(oldp.x, oldp.y);
				}	
				*/
			}
			if(curpoint->curwidth == 0){
				curpoint->curwidth = getwidth(curpoint);
			}
			int i;
			for(i = curpoint->curwidth;i>0;i--){
				BS();
			}
			CLRtoEND();
			
		}
	}else{
		return 0;
	}
	
	return 0;
}

int getwidth(exchar* utf8leading ){
	char buf[6];
	int i = 1;
	int32_t ucs;
	int width ;
	exchar* t;
	{
		buf[0] = utf8leading->ch;
		t = utf8leading + 1 ;
		while(UTF8TRAILING(t->ch )){
			buf[i] = t->ch;
			t = t + 1 ;
			i = i + 1 ;
		}
		buf[i] = '\0';
		utf8proc_iterate(buf, sizeof(buf), &ucs);
		width = mk_wcwidth(ucs); 
		return width; 
	}
}
int valid_char(char ch){
	curP p ;
	fwrite(&ch, 1,1,stdout);
	if(cur_expr->position == 0) {
		cur_expr->ech[cur_expr->len].ch = ch ;
		if((ch & 0x80) == 0){ //ASCII
			cur_expr->ech[cur_expr->len].curwidth = 1; 
		}else {
			cur_expr->ech[cur_expr->len].curwidth = 0;
		}
		cur_expr->len += 1;
		cur_expr->ech = realloc(cur_expr->ech, (cur_expr->len+1)*sizeof(exchar));
		cur_expr->ech[cur_expr->len].ch = '\0' ;
	}
}
int utf8_valid_char(char ch){
	int i ;
	switch(utf8bytes(ch)){
	case 1:
	valid_char(ch);
	break;
	
	case -1:
	fprintf(stderr, "unknown utf8 character\n");
	safe_exit();
	break;
	
	default:
	valid_char(ch);
	for(i = utf8bytes(ch);i>1;i--){
		valid_char(getchar());
	}
	break;
	}
}

int main(int argc, char** argv){
	char c;
	newter=(struct termios *)malloc(sizeof(struct termios)) ;
	original_ter=(struct termios *)malloc(sizeof(struct termios)) ;
	

	tcgetattr(STDIN_FILENO, original_ter);
	cfmakeraw(newter);
	//newter->c_lflag |=  ;
	tcsetattr(STDIN_FILENO, TCSANOW, newter);
//	tcsetattr(STDIN_FILENO, TCSANOW, original_ter);
	setvbuf(stdin,NULL,_IONBF,0);
	{
		cur_expr = (exexpr*)malloc(sizeof(exexpr));
		cur_expr->len = 0 ;
		cur_expr->position = 0 ;
		cur_expr->ech = (exchar*)malloc(sizeof(exchar));
		cur_expr->ech[0].ch = '\0';
	}

	int my_pipe[2];
	if(pipe(my_pipe) == -1)
	{
		fprintf(stderr, "Error creating pipe\n");
	}

	
	child_pid = fork();
	if(child_pid == -1)
	{
		fprintf(stderr, "Fork error\n");
	}
	if(child_pid == 0){ // child process
		dup2(my_pipe[0], STDIN_FILENO);
		dup2(my_pipe[1], STDOUT_FILENO);
		execlp("ikarus", "ikarus", "--quiet", NULL);
	}else{
		while(1){
			c=getchar();
			switch(c){
			case EOT:
			safe_exit();
			break;
			
			case '\x7F':
			backspace(1);
			break;
			
			case ESC:
			escape() ;
			break;		
			
			default:
			utf8_valid_char(c);
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
	kill(child_pid, SIGTERM);
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
		case '1':
		case '3':
		case '4':
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
			right();
		}
		break;

		case '\x44':
		if(chbuf[state-1] == '['){
			left (1);
		}
		break;

		case '\x46':
		if(chbuf[state-1] == 'O'){
			end ();
		}
		break;

		case '\x48':
		if(chbuf[state-1] == 'O'){
			home ();
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
		}else if(chbuf[state-1]=='4'&&chbuf[state-2]== '['){
			end();
		}else if(chbuf[state-1]=='1'&&chbuf[state-2]== '['){
			home();
		}
		break;
		}
	break;
	}
	chbuf[0]=chbuf[1]=chbuf[2]=chbuf[3]=state=0;
}
