#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/time.h>

#include <limits.h>//테스트용
#define MAXLINE 20010

int menu();//처음 시작 메뉴 선택 함수
int howMany();//세대 수 입력받는 함수
int getTotalRow(FILE *fp);//입력파일의 행(가로)을 알아내는 함수
int getTotalColumn(FILE *fp);//입력파일의 열(세로)을 알아내는 함수
double getTotalTime(struct timeval *startt, struct timeval *endt);//시간 계산 함수
void sequencialProcess(char *fname);//2.순차처리 수행 함수
char* getGenFileName(int num, char *gnef);//세대 진행하면서 결과 저장할 파일
char gameRules(char cell, int totalNeig);//모든 세포에 적용해야하는 게임 룰 함수
char getTotalNeighbal(int c, int max_column, char **cellbuf);//이웃 세포들을 int형으로 바꿔서 모두 더해주는 함수
int howManyChild();//생성할 자식 프로세스 또는 쓰레드의 개수를 입력받는 함수
void calculateLineOfChild(int *buf, int child, int row);//각 child process가 계산을 수행할 Line 수 계산
void processConcurrency(char *fname);//3.process 병렬처리 함수
void childProcess(FILE *inputfp, FILE *outputfp, int line, int column, int seek, char **cellbuf);//생성된 child process가 각자 게임 진행하는 함수
void threadConcurrency(char *fname);//4. 멀티 쓰레드 이용한 병렬처리 함수
void *childThread(void *arg);//child thread가 해야할 일

typedef struct _thread_data{
	int line;
	int seek;
	int column;
	FILE *inputfp;
	FILE *outputfp;
}thread_data;

pthread_mutex_t mutex_read = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_write = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char** argv){
	int menunum;
	if(argc != 2){
		fprintf(stderr, "usage : %s [파일명]\n", argv[0]);
		exit(1);
	}

	while(1){
		menunum = menu();
		switch(menunum){
		case 1:
			exit(0);
		case 2:
			sequencialProcess(argv[1]);
			break;
		case 3:
			processConcurrency(argv[1]);
			break;
		case 4:
			threadConcurrency(argv[1]);
			break;
		default :
			printf("옳바른 메뉴 번호를 입력하세요.\n\n");
			break;
		}
		printf("\n");
	}
}

int menu(){//처음 시작 메뉴 선택 함수
	int num;
	
	printf(">>세포 매트릭스 게임<<\n\n");
	printf("(1) 프로그램 종료   (2) 순차처리   (3) Process 병렬처리   (4) Thread 병렬처리\n\n");
	
	printf("번호를 선택하세요 : ");
	scanf("%d",&num);
	
	while(getchar() != '\n');
	return num;
}

int howMany(){//세대 수 입력받는 함수
	int num;
	printf("게임을 진행할 세대 수를 입력하세요 : ");
	while(1){
		scanf("%d", &num);
	
		if(num > 0){
			break;
		}
		printf("\n최소 1이상의 세대 수를 입력하세요(최대 : int가 표현 가능한 수) : ");
	}
	
	while(getchar() != '\n');
	return num;
}

int getTotalColumn(FILE *fp){//입력파일의 행(가로)을 알아내는 함수(공백, 개행 포함)
	int column;
	char buf[MAXLINE+1];
	
	memset(buf, 0, sizeof(buf));
	fgets(buf, MAXLINE, fp);
	column = strlen(buf);
	rewind(fp);
	
	return column;
}

int getTotalRow(FILE *fp){//입력파일의 열(세로)을 알아내는 함수
	int row = 0;
	char buf[MAXLINE+1];
	
	while(fgets(buf, MAXLINE+1, fp) != NULL){
		row++;
	}
	rewind(fp);
	
	return row;
}
char* getGenFileName(int num, char *genf){//세대 진행하면서 결과 저장할 파일명 생성
	
	memset(genf, 0, sizeof(genf));
	sprintf(genf, "gen_%d.matrix", num);
	
	return genf;
}
char gameRules(char cell, int totalNeig){//모든 세포에 적용해야하는 게임 룰 함수
	char result;
	if(cell == '1' || cell == 1){
		if(totalNeig <= 2 || totalNeig >= 7)
			return result = '0';
		else if(totalNeig >=3 && totalNeig <= 6)
			return result = '1';
	}
	else if(cell == '0' || cell == 0){
		if(totalNeig == 4)
			return result = '1';
		else
			return result = '0';
	
	}
	else{
		fprintf(stderr, "0이나 1이 아닌 세포가 존재합니다.\n");
		exit(1);
	}
}

char getTotalNeighbal(int c, int max_column, char **cellbuf){//이웃 세포들을 int형으로 바꿔서 모두 더해주는 함수
	int total = 0;
	char result;
	
	
	if(c-2 >= 0){
		total += atoi(&cellbuf[0][c-2]);
		total += atoi(&cellbuf[1][c-2]);
		total += atoi(&cellbuf[2][c-2]);
	}
	
	if(c >= 0 && c <= max_column-2){
		total += atoi(&cellbuf[0][c]);
		total += atoi(&cellbuf[2][c]);
	}
	
	if(c+2 <= max_column-2){
		total += atoi(&cellbuf[0][c+2]);
		total += atoi(&cellbuf[1][c+2]);
		total += atoi(&cellbuf[2][c+2]);
	}	
	
	result = gameRules(cellbuf[1][c], total);
	return result;
}

double getTotalTime(struct timeval *startt, struct timeval *endt){//시간 계산 함수
	return ((endt->tv_sec)-(startt->tv_sec))*1000 + ((endt->tv_usec)-(startt->tv_usec))*0.001;
}
void sequencialProcess(char *fname){//2.순차처리 수행 함수
	FILE *fp;//input파일 포인터
	FILE *wfp;//결과파일 포인터	
	char *genf;//세대 진행 결과 저장 파일명
	char *resultf="output.matrix";//최종 결과파일명
	char **cellbuf;//입력 파일에서 읽어올 공간
	int totalNeig;
	char *writebuf;
	char outputcell;//결과값 cell
	struct timeval startt, endt;
	double time = 0;

	if((fp = fopen(fname, "r")) == NULL){
		fprintf(stderr, "file open error for %s\n", fname);
		exit(1);
	}

	const int generation = howMany();//세대 수
	const int column = getTotalColumn(fp); //가로 줄
	const int row = getTotalRow(fp);//세로 줄
	
	genf = malloc(sizeof(char)*11+sizeof(int)+1);
   	cellbuf = malloc(sizeof(char *)*3);
   	for(int i = 0; i <3 ; i++){
      		cellbuf[i] = malloc(sizeof(char)*column);
      	}
      	writebuf = (char *)malloc(sizeof(char)*row*column);
      	
      	gettimeofday(&startt, NULL);
      	
      	for(int g = 1; g <= generation; g++){
      		
      		if(g == generation){
        		if((wfp = fopen(resultf, "w")) == NULL){
					fprintf(stderr, "file open error for %s\n", resultf);
					exit(1);
				}
      		}
      		else{
         		genf = getGenFileName(g, genf); //세대 진행 결과 저장 파일명
         		if((wfp = fopen(genf, "w")) == NULL){
					fprintf(stderr, "file open error for %s\n", genf);
					exit(1);
				}
         		//setbuf(wfp, writebuf);
         		
      		}
      		
      		memset(cellbuf[0], '0', column);
      		
      		for(int i = 1; i < 3; i++){
      			memset(cellbuf[i], 0, column);
      			fread(cellbuf[i], column, 1, fp);
      		}
      		  
      		  for(int r = 0; r < row; r++){	
      			for(int c = 0; c < column-1; c+=2){
				outputcell = getTotalNeighbal(c, column, cellbuf);
				
				if(c == column-3)
					fprintf(wfp, "%c\r\n", outputcell);
				else
					fprintf(wfp, "%c ", outputcell);
			}
			
			strncpy(cellbuf[0], cellbuf[1], column);
			strncpy(cellbuf[1], cellbuf[2], column);
			
			memset(cellbuf[2], 0, column);
			fread(cellbuf[2], column, 1, fp);
		}
		
		fflush(wfp);
		fclose(wfp);
      		freopen(genf, "r", fp);//현재 결과 저장된 파일을 다음세대의 인풋파일로 사용     
      	}
      	gettimeofday(&endt, NULL);
      	
      	time = getTotalTime(&startt, &endt);
      	printf("총 수행 시간 : %f ms\n",time);/////////시간 단위 확인하기
      	
      	fclose(fp);
	for(int i = 0; i<3; i++){
		free(cellbuf[i]);
      	}
      	free(cellbuf);
      	free(genf);
      	free(writebuf);
}

int howManyChild(){//생성할 자식 프로세스 또는 쓰레드의 개수를 입력받는 함수
	int num;
	printf("생성할 Child Process 또는 Thread 개수를 입력하세요 : ");
	while(1){
		scanf("%d", &num);
	
		if(num > 0){
			break;
		}
		printf("\n최소 1이상을 입력하세요(최대 : int가 표현 가능한 수) : ");
	}
	while(getchar() != '\n');
	
	return num;
}

void calculateLineOfChild(int *buf, int child, int row){//각 child process가 계산을 수행할 Line 수 계산
	int divide;
	for(int i  = 0; i<child; i++){
		buf[i] = row / child;
	}

	divide = row % child;
	int i = 0;
	while(divide){
		buf[i]++;
		i++;
		divide--;
	}
}

void processConcurrency(char *fname){//3.process 병렬처리 함수
	FILE *inputfp;//input 파일
	FILE *outputfp;//결과 저장 파일포인
	char *genf;//세대 진행 결과 저장 파일명
	char *resultf = "output.matrix";//최종 결과파일명
	int *childLineCount;//각 child process가 진행할 line 수
	pid_t *childPid;//생성된 child process들의 pid들을 저장
	int seek;//fseek하기 위한 변수
	char **cellbuf;
	int waitnum;
	pid_t waitpid;
	struct timeval startt, endt;
	double time = 0;
	

	if((inputfp = fopen(fname, "r")) == NULL){
		fprintf(stderr, "file open error for %s\n", fname);
		exit(1);
	}

	const int generation = howMany();//세대 수
	int child = howManyChild();//병렬처리할 child 수
	const int column = getTotalColumn(inputfp);//가로
	const int row = getTotalRow(inputfp);//세로


	if(child > row){//진행할 row보다 입력받은 child 수가 크면 아무일도 하지않는 chile process가 생기므로 그런 일이 발생하지 않도록 처리
		child = row;
		printf("child : %d\n", row);
	}

	childPid = (pid_t *)malloc(sizeof(pid_t)*child);
	childLineCount = (int *)malloc(sizeof(int)*child);
	genf = (char *)malloc(sizeof(char)*11+sizeof(int)+1);
	cellbuf = (char **)malloc(sizeof(char *)*3);
	for(int i = 0; i < 3; i++)
		cellbuf[i] = (char *)malloc(sizeof(char)*column);


	memset(childLineCount, 0, sizeof(childLineCount));
	memset(genf, 0, sizeof(genf));
	memset(childPid, 0, sizeof(childPid));

	calculateLineOfChild(childLineCount, child, row);


	for(int g = 1; g <= generation; g++){//입력 받은 세대 수만큼 반복
		seek = 0;

		if(g == generation){
			if((outputfp = fopen(resultf, "w")) == NULL){
				fprintf(stderr, "file open error for %s\n", resultf);
				exit(1);
			}
		 }
		 else{
			genf = getGenFileName(g, genf); //세대 진행 결과 저장 파일명
			if((outputfp = fopen(genf, "w")) == NULL){
				fprintf(stderr, "file open error for %s\n", genf);
				exit(1);
		 	}
		 }   
		 
		gettimeofday(&startt, NULL);
		 
		for(int i = 0; i < child; i++){//생성할 자식 프로세스만큼 반복
			if((childPid[i] = fork()) < 0){
				fprintf(stderr, "fork error\n");
				exit(1);
			}
			else if(childPid[i] == 0){
				childProcess(inputfp, outputfp, childLineCount[i], column, seek, cellbuf);
			}
			seek += childLineCount[i];
		}

		waitnum = 0;
		while(waitnum < child){
			if((waitpid = wait((int *)0)) != -1)
				waitnum++;
		
		gettimeofday(&endt, NULL);
		time += getTotalTime(&startt, &endt); 
		
		 printf("PID : %d\n", waitpid);
		}
		fclose(outputfp);
		freopen(genf, "r", inputfp);
	}
	
      	printf("총 수행 시간 : %f ms\n",time);/////////시간 단위 확인하기

	free(genf);
	free(childLineCount);
	free(childPid);
	for(int i = 0; i < 3; i++)
		free(cellbuf[i]);
	free(cellbuf);
	fclose(inputfp);
}

void childProcess(FILE *inputfp, FILE *outputfp, int line, int column, int seek, char **cellbuf){//생성된 child process가 각자 게임 진행하는 함수
	char writebuf[BUFSIZ];
	int totalNeig;
	char outputcell;
	int readpos, writepos;
	char *resultbuf;
	int inputfd = inputfp->_fileno;
	int outputfd = outputfp->_fileno;
	
	resultbuf = malloc(sizeof(char)*column+1);
	memset(resultbuf, 0, sizeof(char)*column+1);
	

	if(seek -1 < 0){
		memset(cellbuf[0], '0', column);
	}
	else{

		pread(inputfd, cellbuf[0], column, (seek-1)*column);
	}

	for(int i = 1; i < 3; i++){
		memset(cellbuf[i], 0, column);
		pread(inputfd, cellbuf[i], column, (seek+i-1)*column);
	}

	for(int r = 0; r < line; r++){
		memset(resultbuf, 0, sizeof(char)*column+1);
		
		for(int c = 0; c < column-1; c+=2){
			outputcell = getTotalNeighbal(c, column, cellbuf);

			if(c == column-3)
				sprintf(resultbuf+c, "%c\r\n", outputcell);
			else
				sprintf(resultbuf+c, "%c ", outputcell);
		}
		
		pwrite(outputfd, resultbuf, column, (seek+r)*column);
		
		strncpy(cellbuf[0], cellbuf[1], column);
		strncpy(cellbuf[1], cellbuf[2], column);

		memset(cellbuf[2], 0, column);
		pread(inputfd, cellbuf[2], column, (seek+2+r)*column);
	}
	
	fflush(outputfp);
	
	free(resultbuf);
	exit(0);
}

void threadConcurrency(char *fname){//4. 멀티 쓰레드 이용
	FILE *inputfp;
	FILE *outputfp;
	char *genf;//세대 진행 결과 저장 파일명
	char *resultf = "output.matrix";
	int *childLineCount;//각 thread가 진행할 line 수
	int seek;//seek 하기위한 변수
	pthread_t *tid;
	thread_data *tdata;
	struct timeval startt, endt;
	double time = 0;
	
	if((inputfp = fopen(fname, "r")) == NULL){
		fprintf(stderr, "file open error for %s\n", fname);
		exit(1);
	}

	const int generation = howMany();//진행할 세대 수
	int child = howManyChild();//병렬처리할 thread 수
	const int column = getTotalColumn(inputfp);//가로
	const int row = getTotalRow(inputfp);
	
	if(child > row){//진행할 row보다 입력받은 child 수가 크면 수 맞춰서 쓸모없는 자식 생성하지 않도록 함
		child = row;
	}

	childLineCount = (int *)malloc(sizeof(int)*child);
	genf = (char *)malloc(sizeof(char)*11 + sizeof(int)+1);
	tid = (pthread_t *)malloc(sizeof(pthread_t)*child);
	tdata = (thread_data *)malloc(sizeof(thread_data)*child);

	memset(childLineCount, 0, sizeof(childLineCount));
	memset(genf, 0, sizeof(genf));
	memset(tid, 0, sizeof(pthread_t)*child);
	
	calculateLineOfChild(childLineCount, child, row);
	
	for(int g = 1; g <= generation; g++){
		seek = 0;

		if(g == generation){
			if((outputfp = fopen(resultf, "w")) == NULL){
				fprintf(stderr, "file open error for %s\n", resultf);
				exit(1);
			}
		}
		else{
			genf = getGenFileName(g, genf);
			if((outputfp = fopen(genf, "w")) == NULL){
				fprintf(stderr, "file open error for %s\n", genf);
				exit(1);
			}
		}
		
		gettimeofday(&startt, NULL);
		
		for(int i = 0; i < child; i++){
			
			tdata[i].line = childLineCount[i];
			tdata[i].inputfp = inputfp;
			tdata[i].outputfp = outputfp;
			tdata[i].seek = seek;
			tdata[i].column = column;
			
			if(pthread_create(&tid[i], NULL, childThread, (void *)&tdata[i]) != 0){
				fprintf(stderr, "pthread_create error\n");
				exit(1);
			}
			seek += childLineCount[i];
		}
		
		for(int i = 0; i < child; i++){
			pthread_join(tid[i], NULL);
		}
		
		gettimeofday(&endt, NULL);
		time += getTotalTime(&startt, &endt);
		
		for(int i = 0; i<child; i++)
			printf("Thread id : %u\n", (unsigned int)tid[i]);
		
		fclose(outputfp);
		freopen(genf, "r", inputfp);
	}
	
      	printf("총 수행 시간 : %f ms\n",time);/////////시간 단위 확인하기
	
	pthread_mutex_destroy(&mutex_read);
	pthread_mutex_destroy(&mutex_write);
	free(genf);
	free(childLineCount);
	free(tid);
	fclose(inputfp);
}

void *childThread(void *arg){//child thread가 해야할 일
	thread_data *data = (thread_data *)arg;
	FILE *inputfp = data->inputfp;
	FILE *outputfp = data->outputfp;
	int line = data->line;
	int seek = data->seek;
	int column = data->column;
	char **cellbuf;
	char *resultbuf;
	char outputcell;
	
	
	resultbuf = malloc(sizeof(char)*column+1);
	cellbuf = (char **)malloc(sizeof(char *)*3);
	for(int i = 0; i < 3; i++)
		cellbuf[i] = (char *)malloc(sizeof(char)*column);
	
	memset(resultbuf, 0, sizeof(char)*column+1);
	for(int i = 0; i < 3; i++)
		memset(cellbuf[i], 0, column);
	
	
	if(seek -1 < 0){
		memset(cellbuf[0], '0', column);
	}
	else{
		pthread_mutex_lock(&mutex_read);

		fseek(inputfp, (seek-1)*column ,SEEK_SET);
		fread(cellbuf[0], column, 1, inputfp);
		
		pthread_mutex_unlock(&mutex_read);

	}

	for(int i = 1; i < 3; i++){
		memset(cellbuf[i], 0, column);
		pthread_mutex_lock(&mutex_read);
		
		fseek(inputfp, (seek+i-1)*column ,SEEK_SET);
		fread(cellbuf[i], column, 1, inputfp);

		pthread_mutex_unlock(&mutex_read);

	}

	for(int r = 0; r < line; r++){
		memset(resultbuf, 0, sizeof(char)*column);
		
		for(int c = 0; c < column-1; c+=2){
			outputcell = getTotalNeighbal(c, column, cellbuf);

			if(c == column-3)
				sprintf(resultbuf+c, "%c\r\n", outputcell);
			else
				sprintf(resultbuf+c, "%c ", outputcell);
			
		}
		

		pthread_mutex_lock(&mutex_write);

		fseek(outputfp, (seek+r)*column, SEEK_SET);
		fwrite(resultbuf, column, 1, outputfp);
		
		pthread_mutex_unlock(&mutex_write);
		
		strncpy(cellbuf[0], cellbuf[1], column);
		strncpy(cellbuf[1], cellbuf[2], column);

		memset(cellbuf[2], 0, column);
		
		pthread_mutex_lock(&mutex_read);
		
		fseek(inputfp, (seek+2+r)*column ,SEEK_SET);
		fread(cellbuf[2], column, 1, inputfp);
		
		pthread_mutex_unlock(&mutex_read);
	}
	
	fflush(outputfp);
	for(int i = 0; i < 3; i++)
		free(cellbuf[i]);
	free(cellbuf);
	free(resultbuf);
		
	pthread_exit(NULL);
}
