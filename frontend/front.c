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
#define CURposition	("\x1B\x5B\x36\x6E")
#define CURhome		("\x1B\x4F\x48")
#define CURend		("\x1B\x4F\x46")
#define CLRtoEnd	("\x1B\x5B\x4B")
#define BACKspace	("\x1B\x5B\x31\x44")
#define CURhide		("\x1B[?25l")
#define CURshow		("\x1B[?25h")
#define noLINEwrap	("\x1B[7l")


#define ETX ((char)0x03)
#define EOT ((char)0x04)
#define ESC ((char)0x1B)
#define BS()		{write(1, BACKspace, sizeof(BACKspace));}
#define CLRtoEND()	{write(1, CLRtoEnd, sizeof(CLRtoEnd));}
#define saveCUR()	{write(1, CURsave, sizeof(CURsave));}
#define restoreCUR()	{write(1, CURrestore, sizeof(CURrestore));}
#define leftCUR()	{write(1, CURleft, sizeof(CURleft));}
#define rightCUR()	{write(1, CURright, sizeof(CURright));}
#define offlineWRAP()	{write(1, noLINEwrap, sizeof(noLINEwrap));}
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
	curP cur_cur ;
} exexpr ;

struct termios * original_ter;
struct termios * newter;
exexpr * cur_expr ;
curP win ;
void updatewin(){
	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	win.x = w.ws_col;
	win.y = w.ws_row ;
}
curP getCUR(){
	char buf[12];
	int y,x,i,xp,yp;
	curP cur ;
	write(STDOUT_FILENO, CURposition, sizeof(CURposition));
	for(i = 0;i < 12; i++){
		read(STDIN_FILENO, buf + i, 1);
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
curP makecurP(){
	int i = 0;
	int acx = 0;
	int acy = 0;
	char ch ;
	curP new ;
	updatewin();
	
	while((ch = cur_expr->ech[i].ch ) != '\0' && i < cur_expr->len-cur_expr->position ){
		if((ch & 0x80) == 0){
			cur_expr->ech[i].curwidth = 1 ;
		}else if(UTF8TRAILING(ch)){
			cur_expr->ech[i].curwidth = 0 ;
		}else if(cur_expr->ech[i].curwidth == 0){
			cur_expr->ech[i].curwidth = getwidth(cur_expr->ech + i) ;
		}
		acx = acx + cur_expr->ech[i].curwidth;
		if(acx + cur_expr->cur_start.x - 1== win.x){
			acx = 0 ;
			if(acy + cur_expr->cur_start.y< win.y){
				acy = acy + 1;
			}
		}else if(acx + cur_expr->cur_start.x - 1> win.x){
			acx = cur_expr->ech[i].curwidth ;
			if(acy + cur_expr->cur_start.y < win.y){
				acy = acy + 1;
			}
		}
		i = i + 1 ;
	}
	if(acy == 0){
		new.x = acx + cur_expr->cur_start.x;
	}else{
		new.x = acx + 1;
	}
	new.y = acy + cur_expr->cur_start.y;
	return new;
	
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
	write(STDOUT_FILENO, buf, strlen(buf));
}
int utf8bytes(char ch){
	unsigned char no = 2 ;
	if((ch & 0x80) == 0)return 1 ;
	while(no <= 6){
		if(UTF8LEADING(ch, no))return no ;
		no += 1;
	}
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
/*
int moveCUR(curP des, curP src){
	char buf[12] ;

	int dx = des.x - src.x;
	int dy = des.y - src.y;
	//char dxch[5], dych[5];
//	strcpy(buf, "\x1B[");
	if(dx < 0){
		dx = -dx ;
		while(dx > 0){
			leftCUR();
			dx -= 1;
		}
	}else if(dx > 0){
		while(dx > 0){
			rightCUR();
			dx -= 1;
		}
	}else if(dx == 0){
	}
	if(dy == 0){
	}else if(dy < 0){
	}else if(dy > 0){
	}
}
*/
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
	printf("%c", ch);
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
		/*
		if(utf8bytes(ch) == 1){
			cur_expr->ech[cur_expr->len].curwidth = 1 ;
		}else{
			int bytes = utf8bytes(ch) ;
			while(bytes > 1){
				newch = getchar() ;
				cur_expr->ech[cur_expr->len].ch = ch ;
				cur_expr->len += 1;
				cur_expr->ech = realloc(cur_expr->ech, (cur_expr->len+1)*sizeof(exchar));
				cur_expr->ech[cur_expr->len].ch = '\0' ;
				bytes -= 1 ;
			}
		}
		*/
	}
//	fprintf(stderr, "len:%d ", cur_expr->len ); 
}
void updateCUR(){
	curP cur = cur_expr->cur_cur;
	curP now = getCUR();
	if(cur.x != now.x || cur.y != now.y){
		CURmoveto(cur.x, cur.y);
	}
}
int main(int argc, char** argv){
	char c;
	newter=(struct termios *)malloc(sizeof(struct termios)) ;
	original_ter=(struct termios *)malloc(sizeof(struct termios)) ;
	
	tcgetattr(STDIN_FILENO, original_ter);
	cfmakeraw(newter);
	//newter->c_lflag |= ICANON ;
	tcsetattr(STDIN_FILENO, TCSANOW, newter);
//	tcsetattr(STDIN_FILENO, TCSANOW, original_ter);
	setvbuf(stdin,NULL,_IONBF,0);
	
	updatewin();
	{
		cur_expr = (exexpr*)malloc(sizeof(exexpr));
		cur_expr->len = 0 ;
		cur_expr->position = 0 ;
		cur_expr->ech = (exchar*)malloc(sizeof(exchar));
		cur_expr->ech[0].ch = '\0';
		cur_expr->cur_start = initcurP() ;
		cur_expr->cur_cur = cur_expr->cur_start ;
	}


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
		dup2(my_pipe[0], STDIN_FILENO);
		dup2(my_pipe[1], STDOUT_FILENO);
		execlp("ikarus", "ikarus", "--quiet", NULL);
	}else{
		while(1){
		//	updateCUR();
			c=getchar();
			switch(c){
			case EOT:
			safe_exit();
			break;
			
			case '\x7F':
			backspace(1);
			cur_expr->cur_cur = makecurP();
			break;
			
			case ESC:
			escape() ;
			break;		
			
			default:
			valid_char(c);
			cur_expr->cur_cur = makecurP();
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
			cur_expr->cur_cur = makecurP();
		}
		break;
		
		case '\x44':
		if(chbuf[state-1] == '['){
			left (1);
			cur_expr->cur_cur = makecurP();
		}
		break;
		
		case '\x46':
		if(chbuf[state-1] == 'O'){
			end ();
			cur_expr->cur_cur = makecurP();
		}
		break;

		case '\x48':
		if(chbuf[state-1] == 'O'){
			home ();
			cur_expr->cur_cur = makecurP();
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
			cur_expr->cur_cur = makecurP();
		}else if(chbuf[state-1]=='1'&&chbuf[state-2]== '['){
			home();
			cur_expr->cur_cur = makecurP();
		}
		break;
		}
	break;
	}
	chbuf[0]=chbuf[1]=chbuf[2]=chbuf[3]=state=0;
}
