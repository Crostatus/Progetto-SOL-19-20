#include "client.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "utility.h"
#include "cassiere.h"
#include <pthread.h>
#include "manager.h"
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#define SHOP_PARAMETERS 11
#define MIN_PARAM_VALUE 0

//Variabili per tenere traccia del numero di persone presenti nel supermarket.
int client_amount = 0;
pthread_mutex_t client_number_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t client_can_enter_cond = PTHREAD_COND_INITIALIZER;

//Variabili per sapere quando il manager ha chiuso il supermarket.
int shop_is_open;
pthread_mutex_t supermarket_key;

//Variabili di configurazione per il supermarket.
int K,
    DESK_AT_START,
    C,
    E,
    T,
    MIN_BUY_TIME,
    P,
    MAX_TIME_TO_SERVE_CUSTOMER,
    MIN_TIME_TO_SERVE_CUSTOMER,
    TIME_PER_ITEM,
    UPPER_THRESHOLD;

desk *supermarket;
extern void setup_market();

static int read_config(char* config_file);
static int insert_values(int values[]);
static void open_starting_desks();
static void spawn_clients();

int main() {
	printf("Sono il processo %ld\n", (long) getpid());
  if(read_config("config.txt") < 0)
    return -1;

  supermarket = (desk *) malloc(sizeof(desk) * K);
  setup_market();
  open_starting_desks();
  pthread_t th_manager;
  if(pthread_create(&th_manager, NULL, &manage, NULL) != 0)
    fprintf(stderr, "pthread_create failed (Manager)\n");

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGQUIT);
	if(pthread_sigmask(SIG_BLOCK, &set, NULL) != 0){
		perror("Error pthread sigmask (Supermarket)");
		exit(EXIT_FAILURE);
	}

  spawn_clients();
  pthread_join(th_manager, NULL);
	free(supermarket);
return 0;
}

//Metodo per far arrivare nuovi clienti nel supermarket.
static void spawn_clients(){
    pthread_t th_client;
    struct timespec sleep_time = get_timespec(15);
    int i;
    pthread_mutex_lock(&supermarket_key);
    while(shop_is_open){
			pthread_mutex_unlock(&supermarket_key);
      pthread_mutex_lock(&client_number_lock);
      while((C - client_amount) <= E)
        pthread_cond_wait(&client_can_enter_cond, &client_number_lock);

      pthread_mutex_unlock(&client_number_lock);
      nanosleep(&sleep_time, NULL);
			pthread_mutex_lock(&supermarket_key);
			if(!shop_is_open)
				break;
			pthread_mutex_unlock(&supermarket_key);
      for(i = 0; i < E; i++){
        if (pthread_create(&th_client, NULL, &go_shopping, NULL) != 0)
         fprintf(stderr, "pthread_create failed (Client)\n");
       }
			 pthread_mutex_lock(&supermarket_key);
    }
		pthread_mutex_unlock(&supermarket_key);
}

//Metodo per aprire casse all' avvio del supermarket.
static void open_starting_desks(){
  int i;
  int *desk_number;
  pthread_t th_worker;

  for(i = 0; i < DESK_AT_START; i++){
    desk_number = (int *)malloc(sizeof(int));
    *desk_number = i;
    if(pthread_create(&th_worker, NULL, &go_work, desk_number) != 0)
      fprintf(stderr, "pthread_create failed (Client)\n");
  }
}

//Lettura del file di configurazione
int read_config(char *config_file){
  FILE* fp;
  fp = fopen(config_file, "r");
  if(fp == NULL){
    perror(config_file);
    return -1;
  }
  char line[130];
  int i;
  int start_values[SHOP_PARAMETERS];
  for(i = 0; i < SHOP_PARAMETERS; i++)
    start_values[i] = MIN_PARAM_VALUE - 1;

  //Lettura del file
  i = 0;
  while(fgets(line, 130, fp) != NULL){
    if(line[0] != '#'){
      if(i < SHOP_PARAMETERS) {
        start_values[i] = atoi(line);
        i++;
      }
      else{
        fprintf(stderr, "File not formatted properly");
        return -2;
      }
    }
  }
  fclose(fp);

  for(i = 0; i < SHOP_PARAMETERS; i++){
    if(start_values[i] == (MIN_PARAM_VALUE - 1)){
      fprintf(stderr, "File not formatted properly");
      return -2;
    }
  }
  return insert_values(start_values);
}

//Inizializzazione variabili lette dal file di configurazione.
int insert_values(int values[]){
  if(values[0] <= 0){
    fprintf(stderr, "Invalid K");
    return -3;
  }
  K = values[0];
  if(values[1] < 0 || values[1] > K){
    fprintf(stderr, "Invalid DESK_AT_START");
    return -3;
  }
  DESK_AT_START = values[1];
  if(values[2] < 1){
    fprintf(stderr, "Invalid C");
    return -3;
  }
  C = values[2];
  if(values[3] > C){
    fprintf(stderr, "Invalid E");
    return -3;
  }
  E = values[3];
  if(values[4] <= 0){
    fprintf(stderr, "Invalid T");
    return -3;
  }
  T = values[4];
  if(values[5] < 0 || values[5] > T){
    fprintf(stderr, "Invalid MIN_BUY_TIME");
    return -3;
  }
  MIN_BUY_TIME = values[5];
  if(values[6] <= 0){
    fprintf(stderr, "Invalid P");
    return -3;
  }
  P = values[6];
  if(values[7] <= 0){
    fprintf(stderr, "Invalid MAX_TIME_TO_SERVE_CUSTOMER");
    return -3;
  }
  MAX_TIME_TO_SERVE_CUSTOMER = values[7];
  if(values[8] < 0 || values[8] > MAX_TIME_TO_SERVE_CUSTOMER){
    fprintf(stderr, "Invalid MIN_TIME_TO_SERVE_CUSTOMER");
    return -3;
  }
  MIN_TIME_TO_SERVE_CUSTOMER = values[8];
  if(values[9] < 0){
    fprintf(stderr, "Invalid TIME_PER_ITEM");
    return -3;
  }
  TIME_PER_ITEM = values[9];
  if(values[10] <= 0){
    fprintf(stderr, "Invalid UPPER_THRESHOLD");
    return -3;
  }
  UPPER_THRESHOLD = values[10];

  return 0;
}
