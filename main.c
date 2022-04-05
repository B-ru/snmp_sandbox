/*
    Программа производит опрос хоста по протоколу SNMPv1.
    Последовательность аргументов программы:
            1. community
            2. OID
            3. ip

    "Иерархия в пакете"
        1. контейнер, длиной 2a         30 2a
        1.1. INTEGER, длиной 01         02 01
        1.1.1. содержимое элемента      00                                  (version)
        1.2. OCTET STRING, длиной 06    04 06
        1.2.1.  содержимое элемента     70 75 62 6с 69 63                   (community)
        1.3.    DATA, длиной 1d         a0 1d
        1.3.1.  INTEGER, длиной 02      02 02
        1.3.1.1.содержимое элемента     00 a5                               (request-id)
        1.3.2.  INTEGER, длиной 01      02 01
        1.3.2.1.содержимое элемента     00                                  (error-status)
        1.3.3.  INTEGER, длиной 01      02 01
        1.3.3.1.содержимое элемента     00                                  (error-index)
        1.3.4.  контейнер, длиной 11    30 11
        1.3.4.1 контейнер, длиной 0f    30 0f
        1.3.4.1.1.OBJECT IDENTIFIER, длиной 0b       
                                        06 0b
        1.3.4.1.1.1.содержимое          2b 06 01 02 01 19 03 02 01 03 01    (modified OID)
        1.3.4.1.2.NULL, длиной 00       05 00
        
*/
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
////////////////////
typedef struct{
    unsigned char length;
    unsigned char top;
    unsigned char *bytes;    
} pack_t;
//prototypes////////
pack_t* InitPack();
pack_t* FormPack(pack_t*, char*, char*);
void    AddToPack(pack_t *, unsigned char);
unsigned char Count(char*, char);
unsigned char* RefineOid(char*);
void    PrintPack(pack_t *);
void    PrintOid(unsigned char *);
void    WriteToBin(pack_t *);
int     CreateSocket();
void    Request(int, char*, pack_t*);
////////////////////
int main( int argc, char* argv[]){
    int socket;
    printf("community:\t%s\noid:\t\t%s\n", argv[1], argv[2]);   
    pack_t *my_pack = InitPack();
    FormPack(my_pack, argv[1], argv[2]);
    PrintPack(my_pack);
    WriteToBin(my_pack);
    socket = CreateSocket();
    Request(socket, argv[3], my_pack);
    return 0;
}
//functions/////////
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

pack_t* FormPack(pack_t *p_pack, char *p_community, char *p_oid){
    unsigned char *oid = malloc(256 * sizeof(unsigned char));
    oid = RefineOid(p_oid);
    //PrintOid(oid);
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
    AddToPack(p_pack, Count(oid, 0));
    for(int i = 0; i < Count(oid,0); i++){
        AddToPack(p_pack, oid[i]);
    }
    AddToPack(p_pack, 5);
    AddToPack(p_pack, 0);
    for(int i = 0; i < 4; i++){
        fprintf(stderr, "%d\t%d\n", sizes_[i], p_pack->top-2);
        p_pack->bytes[sizes_[i]] = p_pack->top - 1 - sizes_[i];
    }
    free(oid);
    return p_pack;
}

void AddToPack(pack_t *p_pack, unsigned char p_byte){
    p_pack->bytes[p_pack->top++] = p_byte;
}

unsigned char Count(char *p_string, char p_c){
    unsigned char i = 0;
    char c = p_string[i++];
    while(c != p_c){
        c = p_string[i++];        
    }
    return i-1;
}

unsigned char* RefineOid(char *p_oid){
    char *input = malloc(256 * sizeof(char));
    strcpy(input, p_oid);
    unsigned char *buffer = malloc( 256 * sizeof(void) );
    
    int i = 0;
    for(; Count(input,0) ; i++){
        if(i == 0){
            unsigned char *temp = malloc(8 * sizeof(unsigned char));
            sscanf(input, "1.3.", &temp);
            if(temp){
                memmove(input, input + 4, Count(input,0) - 3);
                buffer[i] = 43;
            }
            
        } else {
            sscanf(input,"%d.",&buffer[i]);
            unsigned char step = Count(input, '.') + 1;
            memmove(input,input + step,Count(input,0) - step + 1);
        }
    }
    buffer[++i] = 0;
    for(int j = 0; j < i; j++){
        printf("%02x ",  buffer[j]);
    }
    printf("\n");
    return buffer;
}

void PrintPack(pack_t *p_pack){
    for(int i = 0; i < p_pack->top; i++ ){
        printf("%02x ", p_pack->bytes[i]);
    }
    printf("\n");
}

void PrintOid(unsigned char *p_oid){
    printf("%u\n", Count(p_oid, 0));
}

void WriteToBin(pack_t *p_pack){
    FILE *outfile = NULL;
    outfile = fopen("request.bin", "wb");
    if(outfile == NULL){
        printf("Error opening file\n");
        exit(FILE_IO_ERROR);
    } else {
        fwrite(p_pack->bytes, sizeof(unsigned char), p_pack->top, outfile);
    }
}

int CreateSocket(){
    int sock;
    sock = socket(AF_INET,SOCK_DGRAM,0);
    if(sock<0)
    {
        printf("Socket creation error!\n");
        exit(SOCKET_ERROR);
    } else {
        return sock;
    }
}

void Request(int socket, char *address, pack_t *p_pack ){
    unsigned char *buffer = malloc( 256 * sizeof(unsigned char));
    int n,length;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(address);
    sendto(socket, (const char *)p_pack->bytes, p_pack->top , MSG_CONFIRM, (const struct sockaddr *) &addr, sizeof(addr));
    n = recvfrom(socket, buffer, MAXLINE, MSG_WAITALL,(struct sockaddr *) &addr, &length);
    printf("recieved: %03d bytes\n%s\n", n, buffer);
    ///debug
    FILE *outfile = NULL;
    outfile = fopen("response.bin","wb");
     if(outfile == NULL){
        printf("Error opening file\n");
        exit(FILE_IO_ERROR);
    } else {
        fwrite(buffer, sizeof(unsigned char), n, outfile);
    }
    ///
    close(socket);
    return 
}
