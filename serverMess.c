#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>  
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#define MESSAGE_MAXLEN 1024


// NOMBRE CLIENTS
int nr_clients;

//VERROU
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


// STRUCTURE MESSAGE
typedef struct s_mssg {
char sender[9];
char* text;
struct s_mssg * next;
} mssg;

// STRUCTURE BOÎTE AUX LETTRES
typedef struct s_mbox{
mssg *first, *last;
} mbox;


//STRUCTURE DONNÉES CLIENTS
typedef struct client_data_s {
	int sock;
	pthread_t thread;
	char nick[9];
	mbox* box;
	struct client_data_s *prev, *next;
}client_data;


//CLIENT FIRST ET LAST
client_data *first, *last;

//CONSTRUCTEUR BOÎTE VIDE
mbox* init_mbox(){
		mbox* box = (mbox*)malloc(sizeof(mbox));
		
		box->first = NULL;
		box->last = NULL;
		return box;
}

//CONSTRUCTEUR MESSAGE
mssg* create_msg(char* author, char* contents){
	mssg* my_msg= (mssg*)malloc(sizeof(mssg));
	my_msg->text = (char*)malloc(strnlen(contents, 256)*sizeof(char));
	
	//AUTEUR
	strncpy(my_msg->sender, author, strnlen(author,9));
	my_msg->sender[8] = '\0';
	
	//TEXTE
	strncpy(my_msg->text, contents, strnlen(contents,256));
	
	my_msg->next = NULL;
	
	return my_msg;
}


//INITALISATION CLIENTS
void init_clients(){
	nr_clients = 0;
	first = NULL;
	last = NULL;
}


//CONSTRUCTEUR CLIENTS
client_data* alloc_client(int sock){

	client_data* my_client = (client_data*)malloc(sizeof(client_data));
	
	if(my_client == NULL) return NULL;
	
	my_client->sock = sock;
	my_client-> box = init_mbox();
	strcpy(my_client->nick,"");
	
	pthread_mutex_lock(&lock);
	
	if(nr_clients == 0){
		
		first = last = my_client;
		my_client->prev = NULL;
		my_client->next = NULL;
		
	}
	else{
		
		last->next = my_client;
		my_client->prev = last;
		my_client->next = NULL;
		last = my_client;
		
	}
	
	
	nr_clients++;
	pthread_mutex_unlock(&lock);
	
	return my_client;
}

//LIBÈRE MÉMOIRE CLIENT
void free_client(client_data* cli){
	
	pthread_mutex_lock(&lock);
	if(cli!=NULL){
		if(nr_clients == 1){
			first = last = NULL;
		}
		else if(cli==first){
			first=first->next;
			first->prev= NULL;
		}
		else if(cli == last){
			last = last->prev;
			last->next = NULL;
		}
		else {
			cli->prev->next = cli -> next;
			cli->next->prev = cli -> prev;
		}
		free(cli->box);
		free(cli);
		nr_clients--;
	}
	pthread_mutex_unlock(&lock);
}

//RÉCUPÈRE MESSAGE CLIENT
int receive_message(int sock, char *buffer, int size){
	int rc, ind;
	
	for (ind=0;ind<size; ind++){
		rc=read(sock,&buffer[ind],1);
		
		if (buffer[ind]=='\n')break;
		if(rc<=0) return -1;
	}
	if(buffer[ind-1]=='\r') buffer[ind-1] ='\0';
	else buffer[ind] = '\n';	
	
return 0;
}

//VÉRIFIE PSEUDO CLIENT. RENVOIE 1 SI CORRECT
int valid_nick(char* nick){
	
	//VÉRIFIE CARACTÈRE ALPHANUMÉRIQUE
	 for(int i=0;i<strlen(nick)-1;i++){
		if(isalnum(nick[i])==0) return 0;
	} 
	client_data* current = first;
	while(current != NULL){
		if(strcmp(nick, current->nick) == 0) return 0;
		current = current->next;
	}
	return 1;
}
	

//CHERCHE UN CLIENT AVEC PSEUDO DONNÉE
client_data* search_client(char* nick){ 	
	
	client_data* current = first;
	while(current!=NULL){
		
		if(strcmp(nick,current->nick) == 0) return current;
		current = current->next;
	}
	return NULL;
}
		

//METTRE MESSAGE DANS LA BOX
void put(mbox* box, mssg* mess){

	if(box->first == NULL && box->last == NULL){
		box->first = mess;
		box->first->next = box->last;
	}
	else if( box-> first != NULL && box->last == NULL){
		box->last=mess;
		box->first->next=box->last;
	}
	else{	
	    box->last->next = mess;
		box->last = mess;
}
}

// RÉCUPẾRER ET ENLEVER MESSAGE TÊTE DE FILE
mssg* get(mbox* box){
	
	mssg* tmp;
	
	if(box->first == NULL && box->last == NULL) return NULL;
	
	else if(box->first  != NULL && box->last == NULL){
		
		tmp = box->first;
		box->first = NULL;
		return tmp;
	}
	else {
	    tmp = box->first;
		box->first = box->first->next;
		return tmp; 
	}
}

	
//ÉCHO
int do_echo(char *args, char *resp, int resp_len){	
	
		if(args == NULL) args = "";
		snprintf(resp, resp_len, "ok %s\n",args ); 
		return 0;
		 }

//RAND
int do_rand(char *args, char *resp, int resp_len){
	int r;
	
	if(args==NULL) r = rand();
	else if(atoi(args)==0){
		snprintf(resp, resp_len, "Erreur argument du rand\n");
		return 0;
	}
	else r = rand() % atoi(args);
	
	snprintf(resp, resp_len, "rand %d\n",r);
	
	return 0;
}


//QUIT
int do_quit(char *args, char *resp, int resp_len){
	
	return -1;
}

//LISTE CLIENTS
int do_list(char *args, char *resp, int resp_len){

	
	sprintf(resp,"ok ");

	client_data* current = first;
	while(current != NULL){
		sprintf(&resp[strlen(resp)],"%s ",current->nick);
		current = current->next;
	}
	sprintf(&resp[strlen(resp)],"\n");

	return 0;
}



//CHANGE PSEUDO
int do_nick(char *args, char *resp, int resp_len, client_data *cli){
	
	if(valid_nick(args) == 0) {
		snprintf(resp, resp_len, "fail nickname unavailable\n");
		return 0;
	 }
	else{
		snprintf(cli->nick,9,"%s", args);
		snprintf(resp,resp_len,"Pseudo mis à jour\n");
		return 0;
	}
}

//FAIRE ENVOI
int do_send(char *args, char *resp, int resp_len, client_data *cli){
	
	char *pseudo, *texte;
	client_data *dest;
	
	
	pseudo = strsep(&args, " ");
	texte = args;
	
	//CHERCHE SI PSEUDO EXISTE
	if((dest=search_client(pseudo))==NULL){
		snprintf(resp, resp_len, "Utilisateur demandé introuvable\n");
		return 0;
	}
	if(texte==NULL){
		snprintf(resp, resp_len, "Erreur message vide\n");
		return 0;
	}
	
	mssg* envoi = create_msg(cli->nick, texte);

	put(dest->box, envoi);
	snprintf(resp,resp_len,"Message envoyé\n");
	return 0;
}
	
//RECEVOIR MESSAGE
int do_recv(char *args, char *resp, int resp_len, client_data *cli){
	
	mssg* recu;
	if((recu=get(cli->box))==NULL)
		snprintf(resp, resp_len, "Aucun message en attente\n");
		
	else
		snprintf(resp,resp_len,"De : %s\n	%s\n",recu->sender,recu->text);
	
	return 0;
}	
	
	
//ÉVALUE LE MESSAGE
int eval_message(char *msg, char *resp, int resp_len, client_data *cli){
	
	char *cmd, *args;
	
	cmd = strsep(&msg, " ");
	args = msg;

	if(strcmp(cmd, "echo") == 0)
		return do_echo(args, resp, resp_len);
	if(strcmp(cmd, "rand") == 0)
		return do_rand(args, resp, resp_len);
	if(strcmp(cmd, "quit") == 0)
		return do_quit(args, resp, resp_len);
	if(strcmp(cmd, "list") == 0)
		return do_list(args, resp, resp_len);
	if(strcmp(cmd, "nick") == 0)
		return do_nick(args, resp, resp_len, cli);
	if(strcmp(cmd, "send") == 0)
		return do_send(args, resp, resp_len, cli);
	if(strcmp(cmd, "recv") == 0)
		return do_recv(args, resp, resp_len, cli);
	else {
		snprintf(resp, resp_len, "fail unknown command %s\n", cmd);
		return 0;
	} 
}

	

//WORKER
void* worker(void* arg){
	
	char msg[MESSAGE_MAXLEN], response[MESSAGE_MAXLEN];
	
	client_data* i = (client_data*)arg;
	while(1){
		
		if(receive_message(i->sock, msg, MESSAGE_MAXLEN) < 0) 
			break;
		if(eval_message(msg, response, MESSAGE_MAXLEN, i) < 0) 
			break;
		if(write(i->sock, response, strlen(response)) < 0) 
			break;
	}
			close(i->sock);
			free_client(i);
			printf("Client déconnecté\n");
			printf("Nombre de clients actifs : %d\n\n",nr_clients);
		
	return arg;
}
	
//ARRIVÉE CLIENT
void client_arrived(int sock){
	
	
	client_data* cli = alloc_client(sock);
	
	if(pthread_create(&cli->thread, NULL,worker,(void*)cli) != 0)
		 perror("Erreur pthread\n");
	 
	printf("Client connecté\n\n");
	printf("Nombre de clients actifs : %d\n\n", nr_clients);
	return;
}

//ÉCOUTE PORT	
int listen_port(int port_num){
		int s, s2;
		// CRÉATION SOCKET
		if((s=socket(PF_INET6, SOCK_STREAM, 0)) == -1){
			perror("Erreur création de socket");
			return -1;
		}
		//STRUCT ADRESSE
		struct sockaddr_in6 sin;
		socklen_t size_sin = sizeof(struct sockaddr_in6);
		memset(&sin, 0, size_sin); 	
		sin.sin6_family = PF_INET6;
		sin.sin6_port=htons(port_num);	

		// LIAISON
		int optval = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	
		if(bind(s, (struct sockaddr *)&sin, size_sin) == -1){
			perror("Erreur liaison socket-structure");
			return -1;
		}
		//ECOUTE SOCKET
		if(listen(s,1024)==-1){
			perror("Erreur listen socket");
			exit(1);
		}

		while(1){
	if((s2 = accept(s, NULL, NULL)) == -1){
		perror("Erreur acceptation client");
		return -1;
	}
	else client_arrived(s2);
	

}

}



int main(int argc, char* argv[]){
	
	if(argc<2){
		perror("Invalid port\n");
		exit(1);
	}
	init_clients();
	listen_port(atoi(argv[1]));
	return 0;
	
}
	
	
