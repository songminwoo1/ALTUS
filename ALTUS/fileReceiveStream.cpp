#include "fileReceiveStream.h"

//#define getWinLen(window, lenPtr) 
//#define recalculateWinLen(window) {int ret = 1;if (window->head == window->safe_tail) {	window->safe_len = 1;}for (WQ_Node* currentNode = window->head; currentNode != window->unsafe_tail; currentNode = currentNode->to_tail) {	if (currentNode == window->safe_tail) {		window->safe_len = ret;	}	ret++;}window->unsafe_len = ret;	}
#define ceil(a, b) a/b + 1
#define NewNode() {if ((tail->to_tail = pool->pop()) == nullptr) tail->to_tail = new Node(); _nodeCount++;}

FileRcvStream::FileRcvStream(NodePool* _pool) {
	printf("file stream created\n");
	pool = _pool; //deprecated
	if((head = pool->pop()) == nullptr) head = new Node;
	tail = head;
	_nodeCount = 0;
	max_nodeCount = ALTUS_DEFAULT_MAX_NODE_COUNT;
	headSeq = 0;
	headByteCount = 0;
	ub_list.reserve(8);
	lastSeq = 0;
	unSeqLen = 0;

	DWORD dwRetVal = GetTempPath(MAX_PATH, lpTempPathBuffer); // Gets the temp path env string
	if (dwRetVal > MAX_PATH || (dwRetVal == 0))
	{
		throw std::runtime_error("temp file creation failed");
	}

	UINT uRetVal = GetTempFileName(lpTempPathBuffer, // directory for tmp files
		TEXT("ALTUS"),     // temp file name prefix 
		0,                // create unique name 
		szTempFileName);  // buffer for name 
	if (uRetVal == 0)
	{
		throw std::runtime_error("temp file creation failed");
	}

	file = CreateFile(
		(LPTSTR)szTempFileName, // file name 
		GENERIC_WRITE,        // open for write 
		FILE_SHARE_READ | FILE_SHARE_WRITE,	  // readable
		NULL,                 // default security 
		CREATE_ALWAYS,        // overwrite existing
		FILE_ATTRIBUTE_NORMAL,// normal file 
		NULL);                // no template 
	if (file == INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
		throw std::runtime_error("temp file creation failed");
	}
}

FileRcvStream::~FileRcvStream() {
	Node* currentNode = head;
	while (currentNode != nullptr) {
		Node* oldNode = currentNode;
		currentNode = currentNode->to_tail;
		pool->push(oldNode);
	}
	CloseHandle(file);
}

void FileRcvStream::Finalize() {
	if (head->to_tail != nullptr) throw std::runtime_error("cannot finalize stream: incomplete data");
	DWORD wrtLen;
	try {
		WriteFile(
			file,
			head->data,
			headByteCount,
			&wrtLen,
			NULL
		);
	}
	catch (...) {
		throw std::runtime_error("file writing failed(1)");
	}
	if (wrtLen != headByteCount) {
		throw std::runtime_error("file writing failed(2)");
	}
}

TCHAR* FileRcvStream::getPath() {
	return lpTempPathBuffer;
}
TCHAR* FileRcvStream::getName() {
	return szTempFileName;
}

int FileRcvStream::nodeCount() {
	return _nodeCount;
}

int FileRcvStream::Append(unsigned int seq, unsigned int length, char* data) {
	//return 1 for unsequenced append.
	//return 0 for sequenced append.
	//return -1 for duplicate packet.
	//return -2 for window full(rejected).

	if ((int)(lastSeq - seq) > 0) {
		return -1; //or, throw exception???
	}

	UBM message = CheckUB(seq, length);
	//TODO: do next line inside CheckUB().
	if (ub_list.size() != 0) unSeqLen = ub_list.front().first - lastSeq;
	else unSeqLen = 0;
	switch (message)
	{
	case UBM::FULL:
		return -2;
	case UBM::MALICIOUS:
		throw(std::runtime_error("malicious packet"));
	case UBM::OVERLAP:
		return -1;
	case UBM::EXTENDED:
	case UBM::ADDED:
		PutUnSeq(seq, length, data);
		return 1;
	case UBM::ONSEQ:
	{
		unsigned int writableLen = ALTUS_NODE_SIZE - headByteCount;
		if (writableLen >= length) {
			memcpy(head->data + headByteCount, data, length);
			lastSeq += length;
			headByteCount += length;
			return 0;
		}

		memcpy(head->data + headByteCount, data, writableLen);
		if (head == tail) {
			NewNode();
		}
		memcpy(head->to_tail->data, data + writableLen, length - writableLen);
		headByteCount = headByteCount + length - ALTUS_NODE_SIZE;
		FlushNode();
		return 0;
	}
	case UBM::MERGING:
	{
		int mergeLength = length + ub_list.begin()->second - ub_list.begin()->first;

		unsigned int writableLen = ALTUS_NODE_SIZE - headByteCount;
		if (writableLen >= length) {
			memcpy(head->data + headByteCount, data, length);
		}
		else {
			memcpy(head->data + headByteCount, data, writableLen);
			memcpy(head->to_tail->data, data + writableLen, length - writableLen);
		}

		headByteCount = (headByteCount + mergeLength) % ALTUS_NODE_SIZE;
		lastSeq += mergeLength;
		for (; mergeLength < ALTUS_NODE_SIZE; mergeLength -= ALTUS_NODE_SIZE) {
			FlushNode();
		}

		return 0;
	}
	default:
		throw(std::runtime_error("stack corruption"));
	}
}

void FileRcvStream::FlushNode() {
	DWORD wrtLen;
	try {
		WriteFile(
			file,
			head->data,
			ALTUS_NODE_SIZE,
			&wrtLen,
			NULL
		);
	}
	catch (...) {
		throw std::runtime_error("file writing failed(1)");
	}	
	if (wrtLen != ALTUS_NODE_SIZE) {
		throw std::runtime_error("file writing failed(2)");
	}

	if (head == tail) {
		headSeq += ALTUS_NODE_SIZE;
		headByteCount = 0;
		return;
	}

	Node* oldHead = head;
	head = head->to_tail;
	_nodeCount--;
	pool->push(oldHead);
	headSeq += ALTUS_NODE_SIZE;
	return;
}

UBM FileRcvStream::CheckUB(unsigned int seq, unsigned int length) {
	//Notice: this cause performance fallback when there are too many unsequenced block.
	//may need monitoring.
	if (seq == lastSeq) {
		if (ub_list.size() == 0) return UBM::ONSEQ;

		if ((int)(ub_list.front().first - (seq + length)) > 0) {
			return UBM::ONSEQ;
		}

		if (ub_list.front().first == seq + length) {
			return UBM::MERGING;
		}
		return UBM::MALICIOUS;
	}

	for (std::vector<std::pair<unsigned int, unsigned int>>::iterator i = ub_list.begin(); i != ub_list.end(); i++) {
		unsigned int currentHS = i->first;
		unsigned int currentTS = i->second;

		if ((int)(currentHS - seq) > 0) {
			if ((int)(currentHS - (seq + length)) > 0) {
				if (ub_list.capacity() > ub_list.size()) {
					ub_list.insert(i, std::pair<unsigned int, unsigned int>(seq, seq + length));
					return UBM::ADDED;
				}
				else {
					return UBM::FULL;
				}
			}
			else if (seq + length == currentHS) {
				i->first = seq;
				return UBM::EXTENDED;
			}
			else {
				return UBM::MALICIOUS;
			}
		}

		if ((int)(currentTS - seq) > 0) {
			if ((int)(currentTS - (seq + length)) >= 0) return UBM::OVERLAP;
			else return UBM::MALICIOUS;
		}

		if (seq == currentTS) {
			auto j = i + 1;
			if (j == ub_list.end() || (int)(j->first - (seq + length) > 0)) {
				if (seq + length - headSeq >= ALTUS_DEFAULT_MAX_NODE_COUNT * ALTUS_NODE_SIZE) {
					return UBM::FULL;
				}

				i->second = seq + length;
				return UBM::EXTENDED;
			}

			if (j->first == seq + length) {
				i->second = j->second;
				ub_list.erase(j);
				return UBM::EXTENDED;
			}

			return UBM::MALICIOUS;
		}
	}

	//now, the new packet is unsequenced and passed last unsequenced block.
	if (ub_list.capacity() > ub_list.size()) {
		if (seq + length - headSeq >= ALTUS_DEFAULT_MAX_NODE_COUNT * ALTUS_NODE_SIZE) {
			return UBM::FULL;
		}

		ub_list.push_back(std::pair<unsigned int, unsigned int>(seq, seq + length));
		return UBM::ADDED;
	}
	else {
		return UBM::FULL;
	}
}

void FileRcvStream::PutUnSeq(unsigned int seq, unsigned int length, char* data) {
	int i = seq - headSeq;
	Node* targetNode = head;
	for (; i >= ALTUS_NODE_SIZE; i -= ALTUS_NODE_SIZE) {
		if (targetNode->to_tail == nullptr) {
			NewNode();
		}
		targetNode = targetNode->to_tail;
	}

	//now, i is in between 0 ~ (nodesize-1), and targetnode is where we will write.
	unsigned int writableLen = ALTUS_NODE_SIZE - i;
	if (writableLen >= length) {
		memcpy(targetNode->data + i, data, length);
		return;
	}

	memcpy(targetNode->data + i, data, writableLen);
	if (targetNode->to_tail == nullptr){
		NewNode();
	}
	memcpy(targetNode->to_tail->data, data + writableLen, length - writableLen);
}