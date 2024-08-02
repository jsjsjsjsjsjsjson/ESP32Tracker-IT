#ifndef IT_FILE_H
#define IT_FILE_H

#include <stdint.h>
#include <stdio.h>

typedef struct __attribute__((packed)) {
    char IMPM[4];      // not include NULL
    char SongName[26]; // includes NULL
    uint16_t PHiligt;
    uint16_t OrdNum;
    uint16_t InsNum;
    uint16_t SmpNum;
    uint16_t PatNum;
    uint16_t CwtV;
    uint16_t Cmwt;
    uint16_t Flags;
    uint16_t Special;
    uint8_t GV;
    uint8_t MV;
    uint8_t IS;
    uint8_t IT;
    uint8_t Sep;
    uint8_t PWD;
    uint16_t MsgLgth;
    uint32_t MsgOfst;
    uint32_t Reserved;
    uint8_t ChnlPan[64];
    uint8_t ChnlVol[64];
    uint8_t *Orders;
} it_header_t;

void read_it_header(const char *filename, it_header_t *header) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Read Error");
        return;
    }
    fread(header, sizeof(it_header_t), 1, file);
    fclose(file);
    printf("IMPM: %.4s\n", header->IMPM);
    printf("SongName: %s\n", header->SongName);
    printf("PHiligt: %u\n", header->PHiligt);
    printf("OrdNum: %u\n", header->OrdNum);
    printf("InsNum: %u\n", header->InsNum);
    printf("SmpNum: %u\n", header->SmpNum);
    printf("PatNum: %u\n", header->PatNum);
    printf("CwtV: %u\n", header->CwtV);
    printf("Cmwt: %u\n", header->Cmwt);
    printf("Flags: %u\n", header->Flags);
    printf("Special: %u\n", header->Special);
    printf("GV: %u\n", header->GV);
    printf("MV: %u\n", header->MV);
    printf("IS: %u\n", header->IS);
    printf("IT: %u\n", header->IT);
    printf("Sep: %u\n", header->Sep);
    printf("PWD: %u\n", header->PWD);
    printf("MsgLgth: %u\n", header->MsgLgth);
    printf("MsgOfst: %u\n", header->MsgOfst);
    printf("Reserved: %u\n", header->Reserved);
    
    printf("ChnlPan: ");
    for (int i = 0; i < 64; i++) {
        printf("%u ", header->ChnlPan[i]);
    }
    printf("\n");

    printf("ChnlVol: ");
    for (int i = 0; i < 64; i++) {
        printf("%u ", header->ChnlVol[i]);
    }
    printf("\n");
}

#endif // IT_FILE_H