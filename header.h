#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define VERSION 0
#define ERROR_STATUS 0
#define ERROR_INDEX 0
#define SEQUENCE 48
#define PDU_GET_REQUEST 160
#define PDU_GET_RESPONSE 162
#define INTEGER 2
#define OCTET_STRING 4
#define NULV 5
#define OID 6
#define COUNTER32 65
#define ISO 43
#define INITSIZE 8
#define ADDSIZE  8
#define PORT    161
#define MAXLINE 1024
#define OVERFLOW -100
#define UNDERFLOW -101
#define OUTOFMEMORY -102
#define FILE_IO_ERROR -103
#define SOCKET_ERROR -104
///////////////////////////////////////
typedef struct{
    unsigned char length;
    unsigned char top;
    unsigned char *bytes;    
} pack_t;
//prototypes///////////////////////////
pack_t*         InitPack();
void            AddToPack(pack_t *, unsigned char);
void            Resize(pack_t*);
unsigned char   Count(char*, char);
unsigned char*  RefineOid(char*);
void            PrintPack(pack_t *);
void            WriteToBin(pack_t *, const char* );
int             CreateSocket();
pack_t*         Request(int, char *, pack_t *);
pack_t*         PackOid(char *);
pack_t*         PackNull();
pack_t*         PackInt(unsigned int);
pack_t*         PackOctString(char *);
pack_t*         PackSequence(unsigned char, int num, ...);
pack_t*         PackSNMPGetRequest(char*, char*);
unsigned char*  UnPackSequence(pack_t *);
unsigned int    UnPackInteger(pack_t *);
unsigned char*  UnPackOctString(pack_t *);
unsigned char*  UnPackNull(pack_t *);
unsigned char*  UnPackOid(pack_t *);
pack_t*         RePack(unsigned char *, int);
////////////////////////////////////////
pack_t* InitPack(){
    pack_t *pack = NULL;
    pack = malloc(sizeof(pack_t));
    if(pack == NULL) exit(OUTOFMEMORY);
    else{
        pack->length = INITSIZE;
        pack->bytes = malloc(pack->length * sizeof(void));
        if(pack->bytes == NULL){
            free(pack);
            exit(OUTOFMEMORY);
        }
    }
    pack->top = 0;
    return pack;
}
////////////////////////////////////////
/*  Length: 3 ways to encode it
            7 6 5 4 3 2 1 0
    short   0 [---length--]     * will implement this first
    long    1 [len-of-len-]
    indef   0 0 0 0 0 0 0 0     -- value ends with 2 NULL bytes  */
pack_t* PackInt(unsigned int p_value){
    pack_t *result = InitPack();
    unsigned char byte_counter = 0;
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
    va_start(argptr,p_num);
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
    if(p_pack->length == 0) {
        exit(UNDERFLOW);
    } else if(p_pack->top > p_pack->length){
        exit(OVERFLOW);
    } else if(p_pack->top < p_pack->length){
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
////////////////////////////////////////
unsigned char* UnPackSequence(pack_t *p_pack){
    unsigned char* result = malloc(256 * sizeof(unsigned char*));
    if(p_pack->bytes[0] == SEQUENCE ||  p_pack->bytes[0] == PDU_GET_REQUEST || p_pack->bytes[0] == PDU_GET_RESPONSE){
        for(unsigned int i = 2; i < p_pack->bytes[1] + 2;){
            switch (p_pack->bytes[i]) {
                case INTEGER:       printf("Integer   : %d\n", UnPackInteger  (RePack( (unsigned char *)p_pack->bytes + i + 2, p_pack->bytes[i+1]     ))); i += (p_pack->bytes[i+1] + 2); break;
                case COUNTER32:     printf("Counter32 : %d\n", UnPackInteger  (RePack( (unsigned char *)p_pack->bytes + i + 2, p_pack->bytes[i+1]     ))); i += (p_pack->bytes[i+1] + 2); break;
                case OCTET_STRING:  printf("OctString : %s\n", UnPackOctString(RePack( (unsigned char *)p_pack->bytes + i + 2, p_pack->bytes[i+1]     ))); i += (p_pack->bytes[i+1] + 2); break;
                case NULV:          i += 2;  break ;
                case OID:           printf("OID       : %s\n", UnPackOid      (RePack( (unsigned char *)p_pack->bytes + i + 2, p_pack->bytes[i+1]     ))); i += (p_pack->bytes[i+1] + 2); break;
                case SEQUENCE: case PDU_GET_REQUEST: case PDU_GET_RESPONSE:
                                    printf("%s",              UnPackSequence  (RePack( (unsigned char *)p_pack->bytes + i,     p_pack->bytes[i+1] + 2 ))); i += (p_pack->bytes[i+1] + 2); break;
                default:    i++;
            }                
        }
    }
    return result;
}
////////////////////////////////////////
unsigned int  UnPackInteger(pack_t *p_pack){
    int res = 0;
    for(int i = p_pack->top, k = 0; i > 0 ; i--, k++){
        res += pow( 256,k ) * p_pack->bytes[i-1];
    }
    return res;
}
////////////////////////////////////////
unsigned char* UnPackOctString(pack_t *p_pack){
    unsigned char* result = malloc( 256*sizeof(unsigned char)  );
    for(int i = 0; i < p_pack->top; i++) result[i] = p_pack->bytes[i];
    return result;
}
////////////////////////////////////////
unsigned char* UnPackNull(pack_t *p_pack){    
    return NULL;
}
////////////////////////////////////////
unsigned char* UnPackOid(pack_t *p_pack){    
    unsigned char* result = malloc( 256*sizeof(unsigned char)  );
    for(int i = 0; i < p_pack->top; i++) 
        sprintf(
            result + strlen(result), 
            i == 0 && p_pack->bytes[i] == ISO?"%s.":i == (p_pack->top - 1)?"%u":"%u.",
            i == 0 && p_pack->bytes[i] == ISO?"1.3":p_pack->bytes[i]
        );
    return result;

}
////////////////////////////////////////
pack_t* RePack(unsigned char *p_ptr, int p_length){
    pack_t *result = InitPack();
    for(int i = 0; i < p_length; i++){
        AddToPack(result, *(p_ptr+i));
    }
    return result;
}
