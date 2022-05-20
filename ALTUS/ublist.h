#pragma once

#define ALTUS_MAX_UNSEQ_BLOCK_COUNT 8

#define ALTUS_UBENUM_FULL 0
#define ALTUS_UBENUM_MALICIOUS_INPUT 1
#define ALTUS_UBENUM_OVERLAP 2
#define ALTUS_UBENUM_UB_EXTENDED 3
#define ALTUS_UBENUM_NEW_UB_ADDED 4

typedef struct ALTUS_UB_List_STRUCT { //unsequenced block linked list
	unsigned int headSeq[ALTUS_MAX_UNSEQ_BLOCK_COUNT];
	unsigned int tailSeq[ALTUS_MAX_UNSEQ_BLOCK_COUNT];
	unsigned char count; //0~8
}ALTUS_UB_List;

int altusUBCheck(ALTUS_UB_List* ubList, unsigned int seq, unsigned char length);

int altusUBPop(ALTUS_UB_List* ubList); //unsafe function.(only for internal actions)

#ifdef _DEBUG
void ALTUS_UBTest(ALTUS_UB_List* ubList);
#endif // _DEBUG