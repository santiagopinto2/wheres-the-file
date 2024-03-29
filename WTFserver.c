#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits.h>
#include <openssl/sha.h>

typedef struct file_nodes{
	int version;
	char* path;
	char* hash;
	struct file_nodes* next;
} file_node;

void* handle_connection(void* p_client_socket);
void get_message(int client_socket, char* project_name, int file_full, char* looking_for);
void upgrade(int client_socket, char* project_name);
void create(int client_socket, char* project_name);
void destroy(int client_socket, char* project_name);
void history(int client_socket, char* project_name);
void rollback(int client_socket, char* project_name_and_version);
void get_path(char* path, char* project_name, char* extension);
file_node* parse_manifest(int file);
void get_token(char* message, char* token, char delimeter);
int get_manifest_version(char* manifest_path);
int get_int_length(int num);
int get_file_list_length(file_node* head);
int delete_directory(char* project_name);
void free_file_node(file_node* head);

int main(int argc, char** argv){
	int port = atoi(argv[1]);
	//creating socket
	int server_socket;
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if((bind(server_socket, (struct sockaddr*) &server_addr, sizeof(server_addr))) == -1){
		printf("Bind failed\n");
		return;
	}
	if((listen(server_socket, 100)) == -1){
		printf("Listen failed\n");
		return;
	}
	int client_socket, client_addr_size;
	struct sockaddr_in client_addr;
	while(1){
		client_addr_size = sizeof(struct sockaddr_in);
		if((client_socket = accept(server_socket, (struct sockaddr*) &client_addr, (socklen_t*) &client_addr_size)) == -1){
			printf("Accept failed\n");
			return;
		}
		printf("Connected to client\n\n");
		//creating threads
		pthread_t t;
		int* pclient = malloc(sizeof(int));
		*pclient = client_socket;
		pthread_create(&t, NULL, handle_connection, pclient);
	}
	close(server_socket);
	return EXIT_SUCCESS;
}

void* handle_connection(void* p_client_socket){	
	int client_socket = *((int*)p_client_socket);
	free(p_client_socket);
	char buffer[1000];
	bzero(buffer, sizeof(buffer));
	int bytes_read;
	//reads first message from client which contains the command, project name, and any other inital information
	bytes_read = read(client_socket, buffer, sizeof(buffer));
	if(bytes_read == -1){
		printf("Read failed\n");
		return NULL;
	}
	if(strstr(buffer, "checkout") != NULL)
		get_message(client_socket, strchr(buffer, ':')+1, 1, "Project");
	else if(strstr(buffer, "update") != NULL)
		get_message(client_socket, strchr(buffer, ':')+1, 0, "Manifest");
	else if(strstr(buffer, "upgrade") != NULL)
		upgrade(client_socket, strchr(buffer, ':')+1);
	else if(strstr(buffer, "create") != NULL)
		create(client_socket, strchr(buffer, ':')+1);
	else if(strstr(buffer, "destroy") != NULL)
		destroy(client_socket, strchr(buffer, ':')+1);
	else if(strstr(buffer, "currentversion") != NULL)
		get_message(client_socket, strchr(buffer, ':')+1, -1, "Manifest");
	else if(strstr(buffer, "history") != NULL)
		history(client_socket, strchr(buffer, ':')+1);
	else if(strstr(buffer, "rollback") != NULL)
		rollback(client_socket, strchr(buffer, ':')+1);
		
		
		
	/*
	 * 
	 * add other functions here
	 * 
	 * 
	 * */
	
	
	close(client_socket);
	printf("\nDisconnected from client\n");
	return NULL;
}

void get_message(int client_socket, char* project_name, int file_full, char* looking_for){
	//parses through manifest and sends it all in one message, different data is added dependning on what looking_for is set to
	int file;
	if((file = open(project_name, O_RDONLY)) == -1){
		printf("Project folder not found\n");
		write(client_socket, "Project folder not found", strlen("Project folder not found"));
		return;
	}
	close(file);
	char manifest_path[strlen(project_name)*2+11];
	get_path(manifest_path, project_name, ".Manifest");
	if((file = open(manifest_path, O_RDONLY)) == -1){
		printf("Manifest not found\n");
		return;
	}
	printf("%s found, sending over...\n\n", looking_for);
	file_node* head = parse_manifest(file); //parses through manifest and puts it all in a linked list
	close(file);
	int manifest_version = get_manifest_version(manifest_path);
	int file_list_length = get_file_list_length(head);
	int i;
	/*
	 * if contents of files are fully requested (file_full == 1) then
	 * the message that will be sent over will be in this format
	 * manifest version:# of files:file version:length of file path:file pathlength of hash:hashlength of file:filefile version...
	 * 
	 * if just manifest is requested (file_full == 0) then
	 * the message that will be sent over will be in this format
	 * manifest version:# of files:file version:length of file path:file pathlength of hash:hashfile version...
	 * 
	 * if just file paths and versions are requested (file_full == -1) then
	 * the message that will be sent over will be in this format
	 * manifest version:# of files:file version:length of file path:file pathfile version...
	 * */
	char message[1000000];
	bzero(message, sizeof(message));
	char string[get_int_length(manifest_version)+1];
	sprintf(string, "%d", manifest_version);
	strcpy(message, string);
	strcat(message, ":");
	if(head -> version == -1){
		strcat(message, "0");
		write(client_socket, message, strlen(message));
		return;
	}
	char string2[get_int_length(file_list_length)+1];
	sprintf(string2, "%d", file_list_length);
	strcat(message, string2);
	strcat(message, ":");
	file_node* tmp = head;
	for(i = 1; i <= file_list_length; i++){ //goes through each file in the manifest
		int path_length = strlen(tmp -> path);
		int hash_length = strlen(tmp -> hash);
		char string3[get_int_length(tmp -> version)+1];
		sprintf(string3, "%d", tmp -> version);
		strcat(message, string3);
		strcat(message, ":");
		char string4[get_int_length(path_length)+1];
		sprintf(string4, "%d", path_length);
		strcat(message, string4);
		strcat(message, ":");
		strcat(message, tmp -> path);
		if(file_full != -1){ //if hash is requested
			char string5[get_int_length(hash_length)+1];
			sprintf(string5, "%d", hash_length);
			strcat(message, string5);
			strcat(message, ":");
			strcat(message, tmp -> hash);
		}
		if(file_full == 1){ //if contents of each file is requested
			if((file = open(tmp -> path, O_RDONLY)) == -1){
				printf("File not found\n");
				return;
			}
			char buffer[1000000];
			bzero(buffer, sizeof(buffer));
			int bytes_read;
			bytes_read = read(file, buffer, sizeof(buffer));
			if(bytes_read == -1){
				printf("Read failed\n");
				return;
			}
			buffer[strlen(buffer)-1] = '\0';
			int buffer_length = strlen(buffer);
			char string6[get_int_length(buffer_length)+1];
			sprintf(string6, "%d", buffer_length);
			strcat(message, string6);
			strcat(message, ":");
			strcat(message, buffer);
			close(file);
		}
		tmp = tmp -> next;
	}
	write(client_socket, message, strlen(message));
	printf("%s sent\n", looking_for);
	free_file_node(head);
}

void upgrade(int client_socket, char* project_name){
	int file, manifest_file;
	char manifest_path[strlen(project_name)*2+11];
	get_path(manifest_path, project_name, ".Manifest");
	if((file = open(project_name, O_RDONLY)) == -1){
		printf("Project folder not found\n");
		write(client_socket, "Project folder not found", sizeof("Project folder not found"));
		return;
	}
	else{
		int manifest_version = get_manifest_version(manifest_path);
		char string[get_int_length(manifest_version)+1];
		sprintf(string, "%d", manifest_version);
		write(client_socket, string, strlen(string));
	}
	close(file);
	if((manifest_file = open(manifest_path, O_RDONLY)) == -1){
		printf("Manifest not found\n");
		return;
	}
	char buffer[10000], message[10000];
	int bytes_read, file_tmp;
	file_node* head = parse_manifest(manifest_file); //parses through manifest putting everything in a linked list
	close(manifest_file);
	while(1){
		bzero(buffer, sizeof(buffer));
		bytes_read = read(client_socket, buffer, sizeof(buffer));
		printf("buffer: %s\n", buffer);
		if(bytes_read == -1 || strcmp(buffer, "Upgrade done") == 0 || (file_tmp = open(buffer, O_RDONLY)) == -1){
			//server receives message to kill the loop once upgrade is finished on the client side
			if(bytes_read == -1)
				printf("Read failed\n");
			else if(strcmp(buffer, "Upgrade done") == 0)
				printf("Upgrade finished\n");
			else if((file_tmp = open(buffer, O_RDONLY)) == -1)
				printf("File not found: %s\n", buffer);
			free_file_node(head);
			return;
		}
		file_node* tmp = head;
		while(tmp != NULL && strcmp(tmp -> path, buffer) != 0)
			tmp = tmp -> next;
		if(tmp == NULL){
			printf("File not found\n");
			free_file_node(head);
			close(file_tmp);
			return;
		}
		bzero(buffer, sizeof(buffer));
		bytes_read = read(file_tmp, buffer, sizeof(buffer));
		bzero(message, sizeof(message));
		char string[get_int_length(tmp -> version)+1];
		sprintf(string, "%d", tmp -> version);
		strcpy(message, string);
		strcat(message, ":");
		if(tmp -> next == NULL)
			strcat(message, "NULL");
		else
			strcat(message, tmp -> next -> path);
		strcat(message, ":");
		strcat(message, buffer);
		//sends over requested information of a file requested by client
		write(client_socket, message, strlen(message));
		close(file_tmp);
	}
}

void create(int client_socket, char* project_name){
	struct stat st = {0};
	if(stat(project_name, &st) != -1){
		printf("Project folder already exists\n");
		write(client_socket, "Project folder already exists", sizeof("Project folder already exists"));
		return;
	}
	mkdir(project_name, S_IRWXG|S_IRWXO|S_IRWXU);
	char manifest_path[strlen(project_name)*2+11];
	get_path(manifest_path, project_name, ".Manifest");
	int manifest_file = creat(manifest_path, S_IRWXG|S_IRWXO|S_IRWXU);
	write(manifest_file, "1\n", 2);
	//writes to client the version of the new project
	write(client_socket, "1", 1);
	close(manifest_file);
	char history_path[strlen(project_name)*2+10];
	get_path(history_path, project_name, ".History");
	//creates and writes to history file
	int history_file = creat(history_path, S_IRWXG|S_IRWXO|S_IRWXU);
	write(history_file, "Project created\n", strlen("Project created\n"));
	close(history_file);
	printf("Project created\n");
}

void destroy(int client_socket, char* project_name){
	struct stat st = {0};
	if(stat(project_name, &st) == -1){
		printf("Project folder not found\n");
		write(client_socket, "Project folder not found", sizeof("Project folder not found"));
		return;
	}
	char project_name2[strlen(project_name)+2];
	strcpy(project_name2, project_name);
	strcat(project_name2, ":");
	char name_tmp[100];
	struct dirent* p;
	DIR* d = opendir("./");
	int delete_directory_result;
	//opens the directory and deletes all folders in it that are previous versions of the project
	while ((p = readdir(d)) != NULL){
		if(strstr(p -> d_name, project_name2) != NULL)
			delete_directory_result = delete_directory(p -> d_name);
	}
	closedir(d);
	delete_directory_result = delete_directory(project_name); //deletes the current project
	write(client_socket, "Project destroyed", sizeof("Project destroyed"));
	printf("Project destroyed\n");
}

void history(int client_socket, char* project_name){
	struct stat st = {0};
	if(stat(project_name, &st) == -1){
		printf("Project folder not found\n");
		write(client_socket, "Project folder not found", sizeof("Project folder not found"));
		return;
	}
	char history_path[strlen(project_name)*2+10];
	get_path(history_path, project_name, ".History");
	int history_file;
	if((history_file = open(history_path, O_RDONLY)) == -1){
		printf("History not found\n");
		write(client_socket, "History not found", sizeof("History not found"));
		return;
	}
	char buffer[10000];
	bzero(buffer, sizeof(buffer));
	int bytes_read = read(history_file, buffer, sizeof(buffer));
	//sends over the history file as in by itself in one message exactly the way it's written
	write(client_socket, buffer, strlen(buffer));
	printf("History sent\n");
}

void rollback(int client_socket, char* project_name_and_version){
	struct stat st = {0};
	if(stat(project_name_and_version, &st) == -1){
		printf("Project folder with given version not found\n");
		write(client_socket, "Project folder with given version not found", sizeof("Project folder with given version not found"));
		return;
	}
	char project_name_and_version_copy[strlen(project_name_and_version)+1];
	strcpy(project_name_and_version_copy, project_name_and_version);
	char project_name[strchr(project_name_and_version, ':')-project_name_and_version+1];
	get_token(project_name_and_version, project_name, ':');
	int version = atoi(project_name_and_version);
	if(stat(project_name, &st) == -1){
		printf("Project folder not found\n");
		write(client_socket, "Project folder not found", sizeof("Project folder not found"));
		return;
	}
	delete_directory(project_name); //deletes current project version
	char name_tmp[100];
	struct dirent* p;
	DIR* d = opendir("./");
	//opens the directory and deletes all folders in it with the format "projectname:version#" where version number is greater than the version being rolled back to
	while ((p = readdir(d)) != NULL){
		if(strchr(p -> d_name, ':') != NULL){
			bzero(name_tmp, sizeof(name_tmp));
			strcpy(name_tmp, p -> d_name);
			char project_name_tmp[strchr(name_tmp, ':')-name_tmp];
			get_token(name_tmp, project_name_tmp, ':');
			int version_tmp = atoi(name_tmp);
			if(version_tmp > version)
				delete_directory(p -> d_name);
		}
		
	}
	closedir(d);
	rename(project_name_and_version_copy, project_name); //renames the rollbacked project to just "projectname" to now be the current project
	write(client_socket, "Project was reverted", sizeof("Project was reverted"));
	printf("Project was reverted\n");
}

void get_path(char* path, char* project_name, char* extension){ //gets the full path name of a file
	strcpy(path, project_name);
	strcat(path, "/");
	strcat(path, project_name);
	strcat(path, extension);
}

file_node* parse_manifest(int file){ //parses through the manifest and puts everything in a linked list
	char buffer[1000000];
	bzero(buffer, sizeof(buffer));
	int bytes_read;
	bytes_read = read(file, buffer, sizeof(buffer));
	if(bytes_read == -1){
		printf("Read failed\n");
		return NULL;
	}
	file_node* head = (file_node*)malloc(sizeof(file_node));
	char buffer_tmp[strlen(strchr(buffer, '\n'))+1];
	strcpy(buffer_tmp, strchr(buffer, '\n')+1);
	bzero(buffer, strlen(buffer));
	strcpy(buffer, buffer_tmp);
	if(strchr(buffer, '\n') == NULL){
		head -> version = -1;
		head -> path = NULL;
		head -> hash = NULL;
		head -> next = NULL;
	}
	else{
		int count = 0;
		file_node* tmp = head;
		while(strchr(buffer, '\n') != NULL){
			char* token = malloc(strchr(buffer, '\n')-buffer+1);
			if(count%3 == 0){
				get_token(buffer, token, ' ');
				tmp -> version = atoi(token);
			}
			else if(count%3 == 1){
				get_token(buffer, token, ' ');
				tmp -> path = token;
			}
			else if(count%3 == 2){
				get_token(buffer, token, '\n');
				tmp -> hash = token;
				if(strchr(buffer, '\n') == NULL){
					tmp -> next = NULL;
					return head;
				}
				file_node* new_file_node = (file_node*)malloc(sizeof(file_node));
				tmp -> next = new_file_node;
				tmp = tmp -> next;
			}
			count++;
		}
	}
	return head;
}

void get_token(char* message, char* token, char delimeter){
	//takes a full message usually sent by the server and separates the first section of it with the rest of the message to get each token
	char* copy = malloc(strlen(message)+1);
	strcpy(copy, message);
	copy[strchr(message, delimeter)-message] = '\0';
	int i;
	for(i = 0; i <= strlen(copy); i++)
		token[i] = copy[i];
	char* message_tmp = malloc(strlen(strchr(message, delimeter))+1);
	strcpy(message_tmp, strchr(message, delimeter)+1);
	bzero(message, strlen(message));
	strcpy(message, message_tmp);
}

int get_manifest_version(char* manifest_path){ //returns the version number of a manifest file
	int file; 
	if((file = open(manifest_path, O_RDONLY)) == -1)
		return -1;
	int flag;
	int version = 0;
	char buffer;
	while((flag = read(file, &buffer, sizeof(buffer))) > 0){
		if(buffer == '\n'){
			close(file);
			return version;
		}
		version = version*10+buffer-48;
	}
	close(file);
	return -2;
}

int get_int_length(int num){
	int a=1;
	while(num>9){
		a++;
		num/=10;
	}
	return a;
}

int get_file_list_length(file_node* head){ //returns the length of a number
	int count = 1;
	file_node* tmp = head;
	while(tmp -> next != NULL){
		count++;
		tmp = tmp -> next;
	}
	return count;
}

int delete_directory(char* project_name){ //deletes a directory along with everything inside of it recursively
	DIR* d = opendir(project_name);
    size_t path_len = strlen(project_name);
    int r = -1;
    if(d){
		struct dirent* p;
		r = 0;
		while(!r && (p = readdir(d))){
			int r2 = -1;
			char* buf;
			size_t len;
			if (strcmp(p -> d_name, ".") == 0 || strcmp(p -> d_name, "..") == 0)
				continue;
			len = path_len + strlen(p -> d_name) + 2; 
			buf = malloc(len);
			if(buf){
				struct stat statbuf;
				snprintf(buf, len, "%s/%s", project_name, p -> d_name);
				if(!stat(buf, &statbuf)){
					if(S_ISDIR(statbuf.st_mode))
						r2 = delete_directory(buf);
					else
						r2 = unlink(buf);
				}
				free(buf);
			}
			r = r2;
		}
		closedir(d);
    }
    if(!r)
		r = rmdir(project_name);
    return r;
}

void free_file_node(file_node* head){ //frees a linked list
	file_node* tmp;
	while(head!=NULL){
		tmp = head;
		head = head -> next;
		free(tmp);
	}
}
