#include <stdlib.h>
#include "header.h"
////////////////////////////////////////
int main( int argc, char* argv[]){
    char* community = argv[1];
    char* oid       = argv[2];
    char* address   = argv[3];
    pack_t *request = InitPack(), *response = InitPack();
    request =  PackSNMPGetRequest(community,oid);
    response = Request(CreateSocket(), address, request);
    free(request);
    PrintPack(response);
    UnPackSequence(response);
    free(response);
    return 0;
}
