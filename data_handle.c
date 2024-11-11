#include<stdint.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "http-server.h"

#define MAX_USERNAME_LENGTH 50
#define MAX_MESSAGE_LENGTH 200

char const* HTTP_200_OK = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";

uint8_t hex_to_byte(char c) {
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	return -1;

}

typedef struct Reaction {
    char user[MAX_USERNAME_LENGTH];
    char message[MAX_MESSAGE_LENGTH];
}Reaction;

typedef struct Chat {
    uint32_t id;
    char user[MAX_USERNAME_LENGTH];
    char message[MAX_MESSAGE_LENGTH];
    char timestamp[50];
    uint32_t num_reactions;
    Reaction** reactions_arr;
}Chat;


int chat_cnt=0;
int current_id=0;
int cur_size=10;

Chat** chat_array=NULL;

char* get_time(){
    char* buffer=malloc(50);
    time_t now = time(NULL);
    struct tm *tm_info=(struct tm *)malloc(sizeof(struct tm));
    localtime_r(&now,tm_info);
    strftime(buffer,50,"%F %H:%M:%S",tm_info);
    return buffer;
}


int type_parser(char* path){
   int res=0;
   if (path[0]=='/'){
    path++;
   }
   if (strncmp(path, "chats", 5) == 0) {
        res = 1;
    } else if (strncmp(path, "post", 4) == 0) {
        res = 2;
    } else if (strncmp(path, "react", 5) == 0) {
        res = 3;
    } else if (strncmp(path, "reset", 5) == 0) {
        res = 4;
    }
    return res;
}


uint8_t add_chat(char* username, char* message){
    int new_id=current_id+1;
    current_id+=1;
    char* timestamp=get_time();
    Chat* new_chat=malloc(sizeof(Chat));
    new_chat->id=new_id;
    strncpy(new_chat->user,username,MAX_USERNAME_LENGTH);
    strncpy(new_chat->message,message,MAX_MESSAGE_LENGTH);
    strncpy(new_chat->timestamp,timestamp,50);
    
    new_chat->num_reactions=0;
    
    chat_array=realloc(chat_array,(chat_cnt) * (sizeof(Chat*)));
    chat_array[chat_cnt]=new_chat;
    chat_cnt++;
    return 0;
}

uint8_t add_reaction(char* username, char* message, char* id){
 Chat* found_chat=NULL;
 for (int i=0; i<chat_cnt;i++){
    if (chat_array[i]->id==(uint32_t)atoi(id)){
        found_chat=chat_array[i];
        break;
    }
 }
 if (found_chat==NULL){
    return 1;
 }
  Reaction* new_reaction=malloc(sizeof(Reaction));
  strncpy(new_reaction->user,username,MAX_USERNAME_LENGTH);
  strncpy(new_reaction->message,message,MAX_MESSAGE_LENGTH);
  found_chat->reactions_arr=realloc(found_chat->reactions_arr,(found_chat->num_reactions)*(sizeof(Reaction*)));
  found_chat->reactions_arr[found_chat->num_reactions]=new_reaction;
    found_chat->num_reactions+=1;
  return 0;
 
}

void reset(){
    if (chat_array!=NULL){
        for (int i=0;i<chat_cnt;i++){
            if (chat_array[i]!=NULL){
                for (int j=0;j<(int)(chat_array[i]->num_reactions);j++){
                    free(chat_array[i]->reactions_arr[j]);
                }
            }
            free(chat_array[i]);
        }
        free(chat_array);
    }
    chat_cnt = 0;
    current_id = 0;
}

void respond_with_chat(int client){
    write(client, HTTP_200_OK, strlen(HTTP_200_OK));
    
    char buffer[512];
    for (int i=0; i<chat_cnt; i++){
        snprintf(buffer,512,"[#%d %s] %s: %s\n",chat_array[i]->id,chat_array[i]->timestamp,chat_array[i]->user,chat_array[i]->message);
        write(client, buffer, strlen(buffer));
        for (int j=0; j<(int)chat_array[i]->num_reactions;j++){
            snprintf(buffer, sizeof(buffer), "   (%s) %s\n", chat_array[i]->reactions_arr[j]->user, chat_array[i]->reactions_arr[j]->message);
            write(client,buffer,strlen(buffer));
        }
    }
}

void handle_post(char* path, int client){
  
    char* user_str="user=";
    char* message_str="message=";
    char* user_loc=strstr(path,user_str);
    char* message_loc=strstr(path,message_str);

    char* username_start = user_loc + strlen(user_str);
    char* username_end = message_loc - 2;
    char* message_start = message_loc + strlen(message_str);
    char* message_end = path + strlen(path); 

    char* username=malloc(MAX_USERNAME_LENGTH);
    strncpy(username, username_start, username_end - username_start + 1);

    char* message_content=malloc(MAX_MESSAGE_LENGTH);
    strncpy(message_content, message_start, message_end - message_start);


    add_chat(username,message_content);
    respond_with_chat(client);

}

void handle_reaction(char* path, int client){
    char* user_str="user=";
    char* message_str="message=";
    char* id_str="id=";

    char* user_loc=strstr(path,user_str);
    char* message_loc=strstr(path,message_str);
    char* id_loc=strstr(path,id_str);

    char* username_start=user_loc+strlen(user_str);
    char* username_end=message_loc-2;
    char* message_start=message_loc+strlen(message_str);
    char* message_end=id_loc-2;
    char* id_start=id_loc+strlen(id_str);
    char* id_end=path+strlen(path);

    char username[MAX_USERNAME_LENGTH];
    strncpy(username, username_start, username_end - username_start + 1);
    username[username_end - username_start + 1] = '\0';

    char message_content[MAX_MESSAGE_LENGTH];
    strncpy(message_content, message_start, message_end - message_start+1);
    message_content[message_end - message_start+1] = '\0'; 

    char id_output[5];
    strncpy(id_output,id_start,id_end - id_start);
    id_output[id_end - id_start]='\0';

    add_reaction(username,message_content,id_output);
    respond_with_chat(client);
    
}

void handle_response(char *request, int client_socket) {
    char* firstSpace = strchr(request, ' '); 
    firstSpace+=1;
    char* secondSpace = strchr(firstSpace, ' ');
    char* path=malloc(secondSpace-firstSpace+1);
    strncpy(path,firstSpace,secondSpace-firstSpace);
    path[secondSpace-firstSpace]='\0';
    char* path2=malloc(secondSpace-firstSpace+1);
    int i=0;
    int i_2=0;
    while (*(path+i)!=0){
        if (*(path+i)=='%'){
            uint8_t a=hex_to_byte(*(path+i+1));
            uint8_t b=hex_to_byte(*(path+i+2));
            path2[i_2]=(unsigned char)((a<< 4)| b);
            i_2++;
            i+=2;
        }else{
            path2[i_2]=path[i];
            i_2++;
        }
        i++;

    }

    int type= type_parser(path2);
    if (type==1){
        respond_with_chat(client_socket);
    }else if(type==2){
        handle_post(path2,client_socket);
    }else if(type==3){
        handle_reaction(path2,client_socket);
    }else{
        reset();
    }
    
}

int main(int argc, char* argv[]) {
    int port=0;
    if (argc>=2){
        port=atoi(argv[1]);
    }
    start_server(&handle_response, port);
}
