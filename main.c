#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define INITSIZE 44
#define ADDSIZE  8
#define PORT    161
#define MAXLINE 1024
#define OVERFLOW -100
#define UNDERFLOW -101
#define OUTOFMEMORY -102
#define FILE_IO_ERROR -103
#define SOCKET_ERROR -104
////////////////////////////////////////
typedef struct{
    unsigned char length;
    unsigned char top;
    unsigned char *bytes;    
} pack_t;
//prototypes////////////////////////////
pack_t*         InitPack();
pack_t*         FormPack(pack_t*, char*, char*);
void            AddToPack(pack_t *, unsigned char);
void            Resize(pack_t*);
unsigned char   Count(char*, char);
unsigned char*  RefineOid(char*);
void            PrintPack(pack_t *);
void            PrintOid(unsigned char *);
void            WriteToBin(pack_t *, const char* );
int             CreateSocket();
pack_t*         Request(int, char *, pack_t *);
unsigned char   GetDataIndex(pack_t *);
unsigned char   GetRespIndex(pack_t *);
////////////////////////////////////////

int main( int argc, char* argv[]){
    int socket;
    pack_t *my_pack = InitPack();
    pack_t *response = NULL;
    FormPack(my_pack, argv[1], argv[2]);
    printf("request:\n");
    PrintPack(my_pack);
    WriteToBin(my_pack, "request.bin");
    socket = CreateSocket();
    response = Request(socket, argv[3], my_pack);
    printf("response:\n");
    PrintPack(response);
    WriteToBin(response, "response.bin");
    printf("Response' Data index is ... %hhu\nResponse' text index is ... %hhu\n", GetDataIndex(response), GetRespIndex(response));
    for(int i = GetRespIndex(response); i < response->top; i++) printf("%c", response->bytes[i]);
    printf("\n");
    return 0;
}

//functions////////////////////////////
pack_t* InitPack(){
    pack_t *pack = NULL;
    pack = malloc(sizeof(pack_t));
    if(pack == NULL) exit(OUTOFMEMORY);
    pack->length = INITSIZE;
    pack->bytes = malloc(pack->length * sizeof(void));
    if(pack->bytes == NULL){
        free(pack);
        exit(OUTOFMEMORY);
    }
    pack->top = 0;
    return pack;
}
////////////////////////////////////////
pack_t* FormPack(pack_t *p_pack, char *p_community, char *p_oid){
    unsigned char *oid = malloc(256 * sizeof(unsigned char));
    oid = RefineOid(p_oid);
    unsigned char sizes_[4];
    int size_counter = 0;
    AddToPack(p_pack, 48);
    AddToPack(p_pack, 255);
    sizes_[size_counter++] = p_pack->top - 1;
    AddToPack(p_pack, 2);
    AddToPack(p_pack, 1);
    AddToPack(p_pack, 0);
    AddToPack(p_pack, 4);
    AddToPack(p_pack, Count(p_community,0));
    for(int i = 0; i < Count(p_community,0); i++){
        AddToPack(p_pack, p_community[i]);
    }
    AddToPack(p_pack, 160);
    AddToPack(p_pack, 255);
    sizes_[size_counter++] = p_pack->top - 1;
    AddToPack(p_pack, 2);
    AddToPack(p_pack, 2);
    AddToPack(p_pack, 0);
    AddToPack(p_pack, 1);
    AddToPack(p_pack, 2);
    AddToPack(p_pack, 1);
    AddToPack(p_pack, 0);
    AddToPack(p_pack, 2);
    AddToPack(p_pack, 1);
    AddToPack(p_pack, 0);
    AddToPack(p_pack, 48);
    AddToPack(p_pack, 255);
    sizes_[size_counter++] = p_pack->top - 1;
    AddToPack(p_pack, 48);
    AddToPack(p_pack, 255);
    sizes_[size_counter++] = p_pack->top - 1;
    AddToPack(p_pack, 6);
    AddToPack(p_pack, Count((char *)oid, 0));
    for(int i = 0; i < Count((char *)oid,0); i++){
        AddToPack(p_pack, oid[i]);
    }
    AddToPack(p_pack, 5);
    AddToPack(p_pack, 0);
    for(int i = 0; i < 4; i++){
        p_pack->bytes[sizes_[i]] = p_pack->top - 1 - sizes_[i];
    }
    free(oid);
    return p_pack;
}
////////////////////////////////////////
void AddToPack(pack_t *p_pack, unsigned char p_byte){
    if(p_pack->top < p_pack->length){
        p_pack->bytes[p_pack->top++] = p_byte;
    } else {
        Resize(p_pack);
        AddToPack(p_pack, p_byte);
    }
}
////////////////////////////////////////
void Resize(pack_t *p_pack){
    p_pack->bytes = realloc(p_pack->bytes, (p_pack->length + ADDSIZE));
    p_pack->length = p_pack->length + ADDSIZE;
}
////////////////////////////////////////
unsigned char Count(char *p_string, char p_c){
    unsigned char i = 0;
    char c = (unsigned char)p_string[i++];
    while(c != p_c){
        c = p_string[i++];        
    }
    return i-1;
}
////////////////////////////////////////
unsigned char* RefineOid(char *p_oid){
    char *input = malloc(256 * sizeof(char));
    strcpy(input, p_oid);
    unsigned char *buffer = malloc( 256 * sizeof(void) );
    
    int i = 0;
    for(; Count(input,0) ; i++){
        if(i == 0){
            unsigned char temp[2];
            sscanf(input, "%hhu.%hhu.", &temp[0],&temp[1]);
            if(temp[0] == 1 && temp[1] == 3 ){
                memmove(input, input + 4, Count(input,0) - 3);
                buffer[i] = 43;
            }
            
        } else {
            sscanf(input,"%hhu.",&buffer[i]);
            unsigned char step = Count(input, '.') + 1;
            memmove(input,input + step,Count(input,0) - step + 1);
        }
    }
    buffer[++i] = 0;
    return buffer;
}
////////////////////////////////////////
void PrintPack(pack_t *p_pack){
    for(int i = 0; i < p_pack->top; i++ ){
        printf("%02x ", p_pack->bytes[i]);
    }
    printf("\n");
}
////////////////////////////////////////
void PrintOid(unsigned char *p_oid){
    printf("%u\n", Count((char*)p_oid, 0));
}
////////////////////////////////////////
void WriteToBin(pack_t *p_pack, const char *p_filename){
    FILE *outfile = NULL;
    outfile = fopen(p_filename, "wb");
    if(outfile == NULL){
        printf("Error opening file\n");
        exit(FILE_IO_ERROR);
    } else {
        fwrite(p_pack->bytes, sizeof(unsigned char), p_pack->top, outfile);
        fclose(outfile);
    }
}
////////////////////////////////////////
int CreateSocket(){
    int sock = socket(AF_INET,SOCK_DGRAM,0);
    if(sock<0)
    {
        printf("Socket creation error!\n");
        exit(SOCKET_ERROR);
    } else {
        return sock;
    }
}
////////////////////////////////////////
pack_t* Request(int socket, char *address, pack_t *p_pack ){
    unsigned char *buffer = malloc( 256 * sizeof(unsigned char));
    unsigned int n,length;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(address);
    sendto(socket, (const char *)p_pack->bytes, p_pack->top , MSG_CONFIRM, (const struct sockaddr *) &addr, sizeof(addr));
    n = recvfrom(socket, buffer, MAXLINE, MSG_WAITALL,(struct sockaddr *) &addr, &length);
    pack_t *response = InitPack();
    for(int i = 0; i < n; i++){
        AddToPack(response, (unsigned char)buffer[i] );
    }
    close(socket);
    return response;
}
///////////////////////////////////////
unsigned char GetDataIndex(pack_t *p_pack){
    return 6 + p_pack->bytes[3] + p_pack->bytes[5 + p_pack->bytes[3]];
}
///////////////////////////////////////
unsigned char GetRespIndex(pack_t *p_pack){
    unsigned char offset = GetDataIndex( p_pack ), 
    response_id_length      = p_pack->bytes[offset + 3], 
    error_status_length     = p_pack->bytes[offset + 3 + response_id_length + 2],
    error_index_length      = p_pack->bytes[offset + 3 + response_id_length + 2 + error_status_length + 2],
    response_oid_length     = p_pack->bytes[offset + 3 + response_id_length + 2 + error_status_length + 2 + error_index_length + 6],
    response_value_length   = p_pack->bytes[offset + 3 + response_id_length + 2 + error_status_length + 2 + error_index_length + 6 + response_oid_length + 2];
    return offset + 3 + response_id_length + 2 + error_status_length + 2 + error_index_length + 6 + response_oid_length + 3; 
}
