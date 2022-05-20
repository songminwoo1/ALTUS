#include "ublist.h"
#include <stdio.h>

#define pushBack(ubList, i) for(int j = ALTUS_MAX_UNSEQ_BLOCK_COUNT-1; j > i; j--){ubList->headSeq[j] = ubList->headSeq[j-1]; ubList->tailSeq[j] = ubList->tailSeq[j-1];}

int altusUBCheck(ALTUS_UB_List* ubList, unsigned int seq, unsigned char length){
	for (int i = 0; i < ubList->count; i++) {
		unsigned int currentHS = ubList->headSeq[i];
		unsigned int currentTS = ubList->tailSeq[i];

		if ( seq < currentHS ) {
			if (seq + length < currentHS) {
				if (ubList->count < ALTUS_MAX_UNSEQ_BLOCK_COUNT) {
					pushBack(ubList, i);
					ubList->headSeq[i] = seq;
					ubList->tailSeq[i] = seq + length;
					ubList->count++;
					return ALTUS_UBENUM_NEW_UB_ADDED;
				}
				else {
					return ALTUS_UBENUM_FULL;
				}
			}
			else if (seq + length == currentHS) {
				ubList->headSeq[i] = seq;
				return ALTUS_UBENUM_UB_EXTENDED;
			}
			else {
				return ALTUS_UBENUM_MALICIOUS_INPUT;
			}
		}
		else if (seq < currentTS) {
			return ALTUS_UBENUM_MALICIOUS_INPUT;
		}
		else if (seq == currentTS) {
			if (i == ubList->count - 1) {
				ubList->tailSeq[i] = seq + length;
				return ALTUS_UBENUM_UB_EXTENDED;
			}
			unsigned int nextHeadSeq = ubList->headSeq[i + 1];
			if (seq + length < nextHeadSeq) {
				ubList->tailSeq[i] = seq + length;
				return ALTUS_UBENUM_UB_EXTENDED;
			}
			else if (seq + length == nextHeadSeq) {
				ubList->tailSeq[i] = ubList->tailSeq[i + 1];
				for (int j = i + 1; j < ALTUS_MAX_UNSEQ_BLOCK_COUNT-1; j++) {
					ubList->headSeq[j] = ubList->headSeq[j + 1];
					ubList->tailSeq[j] = ubList->tailSeq[j + 1];
				}
				ubList->count--;
				return ALTUS_UBENUM_UB_EXTENDED;
			}
			else {
				return ALTUS_UBENUM_MALICIOUS_INPUT;
			}

		}
		else if(currentTS < seq && i == ubList->count - 1) { //마지막 블록을 지난 경우.
			if (ubList->count < ALTUS_MAX_UNSEQ_BLOCK_COUNT) {
				ubList->headSeq[ubList->count] = seq;
				ubList->tailSeq[ubList->count] = seq + length;
				ubList->count++;
				return ALTUS_UBENUM_NEW_UB_ADDED;
			}
			else {
				return ALTUS_UBENUM_FULL;
			}
		}
	}
	if (ubList->count == 0) {
		ubList->headSeq[0] = seq;
		ubList->tailSeq[0] = seq + length;
		ubList->count = 1;
		return ALTUS_UBENUM_NEW_UB_ADDED;
	}
	return ALTUS_UBENUM_MALICIOUS_INPUT;
}

#ifdef _DEBUG
void ALTUS_UBTest(ALTUS_UB_List* ubList) {
	printf("=============== ALTUS_UB ===============\n");
	for (int i = 0; i < ubList->count; i++) {
		printf("head: %d, tail: %d\n", ubList->headSeq[i], ubList->tailSeq[i]);
	}
	printf("\n");
}
#endif // _DEBUG