#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define VERSION 0
#define ERROR_STATUS 0
#define ERROR_INDEX 0
#define SEQUENCE 48
#define PDU_GET_REQUEST 160
#define INITSIZE 8
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
void            AddToPack(pack_t *, unsigned char);
void            Resize(pack_t*);
unsigned char   Count(char*, char);
unsigned char*  RefineOid(char*);
void            PrintPack(pack_t *);
void            PrintOid(unsigned char *);
void            WriteToBin(pack_t *, const char* );
int             CreateSocket();
pack_t*         Request(int, char *, pack_t *);
unsigned char   GetRespIndex(pack_t *);
pack_t*         PackOid(char *);
pack_t*         PackNull();
pack_t*         PackInt(unsigned int);
pack_t*         PackOctString(char *);
pack_t*         PackSequence(unsigned char, int num, ...);
pack_t*         PackSNMPGetRequest(char*, char*);
////////////////////////////////////////

int main( int argc, char* argv[]){
    pack_t *request = InitPack(), *response = InitPack();
    request =  PackSNMPGetRequest(argv[1],argv[2]);
    printf(">> ");
    for(int i = 0; i < request->top; i++) printf("%02X ", request->bytes[i]);
    printf("\n<< ");
    WriteToBin(request, "request.bin");
    int socket = CreateSocket();
    response = Request(socket, argv[3], request);
    for(int i = 0; i < response->top; i++) printf("%02X ", response->bytes[i]);
    printf("\n");
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
/*
    Length: 3 ways to encode it
            7 6 5 4 3 2 1 0
    short   0 [---length--]     * will implement this first
    long    1 [len-of-len-]
    indef   0 0 0 0 0 0 0 0     -- value ends with 2 NULL bytes
*/
pack_t* PackInt(unsigned int p_value){
    pack_t *result = InitPack();
    unsigned char byte_counter = 0;
    //unsigned char *ptr = (unsigned char *)&p_value;
    if(p_value & 4278190080)                byte_counter = 4;
    else if (p_value & 16711680)            byte_counter = 3;
    else if (p_value & 65280)               byte_counter = 2;
    else if (p_value & 255 || p_value == 0) byte_counter = 1;
    AddToPack(result, 2);
    AddToPack(result, byte_counter & 127);
    for(unsigned char i = 0; i < byte_counter; i++){
        AddToPack(result, *((unsigned char *)&p_value - i - 1 + byte_counter));
    }
    return result;
}
////////////////////////////////////////
pack_t* PackSequence(unsigned char p_token, int p_num,...){
    pack_t *result = InitPack(), *buffer = InitPack();
    va_list argptr;
    va_start(argptr,num);
    for( ;p_num; p_num--){
        pack_t *ptr = va_arg(argptr, pack_t*);
        for(unsigned char i = 0; i < ptr->top; i++){
            AddToPack(buffer, ptr->bytes[i]);
        }
    }
    AddToPack(result, p_token);
    AddToPack(result, buffer->top);
    for(int i = 0; i < buffer->top; i++) AddToPack(result,buffer->bytes[i]);
    return result;
}
////////////////////////////////////////
pack_t* PackNull(){
    pack_t *pack = InitPack();
    AddToPack(pack, 5);
    AddToPack(pack, 0);
    return pack;
}
////////////////////////////////////////
pack_t* PackOid(char *p_oid){
    pack_t *packedOid = InitPack();
    unsigned char *refinedOid = RefineOid(p_oid);
    AddToPack(packedOid, 6);
    AddToPack(packedOid, Count(refinedOid, 0));
    for(unsigned char i = 0; i < Count(refinedOid,0); i++ ){
        AddToPack(packedOid, refinedOid[i]);
    }
    return packedOid;
}
////////////////////////////////////////
pack_t* PackOctString(char *p_value){
    pack_t *result = InitPack();
    unsigned char length = Count(p_value,0);
    AddToPack(result, 4);
    AddToPack(result, length);
    for(unsigned char i = 0; i < length;i++) AddToPack(result, p_value[i]);
    return result;
}
////////////////////////////////////////
pack_t* PackSNMPGetRequest(char *p_community, char* p_oid){
    return PackSequence(
        SEQUENCE, 3, PackInt(VERSION), PackOctString(p_community), PackSequence(
            PDU_GET_REQUEST, 4, PackInt(1), PackInt(ERROR_STATUS), PackInt(ERROR_INDEX),PackSequence(
                SEQUENCE, 1, PackSequence(
                    SEQUENCE, 2, PackOid(p_oid), PackNull()
                )
            )
        )
    );
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
    p_pack->bytes = realloc(p_pack->bytes, (p_pack->length + ADDSIZE) * sizeof(unsigned char));
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
pack_t* Request(int p_socket, char *p_address, pack_t *p_pack ){
    unsigned char *buffer = malloc( 256 * sizeof(unsigned char));
    unsigned int n,length;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(p_address);
    sendto(p_socket, (const char *)p_pack->bytes, p_pack->top , MSG_CONFIRM, (const struct sockaddr *) &addr, sizeof(addr));
    n = recvfrom(p_socket, buffer, MAXLINE, MSG_WAITALL,(struct sockaddr *) &addr, &length);
    pack_t *response = InitPack();
    for(int i = 0; i < n; i++){
        AddToPack(response, (unsigned char)buffer[i] );
    }
    close(p_socket);
    return response;
}
///////////////////////////////////////
unsigned char GetRespIndex(pack_t *p_pack){
//TODO: de-serialize pack in correct way
    unsigned char offset    = 3 + p_pack->bytes[3] + 2 + p_pack->bytes[3 + p_pack->bytes[3] + 2] + 1, 
    response_id_length      = p_pack->bytes[offset + 3], 
    error_status_length     = p_pack->bytes[offset + 3 + response_id_length + 2],
    error_index_length      = p_pack->bytes[offset + 3 + response_id_length + 2 + error_status_length + 2],
    response_oid_length     = p_pack->bytes[offset + 3 + response_id_length + 2 + error_status_length + 2 + error_index_length + 6];
    //response_value_length   = p_pack->bytes[offset + 3 + response_id_length + 2 + error_status_length + 2 + error_index_length + 6 + response_oid_length + 2];
    return offset + 3 + response_id_length + 2 + error_status_length + 2 + error_index_length + 6 + response_oid_length + 3; 
}
