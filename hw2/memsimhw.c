//
// Virual Memory Simulator Homework
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system
// Submission Year:
// Student Name:2EZ
// Student Number:
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define PAGESIZEBITS 12                 // page size = 4Kbytes
#define VIRTUALADDRBITS 32              // virtual address space size = 4Gbytes

struct pageTableEntry {
        int level;                              // page table level (1 or 2)
        char valid;
        struct pageTableEntry *secondLevelPageTable;    // valid if this entry is for the first level page table (level = 1)
        int frameNumber;                                                                // valid if this entry is for the second level page table (level = 2)
};

struct framePage {
        int number;                     // frame number
        int pid;                        // Process id that owns the frame
        int virtualPageNumber;                  // virtual page number using the frame
        struct framePage *lruLeft;      // for LRU circular doubly linked list
        struct framePage *lruRight; // for LRU circular doubly linked list
};

struct invertedPageTableEntry {
        int pid;                                        // process id
        int virtualPageNumber;          // virtual page number
        int frameNumber;                        // frame number allocated
        struct invertedPageTableEntry *next;
};

struct procEntry {
        char *traceName;                        // the memory trace name
        int pid;                                        // process (trace) id
        int ntraces;                            // the number of memory traces
        int num2ndLevelPageTable;       // The 2nd level page created(allocated);
        int numIHTConflictAccess;       // The number of Inverted Hash Table Conflict Accesses
        int numIHTNULLAccess;           // The number of Empty Inverted Hash Table Accesses
        int numIHTNonNULLAccess;                // The number of Non Empty Inverted Hash Table Accesses
        int numPageFault;                       // The number of page faults
        int numPageHit;                         // The number of page hits
        struct pageTableEntry *firstLevelPageTable;
        FILE *tracefp;
};

struct framePage *oldestFrame; // the oldest frame pointer
int firstLevelBits, secondLevelBits, phyMemSizeBits, numProcess, nFrame;
int s_flag = 0;


void InitPhyMem(struct framePage*, int);
void AddFrame(struct framePage*, struct framePage*);
void RefreshFrame(struct framePage*, struct framePage*);
void OneLevelVMSim(struct procEntry*, struct framePage*, char);
void TwoLevelVMSim(struct procEntry*, struct framePage*);
void invertedPageVMSim(struct procEntry*, struct framePage*);
void InitIPage(struct invertedPageTableEntry*);
void InitProcTable(struct procEntry *, char **, int , int);
void InitPageTableEntry(struct pageTableEntry*, int, int);



int main(int argc, char *argv[])
{
        int i;
        int type = atoi(argv[1]);
        if(!strcmp(argv[1], "-s"))
        {
                printf("Usage : %s [-s] firstLevelBits PhysicalMemorySizeBits TraceFileNames\n",argv[0]);
                s_flag = 1;
        }
        firstLevelBits = atoi(argv[2+s_flag]);
        phyMemSizeBits = atoi(argv[3+s_flag]);
        numProcess = argc - 4 - s_flag;
        secondLevelBits = VIRTUALADDRBITS - firstLevelBits - PAGESIZEBITS;
        if(phyMemSizeBits < PAGESIZEBITS)
        {
                printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits %d\n",phyMemSizeBits,PAGESIZEBITS);
                exit(1);
        }
        if(VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits <= 0 )
        {
                printf("firstLevelBits %d is too Big for the 2nd level page system\n",firstLevelBits);
                exit(1);
        }
        // initialize procTable for memory simulations
        struct procEntry *procTable = (struct procEntry*)malloc(sizeof(struct procEntry)*numProcess);
        for(i=0; i<numProcess; i++)
        {
                // opening a tracefile for the process
                printf("process %d opening %s\n",i,argv[i+s_flag+4]);
        }


        nFrame = (1 << (phyMemSizeBits-PAGESIZEBITS));
        assert(nFrame > 0);
        struct framePage *phyMem = (struct framePage*)malloc(sizeof(struct framePage)*nFrame);

        printf("\nNum of Frames %d Physical Memory Size %ld bytes\n",nFrame, (1L<<phyMemSizeBits));
        int simType = atoi(argv[1+s_flag]);
        if(simType == 0 || simType >= 3)
        {
                // initialize procTable for the simulation
                InitProcTable(procTable, argv, numProcess, 0);
                // initialize Memory for the simulation
                InitPhyMem(phyMem, nFrame);
                printf("=============================================================\n");
                printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
                printf("=============================================================\n");
                OneLevelVMSim(procTable, phyMem, 'F');

                // initialize procTable for the simulation
                InitProcTable(procTable, argv, numProcess, 0);
                // initialize Memrory for the simulation
                InitPhyMem(phyMem, nFrame);
                printf("=============================================================\n");
                printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
                printf("=============================================================\n");
                OneLevelVMSim(procTable, phyMem, 'L');
        }
        if(simType == 1 || simType >= 3)
        {
                // initialize procTable for the simulation
                InitProcTable(procTable, argv, numProcess, firstLevelBits);
                // initialize Memory for the simulation
                InitPhyMem(phyMem, nFrame);
                printf("=============================================================\n");
                printf("The Two-Level Page Table Memory Simulation Starts .....\n");
                printf("=============================================================\n");
                TwoLevelVMSim(procTable, phyMem);
        }
        if(simType >= 2)
        {
                // initialize procTable for the simulation
                InitProcTable(procTable, argv, numProcess, firstLevelBits);
                InitPhyMem(phyMem, nFrame);
                printf("=============================================================\n");
                printf("The Inverted Page Table Memory Simulation Starts .....\n");
                printf("=============================================================\n");
                invertedPageVMSim(procTable, phyMem);
        }
        return(0);
}
void InitPhyMem(struct framePage *phyMem, int nFrame)
{
        int i;
        for(i = 0; i < nFrame; i++)
        {
                phyMem[i].number = i;
                phyMem[i].pid = -1;
                phyMem[i].virtualPageNumber = -1;
                phyMem[i].lruLeft = &phyMem[(i-1+nFrame) % nFrame];
                phyMem[i].lruRight = &phyMem[(i+1+nFrame) % nFrame];
        }
        oldestFrame = &phyMem[0];
}
void AddFrame(struct framePage *a, struct framePage *b)
{
        b->lruLeft = a->lruLeft;
        a->lruLeft->lruRight = b;
        a->lruLeft = b;
        b->lruRight = a;
}
void RefreshFrame(struct framePage *a, struct framePage *b)
{
        b->lruLeft->lruRight = b->lruRight;
        b->lruRight->lruLeft = b->lruLeft;
        b->lruLeft = a->lruLeft;
        b->lruRight = a;
        a->lruLeft->lruRight = b;
        a->lruLeft = b;
}
void OneLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, char FIFOorLRU)
{
        unsigned Vaddr, Paddr;
        char rw;
        int i, j;
        int numeof = 0;
        int virPageNumber = 0;
        while(1)
        {
                numeof = 0;
                for(i = 0; i < numProcess; i++)
                {
                        if(EOF == fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &rw))
                        {
                                numeof++;
                                continue;
                        }
                        procTable[i].ntraces++;
                        Paddr = Vaddr&0xfff;
                        virPageNumber = Vaddr>>PAGESIZEBITS;
                        if(FIFOorLRU == 'F')
                        {
                                for(j = 0; j < nFrame; j++)
                                {
                                        if(phyMemFrames[j].pid == i && phyMemFrames[j].virtualPageNumber == virPageNumber)
                                        {
                                                procTable[i].numPageHit++;
                                                break;
                                        }
                                        else if(phyMemFrames[j].pid == -1)
                                        {
                                                procTable[i].numPageFault++;
                                                phyMemFrames[j].pid = i;
                                                phyMemFrames[j].virtualPageNumber = virPageNumber;
                                                break;
                                        }
                                }
                                if(j == nFrame)
                                {
                                        procTable[i].numPageFault++;
                                        oldestFrame->pid = i;
                                        oldestFrame->virtualPageNumber = virPageNumber;
                                        oldestFrame = oldestFrame->lruRight;
                                }
                        }
                        else if(FIFOorLRU == 'L')
                        {
                                for(j = 0; j < nFrame; j++)
                                {
                                        if(phyMemFrames[j].pid == i && phyMemFrames[j].virtualPageNumber == virPageNumber)
                                        {
                                                procTable[i].numPageHit++;
                                                RefreshFrame(oldestFrame, &phyMemFrames[j]);
                                                break;
                                        }
                                        else if(phyMemFrames[j].pid == -1)
                                        {
                                                procTable[i].numPageFault++;
                                                phyMemFrames[j].pid = i;
                                                phyMemFrames[j].virtualPageNumber = virPageNumber;
                                                AddFrame(oldestFrame, &phyMemFrames[j]);
                                                break;
                                        }
                                }
                                if(j == nFrame)
                                {
                                        procTable[i].numPageFault++;
                                        oldestFrame->pid = i;
                                        oldestFrame->virtualPageNumber = virPageNumber;
                                        oldestFrame = oldestFrame->lruRight;
                                }
                        }
                        // -s option print statement
                        if(s_flag)
                                printf("One-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces,Vaddr,Paddr );
                }
                if(numeof == numProcess)
                        break;
        }
        for(i=0; i<numProcess; i++)
        {
                printf("**** %s *****\n",procTable[i].traceName);
                printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
                printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
                printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
                assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
        }
}
void TwoLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames)
{
        int i, j, numeof = 0;
        unsigned Vaddr, Paddr, Faddr, Saddr;
        unsigned pageNumber, rFaddr, rSaddr;
        char rw;
        while(1)
        {
                for(i=0; i<numProcess; i++)
                {
                        if(EOF == fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &rw))
                        {
                                numeof++;
                                continue;
                        }
                        procTable[i].ntraces++;
                        Paddr = Vaddr&0xfff;
                        Faddr = Vaddr>>(VIRTUALADDRBITS - firstLevelBits);
                        Saddr = (Vaddr>>(PAGESIZEBITS)) - (Faddr<<(secondLevelBits));
                        if(procTable[i].firstLevelPageTable[Faddr].valid == 'N')
                        {
                                procTable[i].numPageFault++;
                                procTable[i].num2ndLevelPageTable++;
                                procTable[i].firstLevelPageTable[Faddr].valid = 'Y';
                                procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable = malloc(sizeof(struct pageTableEntry)*(1<<secondLevelBits));
                                InitPageTableEntry(procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable, 2, 1<<secondLevelBits);
                                procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable[Saddr].valid = 'Y';
                                for(j=0; j<nFrame; j++)
                                {
                                        if(phyMemFrames[j].pid == -1)
                                        {
                                                procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable[Saddr].frameNumber = phyMemFrames[j].number;
                                                phyMemFrames[j].pid = i;
                                                phyMemFrames[j].virtualPageNumber = Vaddr>>PAGESIZEBITS;
                                                AddFrame(oldestFrame, &phyMemFrames[j]);
                                                break;
                                        }
                                }
                                if(j == nFrame)
                                {
                                        pageNumber = oldestFrame->virtualPageNumber;
                                        rFaddr = pageNumber>>secondLevelBits;
                                        rSaddr = pageNumber - (Faddr<<secondLevelBits);
                                        procTable[i].firstLevelPageTable[rFaddr].secondLevelPageTable[rSaddr].valid = 'N';
                                        procTable[i].firstLevelPageTable[rFaddr].secondLevelPageTable[rSaddr].frameNumber = -1;
                                        procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable[Saddr].frameNumber = oldestFrame->number;
                                        oldestFrame->pid = i;
                                        oldestFrame->virtualPageNumber = Vaddr>>PAGESIZEBITS;
                                        oldestFrame = oldestFrame->lruRight;
                                }
                        }
                        else if(procTable[i].firstLevelPageTable[Faddr].valid == 'Y')
                        {
                                if(procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable[Saddr].valid == 'N')
                                {
                                        procTable[i].numPageFault++;
                                        procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable[Saddr].valid = 'Y';
                                        for(j=0; j<nFrame; j++)
                                        {
                                                if(phyMemFrames[j].pid == -1)
                                                {
                                                        procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable[Saddr].frameNumber = phyMemFrames[j].number;
                                                        phyMemFrames[j].pid = i;
                                                        phyMemFrames[j].virtualPageNumber = Vaddr>>PAGESIZEBITS;
                                                        AddFrame(oldestFrame, &phyMemFrames[j]);
                                                        break;
                                                }
                                        }
                                        if(j == nFrame)
                                        {
                                                pageNumber = oldestFrame->virtualPageNumber;
                                                rFaddr = pageNumber>>(VIRTUALADDRBITS - firstLevelBits - PAGESIZEBITS);
                                                rSaddr = pageNumber - (Faddr<<(VIRTUALADDRBITS - firstLevelBits - PAGESIZEBITS));
                                                procTable[i].firstLevelPageTable[rFaddr].secondLevelPageTable[rSaddr].valid = 'N';
                                                procTable[i].firstLevelPageTable[rFaddr].secondLevelPageTable[rSaddr].frameNumber = -1;
                                                procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable[Saddr].frameNumber = oldestFrame->number;
                                                oldestFrame->pid = i;
                                                oldestFrame->virtualPageNumber = Vaddr>>PAGESIZEBITS;
                                                oldestFrame = oldestFrame->lruRight;
                                        }
                                }
                                else if(procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable[Saddr].valid == 'Y')
                                {
                                        procTable[i].numPageHit++;
                                        RefreshFrame(oldestFrame, &phyMemFrames[procTable[i].firstLevelPageTable[Faddr].secondLevelPageTable[Saddr].frameNumber]);
                                }
                        }


                        // -s option print statement
                        if(s_flag)
                                printf("Two-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces,Vaddr,Paddr);
                }
                if(numeof == numProcess)
                        break;
        }
        for(i=0; i<numProcess; i++)
        {
                printf("**** %s *****\n",procTable[i].traceName);
                printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
                printf("Proc %d Num of second level page tables allocated %d\n",i,procTable[i].num2ndLevelPageTable);
                printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
                printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
                assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
        }
}
void invertedPageVMSim(struct procEntry *procTable, struct framePage *phyMemFrames)
{
        int i, j, numeof = 0;
        unsigned Vaddr, Paddr, Haddr;
        unsigned pageNumber, rFaddr, rSaddr;
        char rw;
        struct invertedPageTableEntry *cur;
        struct invertedPageTableEntry *iPageTable = malloc(sizeof(struct invertedPageTableEntry)*nFrame);
        InitIPage(iPageTable);
        while(1)
        {
                for(i=0; i<numProcess; i++)
                {
                        if(EOF == fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &rw))
                        {
                                numeof++;
                                continue;
                        }
                        procTable[i].ntraces++;
                        Paddr = Vaddr&0xfff;
                        pageNumber = Vaddr>>PAGESIZEBITS;
                        Haddr = (pageNumber + procTable[i].pid)%nFrame;
                        cur = &iPageTable[Haddr];
                        if(cur->frameNumber == 0)
                        {
                                procTable[i].numPageFault++;
                                procTable[i].numIHTNULLAccess++;
                                cur->frameNumber++;
                                cur->next = malloc(sizeof(struct invertedPageTableEntry));
                                cur = cur->next;
                                cur->pid = i;
                                cur->virtualPageNumber = pageNumber;
                                cur->next = NULL;
                                for(j=0; j<nFrame; j++)
                                {
                                        if(phyMemFrames[j].pid == -1)
                                        {
                                                phyMemFrames[j].pid = i;
                                                phyMemFrames[j].virtualPageNumber = pageNumber;
                                                AddFrame(oldestFrame, &phyMemFrames[j]);
                                                cur->frameNumber = phyMemFrames[j].number;
                                                break;
                                        }
                                }
                                if(j == nFrame)
                                {
                                        cur->frameNumber = oldestFrame->number;
                                        oldestFrame->pid = i;
                                        oldestFrame->virtualPageNumber = pageNumber;
                                        oldestFrame = oldestFrame->lruRight;
                                }

                        }
                        else
                        {
                                procTable[i].numIHTNonNULLAccess++;
                                int iter = iPageTable[Haddr].frameNumber;
                                for(j=0; j<iter; j++)
                                {
                                        cur = cur->next;
                                        procTable[i].numIHTConflictAccess++;
                                        if(cur->pid == i && cur->virtualPageNumber == pageNumber)
                                        {
                                                procTable[i].numPageHit++;
                                                RefreshFrame(oldestFrame, &phyMemFrames[cur->frameNumber]);
                                                break;
                                        }
                                }
                                if(j == iter)
                                {
                                        iPageTable[Haddr].frameNumber++;
                                        procTable[i].numPageFault++;
                                        cur = malloc(sizeof(struct invertedPageTableEntry));
                                        cur->pid = i;
                                        cur->virtualPageNumber = pageNumber;
                                        cur->next = iPageTable[Haddr].next;
                                        iPageTable[Haddr].next = cur;
                                }
                        }



                        if(s_flag)
                                printf("IHT procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces,Vaddr,Paddr);

                }
                if(numeof == numProcess)
                        break;
        }
        for(i=0; i < numProcess; i++)
        {
                printf("**** %s *****\n",procTable[i].traceName);
                printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
                printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
                printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
                printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAccess);
                printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
                printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
                assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
                assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAccess == procTable[i].ntraces);
        }
}
void InitIPage(struct invertedPageTableEntry *iPageTable)
{
        int i;
        for(i=0; i<nFrame; i++)
        {
                iPageTable[i].pid = -1;
                iPageTable[i].virtualPageNumber = 0;
                iPageTable[i].frameNumber = 0;
                iPageTable[i].next = NULL;
        }
}
void InitProcTable(struct procEntry * procTable, char *argv[], int n, int firstLevelPageTable)
{
        int i, j;
        for(i=0; i<n; i++)
        {
                procTable[i].traceName = malloc(sizeof(char)*strlen(argv[i+s_flag+4]));
                strcpy(procTable[i].traceName, argv[i+s_flag+4]);                       // the memory trace name
                procTable[i].pid = i;                                   // process (trace) id
                procTable[i].ntraces = 0;                               // the number of memory traces
                procTable[i].num2ndLevelPageTable = 0;  // The 2nd level page created(allocated);
                procTable[i].numIHTConflictAccess = 0;  // The number of Inverted Hash Table Conflict Accesses
                procTable[i].numIHTNULLAccess = 0;              // The number of Empty Inverted Hash Table Accesses
                procTable[i].numIHTNonNULLAccess = 0;           // The number of Non Empty Inverted Hash Table Accesses
                procTable[i].numPageFault = 0;                  // The number of page faults
                procTable[i].numPageHit = 0;                            // The number of page hits
                procTable[i].firstLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*(1<<firstLevelPageTable));
                procTable[i].tracefp = fopen(argv[i+s_flag+4], "r");
                rewind(procTable[i].tracefp);
                InitPageTableEntry(procTable[i].firstLevelPageTable, 1, 1<<firstLevelPageTable);
        }
}
void InitPageTableEntry(struct pageTableEntry *pageTables, int level, int n)
{
        int i=0;
        for(i=0; i<n; i++)
        {
                pageTables[i].level = level;
                pageTables[i].valid = 'N';
                pageTables[i].frameNumber = -1;
        }
}
