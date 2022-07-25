#pragma once
#define ALTUS_NODE_SIZE 4096
#include <mutex>

enum class UBM {
	FULL = 0,
	MALICIOUS, //weirdly sequenced input.
	OVERLAP, //overlapping with already received area.
	EXTENDED,  //unsequenced block extended(good)
	ADDED,	   //new unsequenced block.
	ONSEQ,		//on-sequence block.
	MERGING		//on-sequence, this will merge with unseq block.
};

#define ALTUS_UBENUM_FULL 0
#define ALTUS_UBENUM_MALICIOUS_INPUT 1
#define ALTUS_UBENUM_OVERLAP 2
#define ALTUS_UBENUM_UB_EXTENDED 3
#define ALTUS_UBENUM_NEW_UB_ADDED 4

class Node {
public:
	Node();
	//~Node();

	//Node* to_head;
	Node* to_tail;
	char data[ALTUS_NODE_SIZE];
	//unsigned short validByteCount;
};

class NodePool {
public:
	NodePool();
	~NodePool();
	//**caution** using methods while deconstructing is undefined behavior.

	Node* pop(); //this may return null. thread safe.
	void push(Node* node);
	int resize(int n); //this can only decrease size.
	//**caution** resize(0) undefined.

	int size;
	std::mutex m;
private:
	Node* recycled_head;
	Node* recycled_tail;
};