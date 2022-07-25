#pragma once
#include "NodeBase.h"
#include <Windows.h>
#include <tchar.h>
#include <vector>

//#define ALTUS_WQ_BLOCK_SIZE 1 //block size of AES, 128-bit. (smallest unit of data in a node)
//#define ALTUS_WQ_NODE_BYTE_COUNT 4096 //number of blocks in one node.(256)
#define ALTUS_NODE_SIZE 4096 //DATA_BLOCK_SIZE * WINDOW_NODE_BLOCK_COUNT(4096)
//#define ALTUS_WINDOW_BLOCK_MAX_DEFAULT 0x20000U //2MB of window.
//#define ALTUS_WINDOW_BLOCK_MAX_ABSOLUTE 0x40000000U //2MB of window.
#define ALTUS_DEFAULT_MAX_NODE_COUNT 16384 //~67MB of window.

class FileRcvStream
{
public:
	//notice: these methods are not thread safe.
	explicit FileRcvStream(NodePool* _pool);
	~FileRcvStream();

	void Finalize(); //this flushes all remaining buffered data to file.

	TCHAR* getPath();
	TCHAR* getName();
	int nodeCount(); //total node count, both safe and unsafe.

	int Append(unsigned int seq, unsigned int length, char* data);

	unsigned int lastSeq;
	unsigned int unSeqLen;
private:
	void FlushNode(); //*caution* this function flushes one full node.

	UBM CheckUB(unsigned int seq, unsigned int length);
	void PutUnSeq(unsigned int seq, unsigned int length, char* data);

	TCHAR lpTempPathBuffer[MAX_PATH];
	TCHAR szTempFileName[MAX_PATH];
	HANDLE file;

	//all node pointer must *not* be NULL. at least one node is in the window.
	NodePool* pool; //deprecated
	Node* head;
	Node* tail; //when there is no unsafe node, this is same with safe_tail.

	int _nodeCount;
	int max_nodeCount;

	unsigned int headSeq; //Seq number of first byte of current node.
	unsigned short headByteCount; //valid byte length of head node.
	//this parameters are for unsequenced data handling.
	std::vector<std::pair<unsigned int, unsigned int>> ub_list; //initial seq, next received seq.
};