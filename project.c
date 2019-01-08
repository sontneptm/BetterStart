#include <wiringPi.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <wiringPi.h>
#include <softTone.h>
#include <errno.h>
#include <signal.h>

#define TOTAL  32                           
#define MAX_COUNT 5
#define PORT	8080
#define MAXBUF 80

#define DHT_PIN 24 
#define FND_REFRESH_DELAY 1
#define HOUR_TENTH 28
#define HOUR_UNIT 27
#define MINUTE_TENTH 26
#define MINUTE_UNIT 6
#define TRIG 25
#define EKHO 29

#define MAXTIMINGS 85
#define CELSIUS_TENTH 31
#define CELSIUS_UNIT 11
#define CELSIUS_DOT_C 10

pthread_mutex_t standard_mutex;

struct tm *date;
time_t unprocessed_time;

int isSomethingDetect = 1;

/* 온습도값의저장을위한전역변수 */
int dht11_dat[5] = { 0, 0, 0, 0, 0 };

void* fndControl(void*);
void* ultraSoundDetect(void*);
void* getTime(void*);
void* readData(void*);
void readDataReal();
void* socket_init(void*);
void* socket_write(void*);
void* socket_read(void*);

int is_writing = 0;
char buf[MAXBUF];
char dummy[MAXBUF];
int count =0;
int server_sockfd, client_sockfd;
struct sockaddr_in serveraddr, clientaddr;
int choice;
int client_len;
int chk_bind;
int read_len;
int write_len;
int num;


int main(int argc, char* argv)
{
	pthread_t init_thread;
	pthread_create(&init_thread, NULL, socket_init, NULL);
	pthread_join(init_thread, NULL);

	wiringPiSetup();
	
	pthread_t time_thread, fnd_thread, us_thread, dth_thread;
	pthread_t write_thread, read_thread;

	pthread_mutex_init(&standard_mutex, NULL);	

	pthread_create(&write_thread, NULL, socket_write, NULL);
	pthread_create(&read_thread, NULL, socket_read, NULL);
	pthread_create(&dth_thread, NULL, readData, NULL);
	pthread_create(&us_thread, NULL, ultraSoundDetect, NULL);
	pthread_create(&time_thread, NULL, getTime, NULL);
	pthread_create(&fnd_thread, NULL,fndControl, NULL);
	
	
	pthread_join(dth_thread, NULL);
	pthread_join(time_thread, NULL);
	pthread_join(fnd_thread, NULL);
	pthread_join(us_thread, NULL);
	pthread_join(write_thread, NULL);
	pthread_join(read_thread, NULL);

	pthread_mutex_destroy(&standard_mutex);

	pthread_cancel(dth_thread);
	pthread_cancel(time_thread);
	pthread_cancel(fnd_thread);
	pthread_cancel(us_thread);
	pthread_cancel(write_thread);
	pthread_cancel(read_thread);

	return 0;
}

void* socket_write(void* args){
	int tem=0;

	while(1){ 
		is_writing = 1;

		memset(dummy, 0x00, MAXBUF);

		if(dht11_dat[2]<50){
			sprintf(dummy,"us= %d,습도 : %d%%,온도:  %d C\n",
			isSomethingDetect, dht11_dat[0], dht11_dat[2]);
		}
		printf("dummy = %s\n", dummy);
		write_len = write(client_sockfd, dummy, strlen(dummy));
		
		is_writing = 0;
		delay(1000);
	}
}

void* socket_read(void* args){
	while(1){ 
		while(is_writing){
		}

		memset(buf, 0x00, MAXBUF);
		read_len = read(client_sockfd, buf, MAXBUF);
		printf("buf = %s\n", buf);

		delay(100);
	}
}

void* ultraSoundDetect(void* arg) {
	pthread_mutex_lock(&standard_mutex);
	
	int start_time, end_time;
	float distance;
	int intDistance;
	int count=0;

	pinMode(TRIG, OUTPUT);
	pinMode(EKHO, INPUT);
	
	pthread_mutex_unlock(&standard_mutex);

	while (1) {

		digitalWrite(TRIG, LOW);
		delay(500);
		digitalWrite(TRIG, HIGH);
		delayMicroseconds(10);
		digitalWrite(TRIG, LOW);
		while (digitalRead(EKHO) == 0);
		start_time = micros();
		while (digitalRead(EKHO) == 1);
		end_time = micros();

		pthread_mutex_lock(&standard_mutex);

		distance = (end_time - start_time) / 29. / 2.;
		intDistance = (int)distance;
		printf("distance %d cm\n", intDistance);

	

		if(intDistance<60){
			isSomethingDetect = 1;
			count = 0;
		}
		if(count > 60 && isSomethingDetect!=0){
			isSomethingDetect = 0;
		} 

		pthread_mutex_unlock(&standard_mutex);

		count ++;
	}
}

void* getTime(void* arg) {
	while (1) {
		pthread_mutex_lock(&standard_mutex);

		time(&unprocessed_time);
		date = localtime(&unprocessed_time);

		pthread_mutex_unlock(&standard_mutex);
		delay(1000);
	}
}

void* fndControl(void* arg)
{	
	pthread_mutex_lock(&standard_mutex);
	int i;
	int j;
	int hour_tenth_digit = date->tm_hour / 10;
	int hour_unit_digit = date->tm_hour % 10;
	int minute_tenth_digit = date->tm_min / 10;
	int minute_unit_digit = date->tm_min % 10;
	int celsius = 0;
	int celsius_ten = 0;
	int celsius_unit = 0;
	int celsius_dot_c = 8;
	
	int gpiopins[4] = { 4, 1, 16, 15 };     /* A, B, C, D : 23 12 15 14 */
	int number[10][4] = {
	{0,0,0,0},      /* 0 */
	{0,0,0,1},      /* 1 */
	{0,0,1,0},      /* 2 */
	{0,0,1,1},      /* 3 */
	{0,1,0,0},      /* 4 */
	{0,1,0,1},      /* 5 */
	{0,1,1,0},      /* 6 */
	{0,1,1,1},      /* 7 */
	{1,0,0,0},      /* 8 */
	{1,0,0,1} };    /* 9 */

	pinMode(HOUR_TENTH, OUTPUT);
	pinMode(HOUR_UNIT, OUTPUT);
	pinMode(MINUTE_TENTH, OUTPUT);
	pinMode(MINUTE_UNIT, OUTPUT);
	pinMode(CELSIUS_TENTH, OUTPUT);
	pinMode(CELSIUS_UNIT, OUTPUT);
	pinMode(CELSIUS_DOT_C, OUTPUT);
	for (i = 0; i < 4; i++) {
		pinMode(gpiopins[i], OUTPUT);                /* 모든 Pin의 출력 설정 */
	}
	pthread_mutex_unlock(&standard_mutex);

	while (1) {
		pthread_mutex_lock(&standard_mutex);

		hour_tenth_digit = date->tm_hour / 10;
		hour_unit_digit = date->tm_hour % 10;
		minute_tenth_digit = date->tm_min / 10;	
		minute_unit_digit = date->tm_min % 10;

		
		if(dht11_dat[2]>5 && dht11_dat[2]<35 ){
			celsius = dht11_dat[2];
		}
		celsius_ten = celsius/10;
		celsius_unit = celsius%10;	

		pthread_mutex_unlock(&standard_mutex);

		delay(1);
		

		if (isSomethingDetect == 1) {
			for (i = 0; i < 4; i++) {
				if (i == 0) {
					digitalWrite(HOUR_TENTH, HIGH);
					digitalWrite(HOUR_UNIT, LOW);
					digitalWrite(MINUTE_TENTH, LOW);
					digitalWrite(MINUTE_UNIT, LOW);
					digitalWrite(CELSIUS_TENTH, LOW);
					digitalWrite(CELSIUS_UNIT, LOW);
					digitalWrite(CELSIUS_DOT_C, LOW);
					for(j=0;j<4;j++){
						digitalWrite(gpiopins[j], number[hour_tenth_digit][j] == 1 ? HIGH : LOW);      /* HIG LED*/
					}
					delay(FND_REFRESH_DELAY);
				}
				else if (i == 1) {
					digitalWrite(HOUR_TENTH, LOW);
					digitalWrite(HOUR_UNIT, HIGH);
					digitalWrite(MINUTE_TENTH, LOW);
					digitalWrite(MINUTE_UNIT, LOW);
					digitalWrite(CELSIUS_TENTH, LOW);
					digitalWrite(CELSIUS_UNIT, LOW);
					digitalWrite(CELSIUS_DOT_C, LOW);
					for(j=0;j<4;j++){
						digitalWrite(gpiopins[j], number[hour_unit_digit][j] == 1 ? HIGH : LOW); /* HIGH(1)D 켜기 */
					}
					delay(FND_REFRESH_DELAY);
				}
			
				else if (i == 2) {
					digitalWrite(HOUR_TENTH, LOW);
					digitalWrite(HOUR_UNIT, LOW);
					digitalWrite(MINUTE_TENTH, HIGH);
					digitalWrite(MINUTE_UNIT, LOW);
					digitalWrite(CELSIUS_TENTH, LOW);
					digitalWrite(CELSIUS_UNIT, LOW);
					digitalWrite(CELSIUS_DOT_C, LOW);
					for(j=0;j<4;j++){
						digitalWrite(gpiopins[j], number[minute_tenth_digit][j] == 1 ? HIGH : LOW);/* HIGH(켜기 */
					}
					delay(FND_REFRESH_DELAY);
				}
				else if (i == 3) {
					digitalWrite(HOUR_TENTH, LOW);
					digitalWrite(HOUR_UNIT, LOW);
					digitalWrite(MINUTE_TENTH, LOW);
					digitalWrite(MINUTE_UNIT, HIGH);
					digitalWrite(CELSIUS_TENTH, LOW);
					digitalWrite(CELSIUS_UNIT, LOW);
					digitalWrite(CELSIUS_DOT_C, LOW);
					for(j=0;j<4;j++){
						digitalWrite(gpiopins[j], number[minute_unit_digit][j] == 1 ? HIGH : LOW);       
					}
					delay(FND_REFRESH_DELAY);
				}
			}
			for (i = 0; i < 3; i++) {
				if (i == 0) {
					digitalWrite(HOUR_TENTH, LOW);
					digitalWrite(HOUR_UNIT, LOW);
					digitalWrite(MINUTE_TENTH, LOW);
					digitalWrite(MINUTE_UNIT, LOW);
					digitalWrite(CELSIUS_TENTH, HIGH);
					digitalWrite(CELSIUS_UNIT, LOW);
					digitalWrite(CELSIUS_DOT_C, LOW);
					for (j = 0; j<4; j++) {
						digitalWrite(gpiopins[j], number[celsius_ten][j] == 1 ? HIGH : LOW);      /* HIG LED*/
					}
					delay(FND_REFRESH_DELAY);
				}
				else if (i == 1) {
					digitalWrite(HOUR_TENTH, LOW);
					digitalWrite(HOUR_UNIT, LOW);
					digitalWrite(MINUTE_TENTH, LOW);
					digitalWrite(MINUTE_UNIT, LOW);
					digitalWrite(CELSIUS_TENTH, LOW);
					digitalWrite(CELSIUS_UNIT, HIGH);
					digitalWrite(CELSIUS_DOT_C, LOW);
					for (j = 0; j<4; j++) {
						digitalWrite(gpiopins[j], number[celsius_unit][j] == 1 ? HIGH : LOW); /* HIGH(1)D 켜기 */
					}
					delay(FND_REFRESH_DELAY);
				}
				else if (i == 2) {
					digitalWrite(HOUR_TENTH, LOW);
					digitalWrite(HOUR_UNIT, LOW);
					digitalWrite(MINUTE_TENTH, LOW);
					digitalWrite(MINUTE_UNIT, LOW);
					digitalWrite(CELSIUS_TENTH, LOW);
					digitalWrite(CELSIUS_UNIT, LOW);
					digitalWrite(CELSIUS_DOT_C, HIGH);
					for (j = 0; j<4; j++) {
						digitalWrite(gpiopins[j], number[celsius_dot_c][j] == 1 ? HIGH : LOW);/* HIGH(켜기 */
					}
					delay(FND_REFRESH_DELAY);
				}
			}
		}
		else if (isSomethingDetect == 0) {
			digitalWrite(HOUR_TENTH, HIGH);
			digitalWrite(HOUR_UNIT, HIGH);
			digitalWrite(MINUTE_TENTH, HIGH);
			digitalWrite(MINUTE_UNIT, HIGH);
			digitalWrite(CELSIUS_TENTH, HIGH);
			digitalWrite(CELSIUS_UNIT, HIGH);
			digitalWrite(CELSIUS_DOT_C, HIGH);
			for (j = 0; j<4; j++) {
				digitalWrite(gpiopins[j], HIGH);/* HIGH(켜기 */
			}
			delay(FND_REFRESH_DELAY);
		}
		
	}
}

void* readData(void* args){
	while(1){
		while(is_writing){
		}
		pthread_mutex_lock(&standard_mutex);
		readDataReal();
		pthread_mutex_unlock(&standard_mutex);
		delay(1000);
	}
}

void readDataReal()
{
	uint8_t laststate= HIGH;
    uint8_t counter= 0;
    uint8_t j= 0, i;
    float f; 
	
	pinMode( DHT_PIN, OUTPUT );
    digitalWrite( DHT_PIN, LOW );
    delay( 18 );
    digitalWrite( DHT_PIN, HIGH );
    delayMicroseconds( 40 );
    pinMode( DHT_PIN, INPUT );
	

	dht11_dat[0] = dht11_dat[1] = dht11_dat[2] = dht11_dat[3] = dht11_dat[4] = 0;
	

		for ( i = 0; i < MAXTIMINGS; i++ )
    {
        counter = 0;
        while ( digitalRead( DHT_PIN) == laststate )
        {
            counter++;
            delayMicroseconds( 1 );
            if ( counter == 255 )
            {
                break;
            }
        }
        laststate = digitalRead( DHT_PIN );
 
        if ( counter == 255 )
            break;
 
        if ( (i >= 4) && (i % 2 == 0) )
        {
            dht11_dat[j / 8] <<= 1;
            if ( counter > 16 )
                dht11_dat[j / 8] |= 1;
            j++;
        }
    }
 
    if ( (j >= 40) &&
         (dht11_dat[4] == ( (dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF) ) )
    {
        f = dht11_dat[2] * 9. / 5. + 32;
        printf( "Humidity = %d.%d %% Temperature = %d.%d C (%.1f F)\n",
         dht11_dat[0], dht11_dat[1], dht11_dat[2], dht11_dat[3], f );
    }else  {
        printf( "Data not good, skip\n" );
    }
}

void* socket_init(void* args){


	client_len = sizeof(clientaddr);

	server_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_sockfd == -1) {
		perror("socket error : ");
		return 0;
	}

	/* bind() */
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(PORT);
	chk_bind = bind(server_sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (chk_bind > 0) {
		perror("bind error : ");
		return 0;
	}

	/* listen() */
	if (listen(server_sockfd, 5)) {
		perror("listen error : ");
	}

	/* accept() */
	client_sockfd = accept(server_sockfd, (struct sockaddr *)&clientaddr, &client_len);
	printf("New Client Connect: %s\n", inet_ntoa(clientaddr.sin_addr));

}
