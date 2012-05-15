/*
 * Author: Volkan Yavuz
 * Date  : 5/3/2011
 * 
 * CSCI 360 Operating Systems Fall 2011
 * 
 * Homework III
 */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>

using namespace std;

typedef void* any_t;

int ProcessInCPU(struct ProcessControlBlock*);
void DeAllocateMemory();
int findShortestQueue();
int findLongestQueue();
void GetPCB(int processID, any_t *value, int *cpuNumber);
bool IsMemoryAvailable(int numOfWords);
int GetNextFreePage();

int numCPU = 0;
int counter=0;
int iTotalMemory = 0;
int iPageSize = 0;
int numPrinter;
int numDisk;
int numCD;

char ch;
string cp;
string inter;

int screen=0;
struct Frame
{
	bool bOccupied;
	int pid;
	int LogicalPage;
	Frame():
	bOccupied(false)
	{
	}
};

Frame *pPageTable;

struct fileInfo{
	string filename;
	string option;
	int startingLocation;
	int size;
};

struct ProcessControlBlock
{
	int ProcessID;
	struct fileInfo myInfo[512];
	string state;
	int count;
	int cpuUsage;
	int cpuAssigned;
	int numOfWords;
	int numOfPages;
	int *pProcessPageTable;
	ProcessControlBlock():
	cpuUsage(0),
	pProcessPageTable(NULL),
	numOfWords(0)
	{
	}
};

struct Node{
	struct Node* next;
	any_t data;
};

struct Queue{
	struct Node* head;
	struct Node* tail;
	int count;
};

struct sCPU
{
	Queue ReadyQueue;	
	int timeSlice;
	string cpu;
	int RunningProcess;
	ProcessControlBlock *pCurrPCB;
	sCPU()
	{
		cpu = "Free";
	}
};

Queue* DiskQueue;		//global disk queue
Queue* PrinterQueue;	//global printer queue
Queue* CdQueue;			//global CD queue
Queue JobPool;

sCPU *pCPU;

int globalCount=0;

void initialize_queue(struct Queue* q){
	q->head=q->tail=NULL;
	q->count = 0;
}

//this will put job in descending order of size
int enqueueJob(Queue *q, ProcessControlBlock *pcb)
{
	Node *node = new Node;
	node->data = pcb;

	if(q->head == NULL)
	{
		q->head = q->tail = node;
	}
	else
	{
		Node *pNode = q->head;
		ProcessControlBlock *pPCB = (ProcessControlBlock *)(pNode->data);
		
		if(pcb->numOfWords > pPCB->numOfWords)
		{
			node->next = q->head;
			q->head = node;
		}
		else
		{
			
			while(pNode->next != NULL)
			{
				ProcessControlBlock *pPCB = (ProcessControlBlock *)(pNode->next->data);
				if(pcb->numOfWords > pPCB->numOfWords)
				{
					node->next = pNode->next;
					pNode->next = node;
					break;
				}
			}
			if(pNode->next == NULL)
			{
				q->tail->next = node;
				q->tail = node;
				node->next = NULL;
			}
		}
	}
	q->count++;

	return 0;
}

int enqueue(struct Queue *q,any_t value)
{
	struct Node *node=new struct Node;

	if (node == NULL) {
		cout<<"\nNode does not exist.\n";
		return 1;
	}
	node->data = value;

	if (q->head == NULL) {
		q->head = q->tail = node;
	} else {
	        q->tail->next = node;
	        q->tail = node;
	}
	q->count++;
	node->next = NULL;
	return 0;
}

int dequeue(struct Queue *q, any_t* value)
{
	if(!q->head)
		return 0;

        *value = q->head->data;
        struct Node *tmp = q->head;
        q->head = q->head->next;
		q->count--;
		
		if(q->count == 0)
		{
			q->tail = NULL;
		}

        free(tmp);
	
		return 1;
}

int queue_empty(const struct Queue *q)
{
    return q->head == NULL;
}

int GetNextFreePage()
{
	int totalPages = iTotalMemory / iPageSize;
	for(int i = 0; i < totalPages; ++i)
	{
		if(pPageTable[i].bOccupied == false)
		{
			pPageTable[i].bOccupied = true;
			return i;
		}
	}
	return -1;
}

bool IsMemoryAvailable(int numOfWords)
{
	int numOfPages = (numOfWords / iPageSize) + ((numOfWords % iPageSize)? 1 : 0);

	int availablePage = 0;
	int totalPages = iTotalMemory / iPageSize;
	
	for(int i = 0; i < totalPages; ++i)
	{
		if(pPageTable[i].bOccupied == false)
			availablePage++;
	}

	return availablePage >= numOfPages;
}

void DeAllocatePages(ProcessControlBlock* pcb)
{
	for(int i = 0; i < pcb->numOfPages; ++i)
	{
		pPageTable[pcb->pProcessPageTable[i]].bOccupied = false;
	}
	//check job pool 
	if(!queue_empty(&JobPool))
	{
		any_t process;
		dequeue(&JobPool, &process);
		ProcessControlBlock *pcb = (ProcessControlBlock *)process;
		
		pcb->pProcessPageTable = new int[pcb->numOfPages];

		//page allocated to process
		for(int i = 0; i < pcb->numOfPages; i++)
		{
			int numFreePage = GetNextFreePage();
			pcb->pProcessPageTable[i] = numFreePage;

			pPageTable[numFreePage].LogicalPage = i;
			pPageTable[numFreePage].pid = pcb->ProcessID;
		}

		int cpuNum = findShortestQueue();
		pcb->cpuAssigned = cpuNum;

		enqueue(&pCPU[cpuNum].ReadyQueue, pcb);
		
		pcb->count=0;
	}
}

struct ProcessControlBlock* CreatePCB()
{ 
	ProcessControlBlock *p = new ProcessControlBlock;
	p->state="Ready";
	
	if(counter==1)
		p->state="Running";
	p->ProcessID=++counter;

	return p;
}

int pickProcessFromQueue(int cpuNum){
	any_t process;
	if(!pCPU[cpuNum].cpu.compare("Free"))
	{
		if(!dequeue(&pCPU[cpuNum].ReadyQueue,&process))
		{
			cout<<"\nQueue is Empty now!\n";
			return -5;
		}
		else
		{
			struct ProcessControlBlock* newProcess = (struct ProcessControlBlock*)process;
			//cout<<"\nPROCESS WITH ID# "<<newProcess->ProcessID<<" IS PASSED TO THE CPU\n";
			//cout<<"\n=================================================================\n";
			newProcess->state="Running";
			pCPU[cpuNum].RunningProcess = newProcess->ProcessID;
			pCPU[cpuNum].cpu = "Running";
			pCPU[cpuNum].pCurrPCB = newProcess;
			
			ProcessInCPU(newProcess);
		}
	}
	else {
		//cout<<"\nProcess with ID# "<<pCPU[cpuNum].RunningProcess<<" is using CPU. New Process will be passed when Current Running Process Will be Terminated. This process can terminated by pressing `t`\n";
	}

	return 0;
}

void ProcessArrived(){
	//cout<<"\n=================================================================\n";
	//cout<<"\nPROCESS HAS ARRIVED IN THE SYSTEM\n";
	//cout<<"\ncreating Process Control Block.......\n";
	struct ProcessControlBlock* pcb=CreatePCB();
	
	cout <<"\nEnter total memory required by the process (in words)\n";
	cin >> pcb->numOfWords;

	pcb->numOfPages = (pcb->numOfWords / iPageSize) + ((pcb->numOfWords % iPageSize)? 1 : 0);

	if(pcb->numOfWords > iTotalMemory)
	{
		cout <<"\nRequired memory by Process is larger then available system memory\n";
		delete pcb;
		return;
	}
	else if(!IsMemoryAvailable(pcb->numOfWords))
	{
		enqueueJob(&JobPool, pcb);
	}
	else
	{
		pcb->pProcessPageTable = new int[pcb->numOfPages];

		//page allocated to process
		for(int i = 0; i < pcb->numOfPages; i++)
		{
			int numFreePage = GetNextFreePage();
			pcb->pProcessPageTable[i] = numFreePage;
			pPageTable[numFreePage].LogicalPage = i;
			pPageTable[numFreePage].pid = pcb->ProcessID;
		}

		int cpuNum = findShortestQueue();
		pcb->cpuAssigned = cpuNum;

		enqueue(&pCPU[cpuNum].ReadyQueue, pcb);
		
		pcb->count=0;
		
		pickProcessFromQueue(cpuNum);
	}
}

void SystemCallPrinter(string value){

	cout<<"\nPerforming System Call of Printer\n";
	
	int cpuNumber = value[0] - '0';
	struct ProcessControlBlock *pcb = pCPU[cpuNumber].pCurrPCB;

	cout <<"\nEnter FileName : ";	
	cin >> pcb->myInfo[pcb->count].filename;

	pcb->myInfo[pcb->count].option="write";
	cout << "\nEnter Size of "<<pcb->myInfo[pcb->count].option<<" : ";
	cin >> pcb->myInfo[pcb->count].size;
	
	cout<<"\nEnter Starting Memory location of File : ";
	cin >> pcb->myInfo[pcb->count].startingLocation;
	
	pcb->count++;

	int queueNo=value[2]-48;
	enqueue(&PrinterQueue[queueNo],pcb);

	pCPU[cpuNumber].cpu="Free";
	pickProcessFromQueue(cpuNumber);
	
}

void InterruptSignalForPrinter(string value){
	any_t process;
	int queueNo=value[1]-48;
	
	cout<<"\nNow Interrupt will be sent to the Queue # " << queueNo;
	dequeue(&PrinterQueue[queueNo],&process);
	struct ProcessControlBlock* newProcess = (struct ProcessControlBlock*)process;
	enqueue(&pCPU[newProcess->cpuAssigned].ReadyQueue,newProcess);
}

void InterruptSignalForDisk(string value){
	any_t process;
	
	int queueNo=value[1]-48;
	cout<<"\nNow Interrupt will be sent to the Queue # " << queueNo;
	dequeue(&DiskQueue[queueNo],&process);
	struct ProcessControlBlock* newProcess = (struct ProcessControlBlock*)process;
	enqueue(&pCPU[newProcess->cpuAssigned].ReadyQueue,newProcess);
}


void SystemCallDisk(string value){
	cout<<"\nPerforming System Call of Disk\n";
	int cpuNumber = value[0] - '0';
	struct ProcessControlBlock *pcb = pCPU[cpuNumber].pCurrPCB;

	cout<<"\nEnter FileName : ";
	
	cin >> pcb->myInfo[pcb->count].filename;
	cout << "\nEnter Operation you want to Perform (read/write) : ";
	cin >> pcb->myInfo[pcb->count].option;

	if(!pcb->myInfo[pcb->count].option.compare("write")){
		cout << "\nEnter Size of "<<pcb->myInfo[pcb->count].option<<" : ";
		cin >> pcb->myInfo[pcb->count].size;
	}

	cout<<"\nEnter Starting Memory location of File : ";
	cin >> pcb->myInfo[pcb->count].startingLocation;

	pcb->count++;
	int queueNo=value[2]-48;

	enqueue(&DiskQueue[queueNo],pcb);

	pCPU[cpuNumber].cpu="Free";
	pickProcessFromQueue(cpuNumber);
}

void SystemCallCD(string value){
	cout<<"\nPerforming System Call of CD/RW\n";
	int cpuNumber = value[0] - '0';
	struct ProcessControlBlock *pcb = pCPU[cpuNumber].pCurrPCB;
	cout<<"\nEnter FileName : ";
	
	cin>>pcb->myInfo[pcb->count].filename;
	cout << "\nEnter Operation you want to Perform (read/write) : ";
	cin >> pcb->myInfo[pcb->count].option;

	if(!pcb->myInfo[pcb->count].option.compare("write")){
		cout << "\nEnter Size of "<<pcb->myInfo[pcb->count].option<<" : ";
		cin >> pcb->myInfo[pcb->count].size;
	}

	cout<<"\nEnter Starting Memory location of File : ";
	cin >> pcb->myInfo[pcb->count].startingLocation;

	pcb->count++;
	int queueNo=value[2]-48;

	enqueue(&CdQueue[queueNo],pcb);

	pCPU[cpuNumber].cpu="Free";
	pickProcessFromQueue(cpuNumber);
}

void InterruptSignalForCD(string value){
	any_t process;
	int queueNo=value[1]-48;
	cout<<"\nNow Interrupt will be sent to the Queue # " << queueNo;
	dequeue(&CdQueue[queueNo],&process);
	struct ProcessControlBlock* newProcess = (struct ProcessControlBlock*)process;
	enqueue(&pCPU[newProcess->cpuAssigned].ReadyQueue,newProcess);
}

void SnapshotRoutine(struct ProcessControlBlock* pcb){
	string option;

	//cout<<"\n=================================================================\n";
	cout<<"\n(Options are; `r`, `p`, `d`, `c`, `m`)\n\n";
	//cout<<"\nEnter r for Viewing PIDs of Processes In ReadyQueue\nEnter p to View PIDs and Printer Specific Information in the Printer Queue\nEnter d to View PIDs and Disk Specific Information in the Disk Queue\nEnter c to View PIDs and Disk Specific Information in the CD/RW Queue\n\n";

	cin>>option;
	struct Node* iterator = NULL;
	int counter=0;
	if(!option.compare("r")){

		for(int i = 0; i < numCPU; i++)
		{
			struct Node* iterator = pCPU[i].ReadyQueue.head;
			cout<<"\nList of PIDs in the ReadyQueue of "<< i <<"th CPU\n";
			
			while(iterator!=NULL){

				struct ProcessControlBlock* p=(struct ProcessControlBlock*)iterator->data;
				
				cout<<"pId: " <<p->ProcessID<<" | CPU Usage Time: "<<p->cpuUsage<<endl;
				for(int i = 0; i < p->numOfPages; ++i)
				{
					cout <<"Logical Page " << i <<" Frame "<< p->pProcessPageTable[i] <<"\n";
				}
				/*
				cout<<"\nProcess IDs = ";
				cout<<p->ProcessID << " ";
				cout <<"\nCPU Usage Time = ";
				cout << p->cpuUsage;
				*/
				iterator=iterator->next;
			}
		}
	}
	if(!option.compare("p")){
		counter=0;
		int i=0;
			while(counter < numPrinter){
				struct Node* iterator = PrinterQueue[counter].head;
				cout<<"\nList of PIDs in the PrinterQueue #"<<counter<<" are\n";						
				i=0;
				while(iterator!=NULL){

					struct ProcessControlBlock* p=(struct ProcessControlBlock*)iterator->data;
					i=0;
					while(i<p->count){
						cout<<"pId: "<<p->ProcessID<<" | State: "<<p->state<<" | FileName: "<<p->myInfo[i].filename<<" | Size of "<<p->myInfo[i].option<<": "<<p->myInfo[i].size<<" | MemoryLocation: "<<p->myInfo[i].startingLocation<<" | Operation: "<<p->myInfo[i].option<<endl;
						/*
						cout<<"ProcessID = " << p->ProcessID <<endl;
						screen++;
						cout<<"State = "<<p->state<<endl;
						screen++;
						cout<<"FileName = "<<p->myInfo[i].filename<<endl;
						screen++;
						cout <<"Size of "<<p->myInfo[i].option<<" = "<<p->myInfo[i].size<<endl;
						screen++;	
						cout<<"MemoryLocation = "<<p->myInfo[i].startingLocation<<endl;
						screen++;
						cout<<"Operation = "<<p->myInfo[i].option<<endl<<endl<<endl;
						screen++;
						*/
						for(int j = 0; j < p->numOfPages; ++j)
						{
							cout <<"Logical Page " << j <<"Physical Page "<< p->pProcessPageTable[j] <<"\n";
						}
						screen++;
						i++;
					}
				iterator=iterator->next;

				if(screen>=23)
					break;
				}
			if(screen>=23)
				screen=0;

			counter++;
			}
		
	}
	if(!option.compare("d")){
		counter=0;
		int i=0;
		
		while(counter< numDisk){
			struct Node* iterator = DiskQueue[counter].head;
			cout<<"\nList of PIDs in the DiskQueue #"<<counter<<" are\n";
			//cout<<"\n=================================================================\n";
			while(iterator!=NULL){

				struct ProcessControlBlock* p=(struct ProcessControlBlock*)iterator->data;
				i=0;
				while(i<p->count){
					cout<<"pId: " << p->ProcessID<<" | State: "<<p->state<<" | FileName: "<<p->myInfo[i].filename<<" | MemoryLocation: "<<p->myInfo[i].startingLocation<<" | Operation: "<<p->myInfo[i].option<<endl;
					/*cout<<"ProcessID = " << p->ProcessID <<endl;
					screen++;
					cout<<"State = "<<p->state<<endl;
					screen++;
					cout<<"FileName = "<<p->myInfo[i].filename<<endl;
					screen++;
					cout<<"MemoryLocation = "<<p->myInfo[i].startingLocation<<endl;
					screen++;
					cout<<"Operation = "<<p->myInfo[i].option<<endl<<endl<<endl;
					*/
					for(int j = 0; j < p->numOfPages; ++j)
					{
						cout <<"Logical Page " << j <<"Physical Page "<< p->pProcessPageTable[j] <<"\n";
					}
					screen++;
					i++;
					}
				iterator=iterator->next;
				if(screen>=23)
					break;
				}

			if(screen>=23)
				screen=0;
			counter++;
			}
		}
	if(!option.compare("c")){	
		counter=0;
		int i=0;
		
		while(counter < numCD){
			struct Node* iterator = CdQueue[counter].head;
			cout<<"\nList of PIDs in the CDQueue #"<<counter<<" are\n";
			while(iterator!=NULL){

				struct ProcessControlBlock* p=(struct ProcessControlBlock*)iterator->data;
				i=0;
				while(i<p->count){
					cout<<"pId: " << p->ProcessID<<" | State: "<<p->state<<" | FileName: "<<p->myInfo[i].filename<<" | MemoryLocation: "<<p->myInfo[i].startingLocation<<" | Operation: "<<p->myInfo[i].option<<endl;
					/*
					cout<<"ProcessID = " << p->ProcessID <<endl;
					screen++;
					cout<<"State = "<<p->state<<endl;
					screen++;
					cout<<"FileName = "<<p->myInfo[i].filename<<endl;
					screen++;
					cout<<"MemoryLocation = "<<p->myInfo[i].startingLocation<<endl;
					screen++;
					cout<<"Operation = "<<p->myInfo[i].option<<endl<<endl<<endl;
					*/
					for(int j = 0; j < p->numOfPages; ++j)
					{
						cout <<"Logical Page " << j <<"Physical Page "<< p->pProcessPageTable[j] <<"\n";
					}
					screen++;
					i++;
				}
				iterator=iterator->next;
				if(screen>=23)
					break;
			}

			if(screen>=23)
				screen=0;
			counter++;
		}
	}
	if(!option.compare("m"))
	{
		cout <<"\nSystem memory frame table status\n";
		int numOfPages = iTotalMemory / iPageSize;

		for(int i = 0; i < numOfPages; i++)
		{
			if(pPageTable[i].bOccupied == false)
			{
				cout <<"Page "<< i << " Free\n";
			}
			else
			{
				cout <<"Page "<< i << " Occupied by Logical frame "<< pPageTable[i].LogicalPage <<" of Process pid" << pPageTable[i].pid << "\n";
			}
		}
	}
	
}

void GetPCB(int processID, any_t *value, int *cpuNumber)
{
	*value = NULL;
	*cpuNumber = -1;
	
	for(int i = 0; i < numCPU; i++)
	{
		Node *iterator = pCPU[i].ReadyQueue.head;
		
		while(iterator != NULL)
		{
			if(((ProcessControlBlock *)iterator->data)->ProcessID 
					== processID)
			{
				*value = iterator->data;
				return;
			}

			iterator = iterator->next;
		}
	}

	for(int i = 0; i < numCPU; i++)
	{
		if(pCPU[i].pCurrPCB->ProcessID == processID)
		{
			*value = pCPU[i].pCurrPCB;
			*cpuNumber = i;
			return;
		}
	}
	return;
}

int ProcessInCPU(struct ProcessControlBlock* pcb){
	
	while(1){
		cin>>cp;
		if(cp[1]=='t'){
			int cpuNUM = cp[0] - '0';
			if(pCPU[cpuNUM].cpu.compare("Running"))
			{
				cout << "\nCPU doesn't have any process running\n";
				break;
			}
			cout<<"\nProcess is Terminating on CPU "<< cpuNUM <<".\n";
			cout <<"\npID: " << pCPU[cpuNUM].pCurrPCB->ProcessID << " | CPU Usage: "<< pCPU[cpuNUM].pCurrPCB->cpuUsage<<endl;
			//cout <<"\npID: " << pCPU[cpuNUM].pCurrPCB->ProcessID << "\n";
			//cout <<"\nCPU Usage: " << pCPU[cpuNUM].pCurrPCB->cpuUsage << "\n";
			
			pCPU[cpuNUM].pCurrPCB->state="IDLE";
			pCPU[cpuNUM].pCurrPCB->cpuUsage = 0;
			pCPU[cpuNUM].pCurrPCB->ProcessID = 0;

			DeAllocatePages(pCPU[cpuNUM].pCurrPCB);

			pCPU[cpuNUM].cpu ="Free";

			enqueue(&pCPU[cpuNUM].ReadyQueue, pCPU[cpuNUM].pCurrPCB);

			if(pickProcessFromQueue(cpuNUM)==-5)
				return 0;
		}
		else if(cp[0]== 'K')
		{
			int processID = cp[1] - '0';
			int cpuNumber;
			any_t process;
			GetPCB(processID, &process, &cpuNumber);
			if(process == NULL)
			{
				cout <<"\nThere is no process exist with pID "<< processID<<"\n";
				return -3;
			}			
			
			struct ProcessControlBlock *pcb = (struct ProcessControlBlock *)process;

			cout <<"\nProcess is getting killed by the system\n";
			cout <<"\npID: " << pcb->ProcessID << " | CPU Usage: "<< pcb->cpuUsage<<endl;
			//cout <<"\nProcess ID = " << pcb->ProcessID << ".\n";
			//cout <<"\nCPU Usage = " << pcb->cpuUsage << ".\n";
			
			pcb->state = "IDLE";
			pcb->cpuUsage = 0;
			pcb->ProcessID = 0;
			DeAllocatePages(pcb);

			if(cpuNumber != -1)
			{
				pCPU[cpuNumber].cpu = "Free";
				
				if(pickProcessFromQueue(cpuNumber)==-5)
					return 0;
			}

		}
		else if(!cp.compare("A"))
		{
			ch='A';
			
			ProcessArrived();
		}
		else if(cp[1] == 'T')
		{
			int cpuNumber = cp[0] - '0';
			if(cpuNumber > numCPU || cpuNumber < 0)
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified CPU Does not exist\n");
				return -4;
			}

			ProcessControlBlock *pcb = pCPU[cpuNumber].pCurrPCB;
			pcb->cpuUsage			 += pCPU[cpuNumber].timeSlice;
			cout <<"\nProcess with pID#"<< pcb->ProcessID <<" ends with its time slice\n";
			enqueue(&pCPU[cpuNumber].ReadyQueue, pcb);
			pCPU[cpuNumber].cpu = "Free";
			pickProcessFromQueue(cpuNumber);
		}
		else if(cp[1]=='p'){
			int cpuNumber = cp[0] - '0';
			if(cp[0] > '0' + numCPU || cp[0] < '0' + 0)
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified CPU Does not exist\n");
			}
			else if(cp[2]>(numPrinter+'0') || cp[2] <0+'0')
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified Device Does not exist\n");
			}
			else
			{
				int timeElapsed;
				cout << "\nPlease enter time elapsed for process in CPU.\n";
				cin  >> timeElapsed;
				
				if(timeElapsed > pCPU[cpuNumber].timeSlice)
				{
					//cout<<"\n=================================================================\n";
					printf("\nTime Elapsed is more then current cpu time slice\n");
				}
				else
				{
					pCPU[cpuNumber].pCurrPCB->cpuUsage += timeElapsed;
					SystemCallPrinter(cp);
				}
			}
		}
		else if(cp[0]=='P'){
			if(cp[1]>(numPrinter+'0') || cp[1]<0+'0')
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified Device Does not exist\n");
			}
			else
			{
				InterruptSignalForPrinter(cp);
			}
		}

		else if(cp[0]=='D'){
			int cpuNumber = cp[0] - '0';
			if(cp[1]>(numDisk +'0') || cp[1]<0+'0')
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified Device Does not exist\n");
			}
			else
			{
				InterruptSignalForDisk(cp);
			}
				
		}

		else if(cp[1]=='d'){

			int cpuNumber = cp[0] - '0';
			if(cp[0] > '0' + numCPU || cp[0] < '0' + 0)
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified CPU Does not exist\n");
			}
			else if(cp[2]>(numDisk+'0') || cp[2]<0+'0')
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified Device Does not exist\n");
			}
			else
			{
				int timeElapsed;
				cout << "\nPlease enter time elapsed for process in CPU.\n";
				cin  >> timeElapsed;
				
				if(timeElapsed > pCPU[cpuNumber].timeSlice)
				{
					//cout<<"\n=================================================================\n";
					printf("\nTime Elapsed is more then current cpu time slice\n");
				}
				else
				{
					pCPU[cpuNumber].pCurrPCB->cpuUsage += timeElapsed;
					SystemCallDisk(cp);
				}
			}
		}
		else if(cp[0]=='C'){
			int cpuNumber = cp[0] - '0';
			if(cp[1]>(numCD +'0') || cp[1]<0+'0')
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified Device Does not exist\n");
			}
			else
			{
				InterruptSignalForCD(cp);
			}
				
		}

		else if(cp[1]=='c'){

			int cpuNumber = cp[0] - '0';
			if(cp[0] > '0' + numCPU || cp[0] < '0' + 0)
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified CPU Does not exist\n");
			}
			else if(cp[2]>(numCD+'0') || cp[2]<0+'0')
			{
				//cout<<"\n=================================================================\n";
				printf("\nSpecified Device Does not exist\n");
			}
			else
			{
				int timeElapsed;
				cout << "\nPlease enter time elapsed for process in CPU.\n";
				cin  >> timeElapsed;
				
				if(timeElapsed > pCPU[cpuNumber].timeSlice)
				{
					//cout<<"\n=================================================================\n";
					printf("\nTime Elapsed is more then current cpu time slice\n");
				}
				else
				{
					pCPU[cpuNumber].pCurrPCB->cpuUsage += timeElapsed;
					SystemCallCD(cp);
				}
			}
		}

		else if(!cp.compare("S"))
			SnapshotRoutine(pcb);
		else if(!cp.compare("q"))
			exit(0);
		else
			cout<<"\nYour Entered Choice is Not Valid. Please Enter Again...\n";
		cout<<"\n===+Enter Input:+===\n";
	}

	return 0;
}

int main(void)
{
	cout << "\nEnter how many Intel i7 CPUs you want to have.\n";
	cin  >> numCPU;

	pCPU = new sCPU[numCPU];

	for(int i = 0; i < numCPU; i++)
		initialize_queue(&pCPU[i].ReadyQueue);
	
	bool bFlag = false;

	while(!bFlag)
	{
		cout <<"\nEnter total memory (in words) for system\n";
		cin  >> iTotalMemory;

		cout <<"\nEnter Page Size (in Words)\n";
		cin  >> iPageSize;

		if(iPageSize & (iPageSize - 1))
		{
			cout <<"Page size should be power of 2";
			bFlag = false;
		}
		else if(iTotalMemory % iPageSize)
		{
			cout <<"Page size should completely cover total memory";
			bFlag = false;
		}
		else 
		{
			bFlag = true;
		}
	}
	
	int numOfPages = iTotalMemory/iPageSize;
	pPageTable = new Frame[numOfPages];


	initialize_queue(&JobPool);

	cout << "\nNow you have to enter devices (printer, CD, disk) for CPUs to work with:\n";

	for(int i = 0; i < numCPU; i++)
	{
		cout << "Time slice for CPU " << (i + 1) << "\n";
		cin  >> pCPU[i].timeSlice;
	}
		cout <<"\n=====================================\n";
		cout <<"Enter number of devices for CPUs \n";
		cout <<"=====================================\n";

		
		printf("Printers : ");
		cin >> numPrinter;

		cout << "Disks : ";
		cin >> numDisk;

		cout << "CD/RW : ";
		cin >> numCD;

		PrinterQueue = new Queue[numPrinter];
		DiskQueue = new Queue[numDisk];
		CdQueue = new Queue[numCD];

		for(int j = 0;j < numPrinter; j++)
			initialize_queue(&PrinterQueue[j]);

		for(int j = 0;j < numDisk; j++)
			initialize_queue(&DiskQueue[j]);

		for(int j = 0;j < numCD; j++)
			initialize_queue(&CdQueue[j]);
	
	
	
	while(1){	
		cout<<"\n===+Enter Input:+===\n";
		cin>>ch;
		if(ch=='A')
			ProcessArrived();
		else if(ch=='q')
			exit(0);
	}

	delete [] pPageTable;
	return 0;
}

void DeAllocateMemory()
{
	delete [] PrinterQueue;
	delete [] DiskQueue;
	delete [] CdQueue;
	delete [] pCPU;
}

int findShortestQueue()
{
	int minCount = pCPU[0].ReadyQueue.count;
	int CPUNumber = 0;

	for(int i = 1; i < numCPU; i++)
	{
		if(pCPU[i].ReadyQueue.count < minCount)
		{
			minCount = pCPU[i].ReadyQueue.count;
			CPUNumber = i;
		}
	}

	return CPUNumber;
}

int findLongestQueue()
{
	int maxCount = pCPU[0].ReadyQueue.count;
	int CPUNumber = 0;

	for(int i = 1; i < numCPU; i++)
	{
		if(pCPU[i].ReadyQueue.count > maxCount)
		{
			maxCount = pCPU[i].ReadyQueue.count;
			CPUNumber = i;
		}
	}

	return CPUNumber;
}
