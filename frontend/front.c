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
#define CLRtoENDofLINE	("\x1B[\x4B")
#define CLRtoENDofSCRN	("\x1B[J")
#define BACKspace	("\x1B[\x31\x44")
#define CURhide		("\x1B[?25l")
#define CURshow		("\x1B[?25h")


#define ETX ((char)0x03)
#define EOT ((char)0x04)
#define ESC ((char)0x1B)
#define BS()		{fwrite( BACKspace, sizeof(BACKspace),1,stdout);}
#define saveCUR()	{fwrite( CURsave, sizeof(CURsave),1,stdout);}
#define restoreCUR()	{fwrite( CURrestore, sizeof(CURrestore),1,stdout);}
#define leftCUR()	{fwrite( CURleft, sizeof(CURleft),1,stdout);}
#define rightCUR()	{fwrite( CURright, sizeof(CURright),1,stdout);}
#define upCUR()		{fwrite( CURup, sizeof(CURup),1,stdout);}
#define CLR_to_end_of_line()	{fwrite( CLRtoENDofLINE, sizeof(CLRtoENDofLINE),1,stdout);}
#define CLR_to_end_of_scrn()	{fwrite( CLRtoENDofSCRN, sizeof(CLRtoENDofSCRN),1,stdout);}
#define CtrlD EOT
#define CtrlC ETX

#define UTF8TRAILING(ch)	((((ch)>>6) & 0x0003) == 0x02)
#define UTF8LEADING(ch,no)      (((ch) & ((0xFF00 >> ((no)+1)) & 0x00FF) ) == ((0xFF00 >> (no)) & 0x00FF))
#define ASCII(ch)		(((ch) & 0x80) == 0)

typedef struct _curP{
	int x ;
	int y ;
} curP ;
typedef struct _exchar{
	char ch;
	char color;
	unsigned char curwidth;
	unsigned int xoffset ;
	unsigned int yoffset ;
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

curP updatewin(){
	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	win.x = w.ws_col;
	win.y = w.ws_row ;
	return win ;
}
curP getCUR(){
	char *buf = NULL;
	char *curposition = "\x1B[6n";
	int y,x,i,xp,yp,ep,n,escaped = 0,braketed = 0,coloned = 0,xnumed = 0,ynumed = 0;
	curP cur ;
	n = fwrite(curposition, strlen(curposition),1,stdout);
	for(i = 0; ; i++){
		buf = realloc(buf, sizeof(i+1));
		buf[i]=getchar();
		if((xnumed && coloned && ynumed && braketed && escaped) == 1){
			if(buf[i] >= '0' && buf[i] <= '9'){
			}else if(buf[i] == 'R'){
				buf[i] = '\0';
				break ;
			}else {
				xnumed = coloned = ynumed = braketed = escaped = 0 ;
			}
		}else if((coloned && ynumed && braketed && escaped) == 1){
			if(buf[i] >= '0' && buf[i] <= '9'){
				xnumed = 1 ;
				xp = i ;
			}else {
				coloned = ynumed = braketed = escaped = 0 ;
			}
		}else if((ynumed && braketed && escaped) == 1){
			if(buf[i] >= '0' && buf[i] <= '9'){
			}else if(buf[i] == ';'){
				coloned = 1;
			}else {
				ynumed = braketed = escaped = 0 ;
			}
		}else if((braketed && escaped) == 1){
			if(buf[i] >= '0' && buf[i] <= '9'){
				ynumed = 1;
				yp = i ;
			}else{
				braketed = escaped = 0 ;
			}
		}else if( escaped == 1){
			if(buf[i] == '['){
				braketed = 1;
			}else{
				escaped = 0 ;
			}
		}else if(escaped == 0){
			if(buf[i] == '\x1B'){
				escaped = 1;
				ep = i ;
			}
		}
	}
	buf[xp - 1] = '\0' ;
	x = atoi(buf + xp) ;
	y = atoi(buf + yp) ;
	{cur.x=x;cur.y=y;};
	for(i = ep - 1; i >= 0; i -- ){
		ungetc(buf[i],stdin);
	}
	free(buf);
	return cur;
}
curP initcurP(){
	updatewin();
	if(cur_expr->len == 0 ){
		return getCUR();
	}
}

void CURmoveleftright(int x){
	char buf[12];
	char xch[5];
	char* direction ;
	if(x>0){
		direction = "C" ;
	}else if(x < 0){
		direction = "D" ;
		x = -x ;
	}else{
		return ;
	}
	strcpy(buf, "\x1B[");
	sprintf(xch, "%d", x);
	strcat(buf, xch);
	strcat(buf, direction);
	fwrite(buf, strlen(buf), 1, stdout);
}
void CURmoveupdown(int y){
	char buf[12];
	char ych[5];
	char* direction ;
	if(y>0){
		direction = "B" ;
	}else if(y < 0){
		direction = "A" ;
		y = -y ;
	}else{
		return ;
	}
	strcpy(buf, "\x1B[");
	sprintf(ych, "%d", y);
	strcat(buf, ych);
	strcat(buf, direction);
	fwrite(buf, strlen(buf), 1, stdout);
}
void CURmove(int dx, int dy){
	CURmoveleftright(dx);
	CURmoveupdown(dy);
}
void CURmoveup(int y){
	char buf[12];
	char ych[5];
	strcpy(buf, "\x1B[");
	sprintf(ych, "%d", y);
	strcat(buf, ych);
	strcat(buf, "A");
	fwrite(buf, strlen(buf), 1, stdout);
}

void CURmovedown(int y){
	char buf[12];
	char ych[5];
	strcpy(buf, "\x1B[");
	sprintf(ych, "%d", y);
	strcat(buf, ych);
	strcat(buf, "B");
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
void CURmoveleft(int x){
	char buf[12];
	char xch[5];
	strcpy(buf, "\x1B[");
	sprintf(xch, "%d", x);
	strcat(buf, xch);
	strcat(buf, "D");
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
size_t write_utf8(exchar* utf8leading, FILE* stream){
	char ch = utf8leading->ch ;
	char buf[7];
	register exchar* t = utf8leading + 1  ;
	register int i = 1;
	if(ASCII(ch)){
		return fwrite(&ch, 1, 1, stream);
	}
	buf[0] = utf8leading->ch ;
	while(UTF8TRAILING(t->ch )){
		buf[i] = t->ch;
		t = t + 1 ;
		i = i + 1 ;
	}
	buf[i] = '\0';
	if(i != utf8bytes(ch)){
		perror("\n\rwrite_utf8: illegal utf8 charater");
		safe_exit();
	}
	return fwrite(buf, i, 1, stream);
}
void updateEXPR(exchar* start, exchar* curpoint){
	exchar* tpoint = start ;
	exchar pointS = *curpoint;
	int dx,dy ;
	while(1){
		exchar* tlast ;
		if(tpoint != cur_expr->ech){
			for(tlast = tpoint - 1;UTF8TRAILING(tlast->ch); tlast --);
			if(updatewin().x - tlast->xoffset < 5){
				tpoint->xoffset = 0;
				tpoint->yoffset = tlast->yoffset + 1 ;
			}else{
				tpoint->xoffset = tlast->curwidth + tlast->xoffset;
				tpoint->yoffset = tlast->yoffset ;
			}
		}else{
			tpoint->xoffset = tpoint->yoffset = 0 ;
		}
		if(tpoint->ch == '\0'){	break;};
		for(tpoint = tpoint + 1; UTF8TRAILING(tpoint->ch); tpoint ++);
	}
	CURmove(start->xoffset-pointS.xoffset, start->yoffset-pointS.yoffset);
	CLR_to_end_of_scrn();
	tpoint = start ;
	while(1){
		exchar* tlast ;
		if(tpoint != cur_expr->ech){
			for(tlast = tpoint - 1;UTF8TRAILING(tlast->ch); tlast --);
			if(tpoint->yoffset>tlast->yoffset ){
				fwrite("\n\r", sizeof("\n\r"), 1, stdout);
			}
		}
		if(tpoint->ch != '\0'){
			write_utf8(tpoint, stdout);
		}else{
			break;
		}
		for(tpoint = tpoint + 1; UTF8TRAILING(tpoint->ch); tpoint ++);
	}
	dx = curpoint->xoffset - tpoint->xoffset ;
	dy = curpoint->yoffset - tpoint->yoffset ;
	CURmove(dx,dy);
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
			exchar* curnext;
			for(curnext = curpoint + 1; UTF8TRAILING(curnext->ch); curnext ++);
			CURmove(curpoint->xoffset-curnext->xoffset, curpoint->yoffset-curnext->yoffset);
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
		cur_expr->position -= 1;
		curpoint = cur_expr->ech + cur_expr->len - cur_expr->position;
		ch = curpoint->ch ;
		if(UTF8TRAILING(ch)){
			right();
		}else{
			exchar* curbefore;
			for(curbefore = curpoint - 1; UTF8TRAILING(curbefore->ch); curbefore --);
			CURmove(curpoint->xoffset-curbefore->xoffset, curpoint->yoffset-curbefore->yoffset);
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
		curpoint = cur_expr->ech + cur_expr->len - cur_expr->position;
		if(UTF8TRAILING(ch)){
			backspace(no+1) ;
		}else{
			updateEXPR(curpoint, curpoint);
		}
	}else{
		return 0;
	}
	
	return 0;
}
int utf8width(char * utf8leading){
	int width ,i;
	int32_t ucs;
	char buf[7] = {'\0'};
	strncpy(buf, utf8leading, sizeof(utf8bytes(*utf8leading)));
	for(i = 1;i <= utf8bytes(*utf8leading) - 1; i++){
		if(!UTF8TRAILING(buf[i])){
			perror("\n\rutf8width: not a legal UTF8 character");
			safe_exit();
		}
	}
	utf8proc_iterate(buf, sizeof(buf), &ucs);
	width = mk_wcwidth(ucs); 
	return width; 
}
int getwidth(exchar* utf8leading ){
	exchar* t;
	int i = 1;
	char buf[7];
	if(ASCII(utf8leading->ch)){return 1;}
	buf[0] = utf8leading->ch;
	t = utf8leading + 1 ;
	while(UTF8TRAILING(t->ch )){
		buf[i] = t->ch;
		t = t + 1 ;
		i = i + 1 ;
	}
	buf[i] = '\0';
	return utf8width(buf);
}


int valid_char(char ch){
	fwrite(&ch, 1,1,stdout);
	exchar* curpoint = cur_expr->ech + cur_expr->len - cur_expr->position;
	if(cur_expr->position == 0) {
		cur_expr->ech[cur_expr->len].ch = ch ;
		cur_expr->len += 1;
		cur_expr->ech = realloc(cur_expr->ech, (cur_expr->len+1)*sizeof(exchar));
		cur_expr->ech[cur_expr->len].ch = '\0' ;
	}else{
		cur_expr->len += 1;
		cur_expr->ech = realloc(cur_expr->ech, (cur_expr->len+1)*sizeof(exchar));
		memmove(curpoint + 1, curpoint ,sizeof(exchar)*(cur_expr->position+1));
		curpoint->ch = ch ;
		cur_expr->ech[cur_expr->len].ch = '\0' ;
	}
	if(UTF8TRAILING(ch)){
		curpoint->color = curpoint->curwidth = curpoint->xoffset = curpoint->yoffset = 0 ;
	}
}
int utf8_valid_char(char ch){
	exchar* tpoint ;
	exchar* curpoint ;
	int dx,dy ;
	int i ;
	static curP op ={-1,-1};
	curP p ;
	exchar *curbefore;
	switch(utf8bytes(ch)){
	case 1:
	valid_char(ch);
	break;
	
	case -1:
	perror("\n\rutf8_valid_char: unknown utf8 character\n");
	safe_exit();
	break;
	
	default:
	valid_char(ch);
	for(i = utf8bytes(ch);i>1;i--){
		valid_char(getchar());
	}
	break;
	}
	curbefore = cur_expr->ech + cur_expr->len - cur_expr->position - utf8bytes(ch);
	curbefore->curwidth = getwidth(curbefore) ; 
	tpoint = curbefore ;
	for(curpoint = curbefore + 1;UTF8TRAILING(curpoint->ch); curpoint ++);
	if(curbefore == cur_expr->ech){
		curbefore->xoffset = curbefore->yoffset = 0  ;
		tpoint = curpoint ;
	}{
		while(1){
			exchar* tlast ;
			for(tlast = tpoint - 1;UTF8TRAILING(tlast->ch); tlast --);
			if(updatewin().x - tlast->xoffset < 5){
				tpoint->xoffset = 0;
				tpoint->yoffset = tlast->yoffset + 1 ;
			}else{
				tpoint->xoffset = tlast->curwidth + tlast->xoffset;
				tpoint->yoffset = tlast->yoffset ;
			}
			if(tpoint >= curpoint){
				if(tpoint->yoffset > tlast->yoffset){
					fwrite("\n\r", sizeof("\n\r"), 1, stdout);
				}
				if(tpoint->ch != '\0'){
					write_utf8(tpoint, stdout);
				}
			}
			if(tpoint->ch == '\0'){break;};
			for(tpoint = tpoint + 1; UTF8TRAILING(tpoint->ch); tpoint ++);
		}
		dx = curpoint->xoffset - tpoint->xoffset ;
		dy = curpoint->yoffset - tpoint->yoffset ;
		CURmove(dx,dy);
	}
}

int main(int argc, char** argv){
	char c;
	newter=(struct termios *)malloc(sizeof(struct termios)) ;
	original_ter=(struct termios *)malloc(sizeof(struct termios)) ;
	

	tcgetattr(STDIN_FILENO, original_ter);
	cfmakeraw(newter);
	tcsetattr(STDIN_FILENO, TCSANOW, newter);
	setvbuf(stdin,NULL,_IONBF,0);
	{
		cur_expr = (exexpr*)malloc(sizeof(exexpr));
		cur_expr->len = 0 ;
		cur_expr->position = 0 ;
		cur_expr->ech = (exchar*)malloc(sizeof(exchar));
		cur_expr->ech[0].ch = '\0';
		cur_expr->cur_start = initcurP();
		printf("init successed x:%d y%d\n\r", cur_expr->cur_start.x  ,cur_expr->cur_start.y);
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
		default:
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
		default:
		
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
		default:
		break;
		}
	break;
	default:
	break;
	}
	chbuf[0]=chbuf[1]=chbuf[2]=chbuf[3]=state=0;
}
