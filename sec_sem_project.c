#include <stdio.h>
#include <unistd.h>
#include <termio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>

#define BUFSIZE 100
#define TRUE 1
#define FALSE 0

int getch();
int auto_fill(int index);                         // 자동 완성
int run_normal(char** cmd);                       // 일반 명령어 실행
void divide_cmd(char* input);                     // 명령어 분리
void run_ls_cmd();                                // ls 함수
void run_redirection(char** cmd, int position);   // 리다이렉션 포함 명령어 실행
void run_pipe(char** cmd, int count);             // 파이프 포함 명령어 실행
void run_background(char** cmd);                  // 백그라운드 실행
void handler(int signo);                          // 시그널 핸들러

struct sigaction act;                             
char input_line[BUFSIZE];                         // 명령어를 담고 있는 변수

// getch function
int getch(void){
	int ch;
	struct termios buf, save;
	tcgetattr(0,&save);
	buf = save;
	buf.c_lflag &= ~(ICANON | ECHO);
	buf.c_cc[VMIN] = 1;
	buf.c_cc[VTIME] = 0;
	tcsetattr(0, TCSAFLUSH, &buf);
	ch = getchar();
	tcsetattr(0, TCSAFLUSH, &save);
	return ch;
}
// 시그널 재정의 함수 
// SIGINT, SIGQUIT 동작이 발생하면 해당 메세지를 띄우고 종료방지 
// 입력을 받을 수 있도록 명령어 할당된 변수 초기화
void handler(int signo){
	switch(signo){
	case SIGINT:
		printf("^C\n"); break;
	case SIGQUIT:
		printf("^W\n"); break;
	}	
	memset(input_line, 0, sizeof(input_line));
}

// main function
void main(){
	act.sa_flags = 0;
	act.sa_handler = handler;
	sigemptyset(&(act.sa_mask));
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);

	while(1){
		int input_index = 0;
		int tab_count = 0;
		printf("User Shell >> ");
		while(1){
			int c = getch();
			// input 'Tab'
			if(c == 9){
				// Tab once
				if(tab_count == 0){
					int index_count = input_index;
					input_index += auto_fill(input_index);
					tab_count++;
				}
				// Tab twice
				else{
					puts("");
					run_ls_cmd();
					memset(input_line,0,strlen(input_line));
					input_index = 0;
					tab_count--;
					break;
				}
			}
			// 명령어 입력
			if(c != -1 && c != 9){
				input_line[input_index] = c;
				input_index++;
				printf("%c", c);
			}
			if(c == -1) break;
			if(c == '\n'){
				// 'q' 입력시 종료
				if(input_line[0] == 'q' && input_line[1] == '\n')
					return;
				input_line[input_index] = '\0';
				// ls 함수 실행
				if(input_line[0] == 'l' && input_line[1] == 's' && input_line[2] == '\n'){
					run_ls_cmd();
				}
				// throw command
				else
					divide_cmd(input_line);

				memset(input_line, 0, sizeof(input_line));
				break;
			}
		}
	}
}
// 'ls' command function
void run_ls_cmd(){
	struct dirent *direntp;
	DIR *dirp;
	char buf[BUFSIZE];
	// 현재 경로
	getcwd(buf, sizeof(buf));
	dirp = opendir(buf);
	// 현재 경로 항목들 읽기
	while((direntp = readdir(dirp)) != NULL){
		if(direntp->d_name[0] != '.')
			printf("%s   ", direntp->d_name);
	}
	closedir(dirp);
	puts("");
}
//자동채우기
int auto_fill(int index){
	struct dirent* direntp;
	DIR *dirp;
	char buf[BUFSIZE];
	char* elements[BUFSIZE];
	int len = 0, blank_pos;

	getcwd(buf,sizeof(buf));
	dirp = opendir(buf);
	// find equal length(공통된 부분의 길이 찾기)
	for(int x = index-1; input_line[x] != ' '; x--)
		len++;	
	blank_pos = index - len;
	// find equal section(공통된 부분길이만큼 배열에 할당)
	char equal[len];
	for(int y = 0; y<len; y++)
		equal[y] = input_line[blank_pos+y];
	equal[len] = '\0';
	// 현재 경로에서 파일이나 디렉토리 목록을 가져와 공통된 길이까지 
	// 이름이 같은것이 있으면 이들을 배열에 할당함
	int k=0;
	while((direntp = readdir(dirp)) != NULL){
		if(!strncmp(direntp->d_name,equal,len)){
			elements[k] = (char*)malloc(BUFSIZE*sizeof(char));
			elements[k] = direntp->d_name;
			k++;
		}
	}

	// element 한개
	if(k == 1){
		// 공통되는 부분이 있는 목록이 한개면 그 항목만 표시하면 되므로 할당 후 출력 
		for(int z = len; z < strlen(elements[0]); z++){
			input_line[index+(z-len)] = elements[0][z];
			printf("%c", elements[0][z]);
		}
		return (strlen(elements[0]) - len);
	}
	// element > 1
	else{
		// 공통부분을 갖는 문자열들의 집합에서 첫번째원소의 공통부분 다음 문자를 시작으로
		// 문자열 집합 두번째 원소부터 비교 시작, 공통부분 나올때까지 반복문 작동
		int z;
		for(z = len-1; z < BUFSIZE; z++){
			int count = 0;
			char simil = elements[0][z];
			for(int w = 1; w < k; w++){
				if(simil == elements[w][z])
					count++;
			}
			if(count != k-1) break;
		}
		// 공통부분(equal) 다음부터 달라지기 전까지 할당 후 출력 
		for(int i=len; i<z; i++){
			input_line[index+(i-len)] = elements[0][i];
			printf("%c",elements[0][i]);
		}
		return (z-len);
	}
}
// divide : 만약 기호들이 있다면 명령어와 기호를 나누고, 명령어에 옵션이나 명령어 뒤에
// 파일이름을 받을 수 있으니 띄어쓰기 기준으로 이들을 나누는 작업을 함
void divide_cmd(char* input){
	char* cmd[BUFSIZE];
	char* ptr = (char*)malloc(BUFSIZE * sizeof(char));
	int cmd_index = 0, p_index = 0;
	int symbol;
	int i, c;
	int isSymbol = FALSE;

	for(i = 0; i < BUFSIZE; i++){
		char c = input[i];
		// normal command
		if((c != ' ') && c != '\n')
			ptr[p_index++] = input[i];
		// blank
		else if(i != 0 && (c == ' ')){
			ptr[p_index] = '\0';
			cmd[cmd_index] = (char*)malloc(BUFSIZE * sizeof(char));
			for(int copy = 0; copy <= p_index; copy++)
				cmd[cmd_index][copy] = ptr[copy];
			cmd_index++;
			memset(ptr, 0, sizeof(ptr));
			p_index = 0;
			continue;
		}
		// command end, make command array
		else if(c == '\n'){
			cmd[cmd_index] = (char*)malloc(BUFSIZE * sizeof(char));
			ptr[p_index] = '\0';
			for(int copy = 0; copy <= p_index; copy++)
				cmd[cmd_index][copy] = ptr[copy];
			cmd_index++;
			cmd[cmd_index] = NULL;
			break;
		}
	}
	int k = 0;
	// 명령어에 기호 붙어 있으면 다른 작업으로 일반 실행과 다르게 돌아가도록 true값 할
	while(cmd[k] != NULL){
		if(cmd[k][0] == '<' || cmd[k][0] == '>' || cmd[k][0] == '|' || cmd[k][0] == '&') {
			isSymbol = TRUE;
			break;
		}
		k++;
	}

	int pipe_count = 0;
	if(isSymbol == TRUE){
		switch(cmd[k][0]){
		// 해당하는 기호에 따라 함수 실행 
		case'<': case'>': 
			run_redirection(cmd, k); break;
		case'&': run_background(cmd);break;
		case'|': 
			for(int i=0; i<strlen(input_line); i++){
				if(input_line[i] == '|')
					pipe_count++;
			}
			run_pipe(cmd, pipe_count);
			break;
		}
	}
	else
		run_normal(cmd);
}
// 명령어를 인자로 받아 이를 실행시는 함수
int run_normal(char** cmd){
	int pid;
	int status;
	
	switch(pid = fork()) {
		case -1: 
			printf("fork failed!\n"); return -1;

		case 0:
		execvp(cmd[0], cmd);
		perror(cmd[0]);
		exit(0);
        }
	
	if (waitpid(pid, &status, 0) == -1)
		return 0;
}
// redirection
void run_redirection(char** cmd, int position){
	int fd;
	char redirection = cmd[position][0];
	char* arg = cmd[position+1];
	char* fore_command[BUFSIZE];
	int i, status;
	pid_t pid;

	switch(pid = fork()) {
		case -1: 
			printf("fork failed!\n"); return ;
		// 리다이렉션 기호와 앞의 명령어를 분리하여 fore_command변수에 할당
		case 0:
		for(i=0; i < position; i++){
			fore_command[i] = (char*)malloc(BUFSIZE * sizeof(char));
			for(int j=0; j < strlen(cmd[i]); j++)
				fore_command[i][j] = cmd[i][j];
		}
		fore_command[i] = NULL;
		
		switch(redirection){
		// std_input
		case '<':
			if( (fd = open(arg, O_RDONLY, 0644)) == -1)
				perror("failed to redirect STDIN");
			dup2(fd,STDIN_FILENO);
			close(fd);
			break;
		// std_output
		case '>':
			if( (fd = open(arg, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
				perror("failed to redirect STDOUT");
			dup2(fd,STDOUT_FILENO);
			close(fd);
			break;
		}
		execvp(fore_command[0], fore_command);
		exit(0);
        }
	
	if (waitpid(pid, &status, 0) == -1)
		return ;
	
}

void run_background(char** cmd){
	int status, i;
	char* fore_command[BUFSIZE];
	int len = 0;
	pid_t pid;

	for(int x = 0; cmd[x] != NULL; x++)
		len++;

	switch(pid = fork()){
		case -1:
			printf("fork failed!\n"); return ;

		case 0:
			// '&'전 까지 명령어 복사
			for(i=0; i < len-1; i++){
				fore_command[i] = (char*)malloc(BUFSIZE * sizeof(char));
				for(int j=0; j < strlen(cmd[i]); j++)
					fore_command[i][j] = cmd[i][j];
			}
			fore_command[i] = NULL;
			execvp(fore_command[0], fore_command);
			perror(fore_command[0]);
			exit(0);
	}
	return;
}

void run_pipe(char** cmd, int p_count){
	char* cmd_ary[p_count + 1][BUFSIZE];
	pid_t pid[p_count + 1];
	int fd[p_count][2];
	int ary_index = 0;
	int l = 0;
	int status;
	int isBackground = 0;
	int isRedirection = 0;
	char* file_name;
	char* redirec;
	// 인자로 받은 문자열을 한 문자씩 검사하여 '|', '&', 리다이렉션이 있는지 검사함
	// 3차원 배열에 파이프기준으로 명령어를 나눠 저장함
	for(int k=0; cmd[k] != NULL; k++){
		if(!strcmp(cmd[k],"|")){
			cmd_ary[ary_index][l] = NULL;
			ary_index++;
			l = 0;
		}
		else if(!strcmp(cmd[k], "&")){
			isBackground = TRUE;
			break;
		}
		else if(!strcmp(cmd[k], "<") || !strcmp(cmd[k], ">")){
			isRedirection = TRUE;
			file_name = cmd[k+1];
			redirec = cmd[k];
			break;
		}
		else{
			cmd_ary[ary_index][l] = cmd[k];
			l++;
		}
	}

	cmd_ary[ary_index][l] = NULL;
	// 파이프함수를 파이프라인 갯수만큼 생성
	for(int t=0; t<p_count; t++){
		pipe(fd[t]);
	}
	// 파이프라인 앞의 명령 결과를 뒤의 명령의 입력으로 전달하도록 구현함
	// fork를 파이프라인 개수 +1만큼 하여(명령어는 파이프라인보다 1개더 많으니)
	// dup2함수를 이용해 앞의 명령어를 전달함
	for(int i=0; i<=p_count; i++){
		pid[i] = fork();
		
		if(pid[i] == -1)
			printf("fork error!\n");
		if(pid[i] == 0){
				// 첫번째 명령어를 표준 출력으로 전달함
				if(i == 0){
					dup2(fd[0][1], STDOUT_FILENO);
					for(int p=0; p<p_count; p++){
						close(fd[p][1]);
						close(fd[p][0]);
					}
					execvp(cmd_ary[0][0], cmd_ary[0]);
					perror(cmd_ary[0][0]);
					exit(0);
				}
				// 명령어 마지막에 도달했을 시, 입력을 받아 뒤에 리다이렉션이 있으면
				// 리다이렉션 실행함수 호
				else if(i == p_count){
					
					dup2(fd[i-1][0],STDIN_FILENO);
					for(int p=0; p<p_count; p++){
						close(fd[p][1]);
						close(fd[p][0]);
					}

					if(isRedirection == TRUE){
						char** input;
						int s;
						input = cmd_ary[p_count];
						for(s=0; input[s]!=NULL; s++);
						input[s] = redirec;
						input[s+1] = file_name;
						run_redirection(input, s);
					}
					
					execvp(cmd_ary[i][0], cmd_ary[i]);
					exit(0);
				}
				// 파이프 라인이 여러개있을 때, 파이프 앞을 표준입력으로 뒤를 표준 출력으로
				// 대체하여 앞의 명령어를 파이프 뒤쪽의 입력으로 전달 
				else{
					
					dup2(fd[i-1][0],STDIN_FILENO);
					dup2(fd[i][1],STDOUT_FILENO);
					for(int p=0; p<p_count; p++){
						close(fd[p][1]);
						close(fd[p][0]);
					}
					
					execvp(cmd_ary[i][0], cmd_ary[i]);
					perror(cmd_ary[i][0]);
					exit(0);
				}
				// 파이프를 이용해 열어둔 파일 기술자들을 모두 닫아 단방향으로 통신하도록
				// for반복문을 통해 이들을 닫아준 후 execvp함수를 호출하여 파이프라인 기준으로
				// 나눈 명령어들을 실행시킴
		}		
	}
	for(int p=0; p<p_count; p++){
		close(fd[p][1]);
		close(fd[p][0]);
	}
	// 만약 '&'기호가 있다면 백그라운드로 실행하기 위해 바로 종료
	if(isBackground == TRUE)
		return;
	// 생성한 자식 프로세스를 기다림
	for(int p=0; p<=p_count; p++){
		while(waitpid(pid[p], &status, WNOHANG) == 0){
			wait(&status);
			sleep(1);
		}
	}
	return ;
}

