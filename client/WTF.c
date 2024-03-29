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

typedef struct update_nodes{
	char action;
	char* path;
	struct update_nodes* next;
} update_node;

void configure(char* string_ip, char* string_port);
void checkout(int network_socket, char* project_name);
void update(int network_socket, char* project_name);
void update_write(int file, char c, char* path, char* hash, int* has_update);
void upgrade(int network_socket, char* project_name);
update_node* parse_update(int file);
void create(int network_socket, char* project_name);
void destroy(int network_socket, char* project_name);
void add(char* project_name, char* file_name);
void remove_func(char* project_name, char* file_name);
void currentversion(int network_socket, char* project_name);
void history(int network_socket, char* project_name);
void rollback(int network_socket, char* project_name, char* version);
void commit(int network_socket, char* project_name);
void get_path(char* path, char* project_name, char* extension);
file_node* parse_manifest(char* manifest_path);
void parse_manifest_nodes(char* manifest_path, int manifest_version, file_node* head);
file_node* parse_message(char* message);
void get_token(char* message, char* token, char delimeter);
int get_manifest_version(char* manifest_path);
int get_int_length(int num);
int get_file_list_length(file_node* head);
void free_file_node(file_node* head);
void free_update_node(update_node* head);

int main(int argc, char** argv){
	if(strcmp(argv[1], "configure") == 0)
		configure(argv[2], argv[3]);
	else if(strcmp(argv[1], "add") == 0)
		add(argv[2], argv[3]);
	else if(strcmp(argv[1], "remove") == 0)
		remove_func(argv[2], argv[3]);
	else{
		int ip_port_file=open("ip_port.configure", O_RDONLY);
		if(ip_port_file == -1){
			printf("Configure file not found\n");
			return EXIT_FAILURE;
		}
		char buffer[100];
		bzero(buffer, 100);
		int bytes_read;
		bytes_read = read(ip_port_file, buffer, sizeof(buffer));
		if(bytes_read == -1){
			printf("Fetching the IP and port failed\n");
			return EXIT_FAILURE;
		}
		int n_index = strchr(buffer, '\n')-buffer;
		int n_size = sizeof(strchr(buffer, '\n'));
		char ip[n_index+1];
		strncpy(ip, buffer, n_index);
		ip[n_index] = '\0';
		char port[n_size];
		strncpy(port, buffer+n_index+1, n_size);
		port[n_size] = '\0';
		int network_socket;
		if((network_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
			printf("Socket creation failed\n");
			return EXIT_FAILURE;
		}
		struct sockaddr_in server_address;
		server_address.sin_family = AF_INET;
		server_address.sin_port = htons(atoi(port));
		if((inet_pton(AF_INET, ip, &(server_address.sin_addr))) == -1){
			printf("Invalid address\n");
			return EXIT_FAILURE;
		}
		int connection_status = connect(network_socket, (struct sockaddr*) &server_address, sizeof(server_address));
		if(connection_status == -1){
			printf("Connection failed\n");
			return EXIT_FAILURE;
		}
		printf("Connected to server\n\n");
		
		
		
	
		if(strcmp(argv[1], "checkout") == 0)
			checkout(network_socket, argv[2]);
		else if(strcmp(argv[1], "update") == 0)
			update(network_socket, argv[2]);
		else if(strcmp(argv[1], "upgrade") == 0)
			upgrade(network_socket, argv[2]);
		else if(strcmp(argv[1], "create") == 0)
			create(network_socket, argv[2]);
		else if(strcmp(argv[1], "destroy") == 0)
			destroy(network_socket, argv[2]);
		else if(strcmp(argv[1], "currentversion") == 0)
			currentversion(network_socket, argv[2]);
		else if(strcmp(argv[1], "history") == 0)
			history(network_socket, argv[2]);
		else if(strcmp(argv[1], "rollback") == 0)
			rollback(network_socket, argv[2], argv[3]);
		else if (strcmp(argv[1], "commit") == 0)
			commit(network_socket, argv[2]);
		
		
		/*
		 * 
		 * 
		 * add other functions here
		 * 
		 * 
		 * */
		
		
		
			
		close(network_socket);
		printf("\nDisconnected from server\n");
	}
	return EXIT_SUCCESS;
}

void configure(char* string_ip, char* string_port){
	int ip_port_file = creat("./ip_port.configure", S_IRWXG|S_IRWXO|S_IRWXU);
	int write_int;
	write_int = write(ip_port_file, string_ip, strlen(string_ip));
	write_int = write(ip_port_file, "\n", 1);
	write_int = write(ip_port_file, string_port, strlen(string_port));
	printf("Configure finished\n");
}

void checkout(int network_socket, char* project_name){
	struct stat st = {0};
	if(stat(project_name, &st) != -1){
		printf("Folder already exists client side\n");
		return;
	}
	char buffer[strlen(project_name)+10];
	strcpy(buffer, "checkout:");
	strcat(buffer, project_name);
	write(network_socket, buffer, sizeof(buffer));
	char message[1000000];
	bzero(message, sizeof(message));
	int bytes_read;
	bytes_read = read(network_socket, message, sizeof(message));
	if(bytes_read == -1){
		printf("Read failed\n");
		return;
	}
	if(strcmp(message, "Project folder not found") == 0){
		printf("Project folder not found server side\n");
		return;
	}
	mkdir(project_name, S_IRWXG|S_IRWXO|S_IRWXU);
	printf("Project received, storing...\n");
	get_path(buffer, project_name, ".Manifest");
	int manifest_file = creat(buffer, S_IRWXG|S_IRWXO|S_IRWXU);
	//the message that will be sent over will be in this format
	//manifest version:# of files:file version:length of file path:file pathlength of hash:hashlength of file:filefile version...
	char manifest_version[strchr(message, ':')-message+1];
	get_token(message, manifest_version, ':');
	write(manifest_file, manifest_version, strlen(manifest_version));
	if(strlen(message) == 1){
		printf("Finished storing project\n");
		return;
	}
	char file_list_length_string[strchr(message, ':')-message+1];
	get_token(message, file_list_length_string, ':');
	int file_list_length = atoi(file_list_length_string);
	int i, j;
	for(i = 1; i <= file_list_length; i++){
		char file_version[strchr(message, ':')-message+1];
		get_token(message, file_version, ':');
		write(manifest_file, "\n", 1);
		write(manifest_file, file_version, strlen(file_version));
		char file_path_length_string[strchr(message, ':')-message+1];
		get_token(message, file_path_length_string, ':');
		int file_path_length = atoi(file_path_length_string);
		char file_path_and_hash_length[strchr(message, ':')-message+1];
		get_token(message, file_path_and_hash_length, ':');
		char file_path[file_path_length+1];
		strncpy(file_path, file_path_and_hash_length, file_path_length);
		file_path[file_path_length] = '\0';
		write(manifest_file, " ", 1);
		write(manifest_file, file_path, strlen(file_path));
		char hash_length_string[strlen(file_path_and_hash_length)-file_path_length+1];
		for(j = file_path_length; j < strlen(file_path_and_hash_length); j++)
			hash_length_string[j-file_path_length] = file_path_and_hash_length[j];
		int hash_length = atoi(hash_length_string);
		char hash_and_file_length[strchr(message, ':')-message+1];
		get_token(message, hash_and_file_length, ':');
		char hash[hash_length+1];
		strncpy(hash, hash_and_file_length, hash_length);
		hash[hash_length] = '\0';
		write(manifest_file, " ", 1);
		write(manifest_file, hash, strlen(hash));
		char file_length_string[strlen(hash_and_file_length)-hash_length+1];
		for(j = hash_length; j < strlen(hash_and_file_length); j++)
			file_length_string[j-hash_length] = hash_and_file_length[j];
		int file_length = atoi(file_length_string);
		if(i == file_list_length){
			write(manifest_file, "\n", 1);
			int new_file = creat(file_path, S_IRWXG|S_IRWXO|S_IRWXU);
			write(new_file, message, strlen(message));
			close(new_file);
		}
		else{
			char file_and_file_version[strchr(message, ':')-message+1];
			for(j = 0; message[j] != ':'; j++)
				file_and_file_version[j] = message[j];
			file_and_file_version[strchr(message, ':')-message] = '\0';
			char file_full[file_length+1];
			strncpy(file_full, file_and_file_version, file_length);
			file_full[file_length] = '\0';
			int new_file = creat(file_path, S_IRWXG|S_IRWXO|S_IRWXU);
			write(new_file, file_full, file_length);
			close(new_file);
			char tmp[strlen(message)+1];
			bzero(tmp, sizeof(tmp));
			for(j = file_length; j < strlen(message); j++)
				tmp[j-file_length] = message[j];
			bzero(message, strlen(message));
			strcpy(message, tmp);
		}	
	}
	close(manifest_file);
	printf("Finished storing project\n");
}

void update(int network_socket, char* project_name){
	char manifest_path[strlen(project_name)*2+11];
	get_path(manifest_path, project_name, ".Manifest");
	int manifest_file, bytes_read, i, has_conflict = 0, has_update = 0;
	if((manifest_file = open(manifest_path, O_RDONLY)) == -1){
		printf("Client manifest not found\n");
		return;
	}
	close(manifest_file);
	file_node* client_head = parse_manifest(manifest_path);
	char buffer[strlen(project_name)+8];
	strcpy(buffer, "update:");
	strcat(buffer, project_name);
	write(network_socket, buffer, sizeof(buffer));
	char* message = malloc(10000);
	bzero(message, sizeof(message));
	i = 0;
	char buffer2;
	while((bytes_read = read(network_socket, &buffer2, sizeof(buffer2))) > 0){
		message[i] = buffer2;
		i++;
	}
	if(bytes_read == -1){
		printf("Read failed\n");
		free_file_node(client_head);
		return;
	}
	if(strcmp(message, "Project folder not found") == 0){
		printf("Project folder not found server side\n");
		return;
	}
	char* update_file_path = malloc(strlen(project_name)*2+9);
	get_path(update_file_path, project_name, ".Update");
	int update_file = creat(update_file_path, S_IRWXG|S_IRWXO|S_IRWXU);
	char* conflict_file_path = malloc(strlen(project_name)*2+11);
	get_path(conflict_file_path, project_name, ".Conflict");
	//the message that will be sent over will be in this format
	//manifest version:# of files:file version:length of file path:file pathlength of hash:hashfile version...
	char* manifest_version_string = malloc(strchr(message, ':')-message+1);
	get_token(message, manifest_version_string, ':');
	int manifest_version = atoi(manifest_version_string);
	if(get_manifest_version(manifest_path) == manifest_version){
		printf("Up to date\n");
		remove(conflict_file_path);
		close(update_file);
		free_file_node(client_head);
		return;
	}
	printf("Starting project comparison...\n\n");
	if(client_head -> version == -1){
		printf("Finished comparison, no differences found besides manifest version\n");
		remove(conflict_file_path);
		close(update_file);
		free_file_node(client_head);
		return;
	}
	//int file_list_length = get_file_list_length(head);
	int conflict_file = creat(conflict_file_path, S_IRWXG|S_IRWXO|S_IRWXU);
	file_node* server_head = parse_message(message);
	/*file_node* tmp4 = client_head;
	while(tmp4!= NULL){
		printf("version: %d\npath: %s\nhash: %s\n", tmp4->version, tmp4->path, tmp4->hash);
		tmp4=tmp4->next;
	}
	printf("\n");
	file_node* tmp3 = server_head;
	while(tmp3!= NULL){
		printf("version: %d\npath: %s\nhash: %s\n", tmp3->version, tmp3->path, tmp3->hash);
		tmp3=tmp3->next;
	}*/
	file_node* client_tmp = client_head;
	file_node* server_tmp = server_head;
	while(client_tmp != NULL){
		if(strcmp(client_tmp -> path, server_tmp -> path) != 0){
			file_node* server_tmp2 = server_tmp -> next;
			int delete_or_add = 0;
			while(server_tmp2 != NULL){
				if(strcmp(server_tmp2 -> path, client_tmp -> path) == 0){
					delete_or_add = 1;
					break;
				}
				server_tmp2 = server_tmp2 -> next;
			}
			if(has_conflict){
				if(delete_or_add)
					server_tmp = server_tmp -> next;
				else
					client_tmp = client_tmp -> next;
			}
			else{
				if(delete_or_add){
					update_write(update_file, 'A', server_tmp -> path, server_tmp -> hash, &has_update);
					server_tmp = server_tmp -> next;
				}
				else{
					update_write(update_file, 'D', client_tmp -> path, client_tmp -> hash, &has_update);
					client_tmp = client_tmp -> next;
				}
			}
		}
		else{
			unsigned char* buf = malloc(SHA_DIGEST_LENGTH);
			int file = open(client_tmp -> path, O_RDONLY);
			bzero(message, sizeof(message));
			read(file, message, sizeof(message));
			char* client_local_hash = malloc(SHA_DIGEST_LENGTH*2);
			SHA1(message, strlen(message), buf);
			for (i = 0; i < SHA_DIGEST_LENGTH; i++)
				sprintf((char*)&(client_local_hash[i*2]), "%02x", buf[i]);
			printf("message: %s\n", message);
		//	printf("client local hash: %s\n", client_local_hash);
		//	printf("client hash: %s\n", client_tmp -> hash);
		//	printf("server hash: %s\n", server_tmp -> hash);
			if(strcmp(client_tmp -> hash, server_tmp -> hash) != 0 && strcmp(client_tmp -> hash, client_local_hash) != 0){
				if(!has_conflict)
					printf("\nConflict found, removing update file\n");
				//printf("server hash: %s\nclient hash: %s\nlive hash: %s\n", server_tmp->hash, client_tmp->hash, client_local_hash);
				update_write(conflict_file, 'C', client_tmp -> path, client_local_hash, &has_conflict);	
			}
			else if(!has_conflict && client_tmp -> version != server_tmp -> version
			&& strcmp(client_tmp -> hash, server_tmp -> hash) != 0 && strcmp(client_tmp -> hash, client_local_hash) == 0)
				update_write(update_file, 'M', client_tmp -> path, client_tmp -> hash, &has_update);
			client_tmp = client_tmp -> next;
			server_tmp = server_tmp -> next;
		}
	}
	if(server_tmp != NULL && !has_conflict){
		while(server_tmp != NULL){
			update_write(update_file, 'A', server_tmp -> path, server_tmp -> hash, &has_update);
			server_tmp = server_tmp -> next;
		}
	}
	
	
	/*
	char data[] = "hello world";
	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1(data, strlen(data), hash);
	int i;
	char buf[SHA_DIGEST_LENGTH*2];
	for (i = 0; i < SHA_DIGEST_LENGTH; i++){
        sprintf((char*)&(buf[i*2]), "%02x", hash[i]);
    }
    printf("hash: %s\n", buf);
    * 
    * 
    * compile like
    * gcc WTF.c -o WTF -pthread -lssl -lcrypto
    * */
    
    free_file_node(client_head);
    free_file_node(server_head);
    close(update_file);
    close(conflict_file);
    if(has_conflict){
		remove(update_file_path);
		printf("\nFinished comparison, conflicts were found and must be resolved\nbefore project can be updated\n");
	}
    else{
		remove(conflict_file_path);
		if(has_update)
			printf("\nFinished comparison, updates were found\n");
		else
			printf("Finished comparison, no differences found besides manifest version\n");
	}
}

void update_write(int file, char c, char* path, char* hash, int* has_update_or_conflict){
	*has_update_or_conflict = 1;
	write(file, &c, 1);
	write(file, " ", 1);
	write(file, path, strlen(path));
	write(file, " ", 1);
	write(file, hash, strlen(hash));
	write(file, "\n", 1);
	printf("%c %s\n", c, path);
}

void upgrade(int network_socket, char* project_name){
	struct stat st = {0};
	if(stat(project_name, &st) == -1){
		printf("Project not found\n");
		return;
	}
	char conflict_path[strlen(project_name)*2+11];
	get_path(conflict_path, project_name, ".Conflict");
	if(stat(conflict_path, &st) != -1){
		printf("Conflict file found, first resolve all conflicts and then update again\n");
		return;
	}
	char update_path[strlen(project_name)*2+9];
	get_path(update_path, project_name, ".Update");
	int update_file;
	if((update_file = open(update_path, O_RDONLY)) == -1){
		printf("Update file not found, first perform an update\n");
		close(update_file);
		return;
	}
	char buffer[strlen(project_name)+9];
	strcpy(buffer, "upgrade:");
	strcat(buffer, project_name);
	write(network_socket, buffer, sizeof(buffer));
	char message[1000];
	bzero(message, sizeof(message));
	int i = 0, bytes_read;
	bytes_read = read(network_socket, message, sizeof(message));
	if(bytes_read == -1){
		printf("Read failed\n");
		return;
	}
	if(strcmp(message, "Project folder not found") == 0){
		printf("Project folder not found server side\n");
		return;
	}
	int manifest_version;
	manifest_version = atoi(message);
	update_node* update_head = parse_update(update_file);
	close(update_file);
	if(update_head -> action == 'Z'){
		("No changes were found in update file\n");
		remove(update_path);
		return;
	}
	printf("Starting update changes...\n");
	char manifest_path[strlen(project_name)*2+11];
	get_path(manifest_path, project_name, ".Manifest");
	int manifest_file;
	if((manifest_file = open(manifest_path, O_RDONLY)) == -1){
		printf("Client manifest not found\n");
		return;
	}
	close(manifest_file);
	file_node* manifest_head = parse_manifest(manifest_path);
	update_node* update_tmp = update_head;
	char buffer2;
	while(update_tmp != NULL){
		if(update_tmp -> action == 'D'){
			file_node* manifest_tmp = manifest_head;
			if(strcmp(manifest_tmp -> path, update_tmp -> path) == 0){
				manifest_head = manifest_head -> next;
			}
			else{
				while(manifest_tmp -> next != NULL){
					if(strcmp(manifest_tmp -> next -> path, update_tmp -> path) == 0){
						manifest_tmp -> next = manifest_tmp -> next -> next;
						break;
					}
					manifest_tmp = manifest_tmp ->next;
				}
			}
			remove(update_tmp -> path);
		}
		else if(update_tmp -> action == 'A' || update_tmp -> action == 'M'){
			char buffer[strlen(update_tmp -> path)+1];
			strcpy(buffer, update_tmp -> path);
			write(network_socket, buffer, strlen(buffer));
			bzero(message, sizeof(message));
			bytes_read = read(network_socket, message, sizeof(message));
			int file_tmp = creat(update_tmp -> path, S_IRWXG|S_IRWXO|S_IRWXU);
			char file_version_string[strchr(message, ':')-message+1];
			get_token(message, file_version_string, ':');
			int file_version = atoi(file_version_string);
			char file_next_path[strchr(message, ':')-message+1];
			get_token(message, file_next_path, ':');
			write(file_tmp, message, strlen(message));
			unsigned char* buf = malloc(SHA_DIGEST_LENGTH);
			bzero(buf, sizeof(buf));
			char* new_hash = malloc(SHA_DIGEST_LENGTH*2);
			SHA1(message, strlen(message), buf);
			for (i = 0; i < SHA_DIGEST_LENGTH; i++)
				sprintf((char*)&(new_hash[i*2]), "%02x", buf[i]);
			file_node* new_file_node = (file_node*)malloc(sizeof(file_node));
			new_file_node -> version = file_version;
			new_file_node -> path = malloc(strlen(update_tmp -> path)+1);
			strcpy(new_file_node -> path, update_tmp -> path);
			new_file_node -> hash = malloc(strlen(new_hash)+1);
			strcpy(new_file_node -> hash, new_hash);
			new_file_node -> hash[strlen(new_hash)-1] = '\0';
			file_node* manifest_tmp = manifest_head;
			if(update_tmp -> action == 'A'){
				if(strcmp(file_next_path, "NULL") == 0){
					while(manifest_tmp -> next != NULL)
						manifest_tmp = manifest_tmp -> next;
					new_file_node -> next = NULL;
					manifest_tmp -> next = new_file_node;
				}
				else{
					while(manifest_tmp -> next != NULL){
						if(strcmp(manifest_tmp -> next -> path, file_next_path) == 0){
							new_file_node -> next = manifest_tmp -> next;
							manifest_tmp -> next = new_file_node;
							break;
						}
						manifest_tmp = manifest_tmp -> next;
					}
				}
			}
			else if(update_tmp -> action == 'M'){
				while(strcmp(manifest_tmp -> next -> path, update_tmp -> path) != 0)
					manifest_tmp = manifest_tmp -> next;
				new_file_node -> next = manifest_tmp -> next -> next;
				manifest_tmp -> next = new_file_node;
			}
			close(file_tmp);
		}
		update_tmp = update_tmp -> next;
	}
	write(network_socket, "Upgrade done", sizeof("Upgrade done"));
	parse_manifest_nodes(manifest_path, manifest_version, manifest_head);
	remove(update_path);
	free_file_node(manifest_head);
	free_update_node(update_head);
	printf("Finished update changes\n");
}

update_node* parse_update(int file){
	char* buffer = malloc(1000);
	bzero(buffer, sizeof(buffer));
	int bytes_read, i = 0;
	char buffer2;
	while((bytes_read = read(file, &buffer2, sizeof(buffer2))) > 0){
		buffer[i] = buffer2;
		i++;
	}
	if(bytes_read == -1){
		printf("Read failed\n");
		return NULL;
	}
	update_node* head = (update_node*)malloc(sizeof(update_node));
	if(strchr(buffer, '\n') == NULL){
		head -> action = 'Z';
		head -> path = NULL;
		head -> next = NULL;
		return head;
	}
	int count = 0;
	update_node* tmp = head;
	while(strchr(buffer, '\n') != NULL){
		if(count%2 == 0){
			char* token = malloc(strchr(buffer, ' ')-buffer+1);
			get_token(buffer, token, ' ');
			tmp -> action = token[0];
		}
		else if(count%2 == 1){
			char* token = malloc(strchr(buffer, '\n')-buffer+1);
			get_token(buffer, token, ' ');
			tmp -> path = token;
			char dummy[strchr(buffer, '\n')-buffer+1];
			get_token(buffer, dummy, '\n');
			if(strchr(buffer, '\n') == NULL){
				tmp -> next = NULL;
				return head;
			}
			update_node* new_update_node = (update_node*)malloc(sizeof(update_node));
			tmp -> next= new_update_node;
			tmp = tmp -> next;
		}
		count++;
	}
	return head;
}

void create(int network_socket, char* project_name){
	char buffer[strlen(project_name)+8];
	strcpy(buffer, "create:");
	strcat(buffer, project_name);
	write(network_socket, buffer, sizeof(buffer));
	char message[200];
	bzero(message, sizeof(message));
	int bytes_read = read(network_socket, message, sizeof(message));
	if(bytes_read == -1){
		printf("Read failed\n");
		return;
	}
	if(strstr(message, "Project folder already exists") != NULL){
		printf("Project folder already exists server side\n");
		return;
	}
	printf("Creating project...\n");
	char manifest_version_string[strlen(message)+1];
	strcpy(manifest_version_string, message);
	mkdir(project_name, S_IRWXG|S_IRWXO|S_IRWXU);
	char manifest_path[strlen(project_name)*2+11];
	get_path(manifest_path, project_name, ".Manifest");
	int manifest_file = creat(manifest_path, S_IRWXG|S_IRWXO|S_IRWXU);
	write(manifest_file, manifest_version_string, strlen(manifest_version_string));
	write(manifest_file, "\n", 1);
	close(manifest_file);
	printf("Finished creating project\n");
}

void destroy(int network_socket, char* project_name){
	char buffer[strlen(project_name)+9];
	strcpy(buffer, "destroy:");
	strcat(buffer, project_name);
	write(network_socket, buffer, sizeof(buffer));
	char message[200];
	bzero(message, sizeof(message));
	int bytes_read = read(network_socket, message, sizeof(message));
	if(bytes_read == -1){
		printf("Read failed\n");
		return;
	}
	if(strcmp(message, "Project folder not found") == 0)
		printf("Project folder not found server side\n");
	else
		printf("Project destroyed\n");
}

void add(char* project_name, char* file_name){
	struct stat st = {0};
	if(stat(project_name, &st) == -1){
		printf("Project folder not found\n");
		return;
	}
	char file_path[strlen(project_name)+strlen(file_name)+2];
	strcpy(file_path, project_name);
	strcat(file_path, "/");
	strcat(file_path, file_name);
	char manifest_path[strlen(project_name)*2+11];
	get_path(manifest_path, project_name, ".Manifest");
	int file, manifest_file, i;
	if((file = open(file_path, O_RDONLY)) == -1){
		printf("File not found\n");
		return;
	}
	if((manifest_file = open(manifest_path, O_RDWR | O_APPEND)) == -1){
		printf("Client manifest not found\n");
		return;
	}
	char manifest_message[10000];
	bzero(manifest_message, sizeof(manifest_message));
	read(manifest_file, manifest_message, sizeof(manifest_message));
	if(strstr(manifest_message, file_path) != NULL){
		close(file);
		close(manifest_file);
		printf("File already exists in manifest\n");
		return;
	}
	char message[10000];
	bzero(message, sizeof(message));
	read(file, message, sizeof(message));
	unsigned char* buf = malloc(SHA_DIGEST_LENGTH);
	bzero(buf, sizeof(buf));
	char* file_hash = malloc(SHA_DIGEST_LENGTH*2);
	SHA1(message, strlen(message), buf);
	file_hash[strlen(file_hash)-1] = '\0';
	for (i = 0; i < SHA_DIGEST_LENGTH; i++)
		sprintf((char*)&(file_hash[i*2]), "%02x", buf[i]);
	write(manifest_file, "1 ", 2);
	write(manifest_file, file_path, strlen(file_path));
	write(manifest_file, " ", 1);
	write(manifest_file, file_hash, strlen(file_hash));
	write(manifest_file, "\n", 1);
	close(file);
	close(manifest_file);
	printf("File added to manifest\n");
}

void remove_func(char* project_name, char* file_name){
	struct stat st = {0};
	if(stat(project_name, &st) == -1){
		printf("Project folder not found\n");
		return;
	}
	char file_path[strlen(project_name)+strlen(file_name)+2];
	strcpy(file_path, project_name);
	strcat(file_path, "/");
	strcat(file_path, file_name);
	char manifest_path[strlen(project_name)*2+11];
	get_path(manifest_path, project_name, ".Manifest");
	int file, manifest_file;
	if((file = open(file_path, O_RDONLY)) == -1){
		printf("File not found\n");
		return;
	}
	if((manifest_file = open(manifest_path, O_RDONLY)) == -1){
		printf("Client manifest not found\n");
		return;
	}
	char manifest_message[10000];
	bzero(manifest_message, sizeof(manifest_message));
	read(manifest_file, manifest_message, sizeof(manifest_message));
	if(strstr(manifest_message, file_path) == NULL){
		close(file);
		close(manifest_file);
		printf("File not found in manifest\n");
		return;
	}
	close(manifest_file);
	file_node* head = parse_manifest(manifest_path);
	if(strcmp(head -> path, file_path) == 0)
		head = head -> next;
	else{
		file_node* tmp = head;
		while(strcmp(tmp -> next -> path, file_path) != 0)
			tmp = tmp -> next;
		tmp -> next = tmp -> next -> next;
	}
	parse_manifest_nodes(manifest_path, get_manifest_version(manifest_path), head);
	close(file);
	close(manifest_file);
	free_file_node(head);
	printf("File removed from manifest\n");
}

void currentversion(int network_socket, char* project_name){
	char buffer[strlen(project_name)+16];
	strcpy(buffer, "currentversion:");
	strcat(buffer, project_name);
	write(network_socket, buffer, sizeof(buffer));
	char message[10000];
	bzero(message, sizeof(message));
	int bytes_read = read(network_socket, message, sizeof(message));
	if(bytes_read == -1){
		printf("Read failed\n");
		return;
	}
	if(strcmp(message, "Project folder not found") == 0){
		printf("Project folder not found server side\n");
		return;
	}
	printf("Printing project...\n");
	char manifest_version[strchr(message, ':')-message+1];
	get_token(message, manifest_version, ':');
	printf("Manifest version: %s\n", manifest_version);
	if(strlen(message) == 1){
		printf("Finished printing project\n");
		return;
	}
	char file_list_length_string[strchr(message, ':')-message+1];
	get_token(message, file_list_length_string, ':');
	int file_list_length = atoi(file_list_length_string);
	int i, j;
	//the message that will be sent over will be in this format
	//manifest version:# of files:file version:length of file path:file pathfile version...
	for(i = 1; i <= file_list_length; i++){
		char file_version[strchr(message, ':')-message+1];
		get_token(message, file_version, ':');
		char file_path_length_string[strchr(message, ':')-message+1];
		get_token(message, file_path_length_string, ':');
		int file_path_length = atoi(file_path_length_string);
		if(i == file_list_length){
			char short_file_path[strlen(message)-strlen(project_name)];
			get_token(message, short_file_path, '/');
			printf("File: %s version %s\n", message, file_version);
			printf("Finished printing project\n");
			return;
		}
		char file_path_and_file_version[strchr(message, ':')-message+1];
		for(j = 0; message[j] != ':'; j++)
			file_path_and_file_version[j] = message[j];
		file_path_and_file_version[strchr(message, ':')-message] = '\0';
		char file_path[file_path_length];
		strncpy(file_path, file_path_and_file_version, file_path_length);
		file_path[file_path_length] = '\0';
		char tmp[strlen(message)+1];
		bzero(tmp, sizeof(tmp));
		for(j = file_path_length; j < strlen(message); j++)
			tmp[j-file_path_length] = message[j];
		bzero(message, strlen(message));
		strcpy(message, tmp);
		char short_file_path[strlen(file_path)-strlen(project_name)];
		get_token(file_path, short_file_path, '/');
		printf("File: %s version %s\n", file_path, file_version);
	}
}

void history(int network_socket, char* project_name){
	char buffer[strlen(project_name)+9];
	strcpy(buffer, "history:");
	strcat(buffer, project_name);
	write(network_socket, buffer, sizeof(buffer));
	char message[10000];
	bzero(message, sizeof(message));
	int bytes_read = read(network_socket, message, sizeof(message));
	if(bytes_read == -1){
		printf("Read failed\n");
		return;
	}
	if(strcmp(message, "Project folder not found") == 0){
		printf("Project folder not found server side\n");
		return;
	}
	printf("Printing history...\n\n");
	printf("%s", message);
	printf("Finished printing history\n");
}

void rollback(int network_socket, char* project_name, char* version){
	char buffer[strlen(project_name)+strlen(version)+11];
	strcpy(buffer, "rollback:");
	strcat(buffer, project_name);
	strcat(buffer, ":");
	strcat(buffer, version);
	write(network_socket, buffer, sizeof(buffer));
	char message[50];
	bzero(message, sizeof(message));
	int bytes_read = read(network_socket, message, sizeof(message));
	if(bytes_read == -1){
		printf("Read failed\n");
		return;
	}
	if(strcmp(message, "Project folder not found") == 0)
		printf("Project folder not found server side\n");
	else if(strcmp(message, "Project folder with given version not found") == 0)
		printf("Project folder with given version not found server side\n");
	else if(strcmp(message, "Project was reverted") == 0)
		printf("Project was reverted\n");
}

void commit(int network_socket, char* project_name){
	
	struct stat st = {0};
	if(stat(project_name, &st) == -1){
		printf("Project not found\n");
		return;
	}
	
	char buffer[strlen(project_name)+8];
	strcpy(buffer, "commit:");
	strcat(buffer, project_name);
	write(network_socket, buffer, sizeof(buffer));
	char message[1000];
	bzero(message, sizeof(message));
	int i = 0, bytes_read;
	bytes_read = read(network_socket, message, sizeof(message));
	if(bytes_read == -1){
		printf("Read failed\n");
		return;
	}
	if(strstr(message, "Project folder not found") != NULL){
		printf("Project folder not found server side\n");
		return;
	}
	
	if (strstr(message, "Manifest not found") != NULL) {
		printf("Manifest not found\n");
		return;
	}
	
	//printf("%s\n", message);
	
	char conflict_path[strlen(project_name)*2+11];
	get_path(conflict_path, project_name, "Conflict");
	if(stat(conflict_path, &st) != -1){
		printf("Conflict file found, first resolve all conflicts and then update again\n");
		return;
	}
	
	char update_path[strlen(project_name)*2+9];
	get_path(update_path, project_name, "Update");
	int update_file;
	if((update_file = open(update_path, O_RDONLY)) != -1){
		char buffertest[1];
		bytes_read = read(update_file, buffertest, 1);
		if (bytes_read == -1) {
			printf("Read Failed\n");
			close(update_file);
			return;
		}
		if (bytes_read > 0) {
			printf("Nonempty update file found, make required updates before committing again\n");
			close(update_file);
			return;
		}
	}
	close(update_file);
	
	int x = 0;
	while (message[x] != ':') {
		x++;
	}
	char versionstr[10];
	bzero(versionstr, 10);
	strncat(versionstr, message, x);
	int server_manifest_version = atoi(versionstr);
	
	char manifest_path[strlen(project_name)*2+11];
	get_path(manifest_path, project_name, ".Manifest");
	int client_manifest_version = get_manifest_version(manifest_path);
	
	if (server_manifest_version != client_manifest_version) {
		printf("Client and server manifest versions do not match, update local repository before committing\n");
		return;
	}
	file_node* server_manifest = parse_message(message);
	
	printf("got past here\n");
	file_node* client_manifest = parse_manifest(manifest_path);
	
	file_node* sptr = server_manifest;
	file_node* cptr = client_manifest;
	
	char commit_path[strlen(project_name)*2+11];
	get_path("./", project_name, ".Commit");
	int commit_file = creat(commit_path, S_IRWXG|S_IRWXO|S_IRWXU);
	int write_int;
	
	while (cptr != NULL) {//search for modifies and adds
		sptr = server_manifest;
		int found = 0;
		while (sptr != NULL) {
			if (strcmp(cptr->hash, sptr->hash) == 0) {
				found = 1;
				break;
			}
			sptr = sptr->next;
		}
		if (!found) {
			char committxt[1000];
			strcat(committxt, "A ");
			strcat(committxt, cptr->path);
			strcat(committxt, " ");
			strcat(committxt, sptr->hash);
			strcat(committxt, "\0");
			write_int = write(commit_file, committxt, strlen(committxt));
			write_int = write(commit_file, "\n", 1);
			printf("A %s\n", cptr->path);
			break;
		}
		
		int file = open(cptr->path, O_RDONLY);
		char filecontents[10000];
		bzero(filecontents, sizeof(filecontents));
		read(file, filecontents, sizeof(filecontents));
		unsigned char* buf = malloc(SHA_DIGEST_LENGTH);
		bzero(buf, sizeof(buf));
		char* file_hash = malloc(SHA_DIGEST_LENGTH*2);
		SHA1(filecontents, strlen(filecontents), buf);
		file_hash[strlen(file_hash)-1] = '\0';
		printf("%s ", buf);
		for (i = 0; i < SHA_DIGEST_LENGTH; i++)
			sprintf((char*)&(file_hash[i*2]), "%02x", buf[i]);
		printf("%s\n", buf);
		close(file);
		if (strcmp(cptr->hash, buf) != 0) {
			if (sptr->version < cptr->version) {
				char committxt[1000];
				strcat(committxt, "M ");
				strcat(committxt, cptr->path);
				strcat(committxt, " ");
				strcat(committxt, sptr->hash);
				strcat(committxt, "\0");
				write_int = write(commit_file, committxt, strlen(committxt));
				write_int = write(commit_file, "\n", 1);
				printf("M %s\n", cptr->path);
				break;
			} else {
				remove(commit_path);
				printf("Client must update before committing any changes\n");
				return;
			}
		}
		cptr = cptr->next;
	}
	
	sptr = server_manifest;
	while (sptr != NULL) {
		cptr = client_manifest;
		int found = 0;
		while(cptr != NULL) {
			if (strcmp(cptr->hash, sptr->hash) == 0) {
				found = 1;
				break;
			}
			cptr = cptr->next;
		}
		if (!found) {
			if (sptr->version <= client_manifest_version) {
				char committxt[1000];
				strcat(committxt, "D ");
				strcat(committxt, cptr->path);
				strcat(committxt, " ");
				strcat(committxt, sptr->hash);
				strcat(committxt, "\0");
				write_int = write(commit_file, committxt, strlen(committxt));
				write_int = write(commit_file, "\n", 1);
				printf("D %s\n", cptr->path);
				break;
			} else {
				remove(commit_path);
				printf("Client must synch with server before committing any changes\n");
				return;
			}
		}
		sptr = sptr->next;
	}
	
/*	
//	- compare manifest versions//
	- run through client manifest and assign hashes
	- use the live hashes to find changed files
//	- generate .Commit file//
	- compare hashes
		- modify
			- server and client have the same file
			- stored hashes are the same
			- live hash is different from the stored hash
//		- add//
//			- file isn't in the server manifest, but it's in the client manifest//
//		- delete
//			- server manifest has the file, but client manifest doesn't
		- output to .Commit file and stdout
	- if everything follows those rules, send commit file to server
		- server saves it as an active commit
		- report success
//	- if a file is in the server's manifest with a different hash than the client's whose version number is not lower than the client's 
//		- commit fails with a message stating that the repository has to be synced
*/
}

void get_path(char* path, char* project_name, char* extension){
	strcpy(path, project_name);
	strcat(path, "/");
	strcat(path, project_name);
	strcat(path, extension);
}

file_node* parse_manifest(char* manifest_path){
	int file = open(manifest_path, O_RDONLY);
	char* buffer = malloc(10000);
	bzero(buffer, sizeof(buffer));
	int bytes_read, i = 0;
	char buffer2;
	while((bytes_read = read(file, &buffer2, sizeof(buffer2))) > 0){
		buffer[i] = buffer2;
		i++;
	}
	if(bytes_read == -1){
		close(file);
		printf("Read failed\n");
		return NULL;
	}
	file_node* head = (file_node*)malloc(sizeof(file_node));
	char* buffer_tmp = malloc(strlen(strchr(buffer, '\n'))+1);
	strcpy(buffer_tmp, strchr(buffer, '\n')+1);
	bzero(buffer, strlen(buffer));
	strcpy(buffer, buffer_tmp);
	if(strchr(buffer, '\n') == NULL){
		head -> version = -1;
		head -> path = NULL;
		head -> hash = NULL;
		head -> next = NULL;
		return head;
	}
	int count = 0;
	file_node* tmp = head;
	while(strchr(buffer, '\n') != NULL){
		if(count%3 == 0){
			char* token = malloc(strchr(buffer, ' ')-buffer+1);	
			get_token(buffer, token, ' ');
			tmp -> version = atoi(token);
		}
		else if(count%3 == 1){
			char* token = malloc(strchr(buffer, ' ')-buffer+1);	
			get_token(buffer, token, ' ');
			tmp -> path = token;
		}
		else if(count%3 == 2){
			char* token = malloc(strchr(buffer, '\n')-buffer+1);	
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
	close(file);
	return head;
}

void parse_manifest_nodes(char* manifest_path, int manifest_version, file_node* head){
	int manifest_file = creat(manifest_path, S_IRWXG|S_IRWXO|S_IRWXU);
	char string[get_int_length(manifest_version)+1];
	sprintf(string, "%d", manifest_version);
	write(manifest_file, string, get_int_length(manifest_version));
	write(manifest_file, "\n", 1);
	file_node* tmp = head;
	while(tmp != NULL){
		char string2[get_int_length(tmp -> version)+1];
		sprintf(string2, "%d", tmp -> version);
		write(manifest_file, string2, get_int_length(tmp -> version));
		write(manifest_file, " ", 1);
		write(manifest_file, tmp -> path, strlen(tmp -> path));
		write(manifest_file, " ", 1);
		write(manifest_file, tmp -> hash, strlen(tmp -> hash));
		write(manifest_file, "\n", 1);
		tmp = tmp -> next;
	}
	close(manifest_file);
}

file_node* parse_message(char* message){
	file_node* head = (file_node*)malloc(sizeof(file_node));
	file_node* tmp = head;
	char* file_list_length_string = malloc(strchr(message, ':')-message+1);
	get_token(message, file_list_length_string, ':');
	int file_list_length = atoi(file_list_length_string);
	int i, j;
	for(i = 1; i <= file_list_length; i++){
		tmp -> next = NULL;
		//the message that will be sent over will be in this format
		//manifest version:# of files:file version:length of file path:file pathlength of hash:hashfile version...
		char* file_version_string = malloc(strchr(message, ':')-message+1);
		get_token(message, file_version_string, ':');
		int file_version = atoi(file_version_string);
		tmp -> version = file_version;
		char* file_path_length_string = malloc(strchr(message, ':')-message+1);
		get_token(message, file_path_length_string, ':');
		int file_path_length = atoi(file_path_length_string);
		char* file_path_and_hash_length = malloc(strchr(message, ':')-message+1);
		get_token(message, file_path_and_hash_length, ':');
		char* file_path = malloc(file_path_length+1);
		strncpy(file_path, file_path_and_hash_length, file_path_length);
		file_path[file_path_length] = '\0';
		tmp -> path = file_path;
		if(i == file_list_length){
			char* hash = malloc(strlen(message)+1);
			strncpy(hash, message, strlen(message));
			hash[strlen(message)] = '\0';
			tmp -> hash = hash;
			return head;
		}
		char* hash_length_string = malloc(strlen(file_path_and_hash_length)-file_path_length+1);
		for(j = file_path_length; j < strlen(file_path_and_hash_length); j++)
			hash_length_string[j-file_path_length] = file_path_and_hash_length[j];
		int hash_length = atoi(hash_length_string);
		char* hash_and_file_version = malloc(strchr(message, ':')-message+1);
		for(j = 0; message[j] != ':'; j++)
			hash_and_file_version[j] = message[j];
		hash_and_file_version[strchr(message, ':')-message] = '\0';
		char* hash = malloc(hash_length+1);
		strncpy(hash, hash_and_file_version, hash_length);
		hash[hash_length] = '\0';
		tmp -> hash = hash;
		char* tmp2 = malloc(strlen(message)+1);
		bzero(tmp2, sizeof(tmp2));
		for(j = hash_length; j < strlen(message); j++)
			tmp2[j-hash_length] = message[j];
		bzero(message, strlen(message));
		strcpy(message, tmp2);
		file_node* new_file_node = (file_node*)malloc(sizeof(file_node));
		tmp -> next = new_file_node;
		tmp = tmp -> next;
	}
}

void get_token(char* message, char* token, char delimeter){
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

int get_manifest_version(char* manifest_path){
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

int get_file_list_length(file_node* head){
	int count = 1;
	file_node* tmp = head;
	while(tmp -> next != NULL){
		count++;
		tmp = tmp -> next;
	}
	return count;
}

void free_file_node(file_node* head){
	file_node* tmp;
	while(head!=NULL){
		tmp = head;
		head = head -> next;
		free(tmp);
	}
}

void free_update_node(update_node* head){
	update_node* tmp;
	while(head!=NULL){
		tmp = head;
		head = head -> next;
		free(tmp);
	}
}
