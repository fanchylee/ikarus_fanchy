#include <sys/types.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct _curP{
	int x ;
	int y ;
} curP ;

struct termios * original_ter;
struct termios * newter;
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
int main(){
	newter=(struct termios *)malloc(sizeof(struct termios)) ;
	original_ter=(struct termios *)malloc(sizeof(struct termios)) ;
	curP p ;
	

	tcgetattr(STDIN_FILENO, original_ter);
	cfmakeraw(newter);
	//newter->c_lflag |= ICANON ;
	tcsetattr(STDIN_FILENO, TCSANOW, newter);
//	tcsetattr(STDIN_FILENO, TCSANOW, original_ter);
	setvbuf(stdin,NULL,_IONBF,0);
	fwrite("weeeiewoeo", strlen("weeeiewoeo"), 1, stdout);
	p = getCUR() ;
	CURmoveto(1,p.y);
	getchar();
	safe_exit();
}
int safe_exit(){
	
	tcsetattr(STDIN_FILENO, TCSANOW, original_ter);
	free(original_ter);
	free(newter);

	exit(0);
}
