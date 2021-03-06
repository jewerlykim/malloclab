/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// mdriver 구동을 위한 team정보 struct 설정
team_t team = {
    /* Team name */
    "jungle",
    /* First member's full name */
    "seung",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

// 각종 변수,함수 설정
#define WSIZE 4 // word and header footer 사이즈를 byte로.
#define DSIZE 8 // double word size를 byte로
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// size를 pack하고 개별 word 안의 bit를 할당 (size와 alloc을 비트연산)
#define PACK(size, alloc) ((size) | (alloc))

/* address p위치에 words를 read와 write를 한다. */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// address p위치로부터 size를 읽고 field를 할당
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* given block ptr bp, 그것의 header와 footer의 주소를 계산*/
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* GIVEN block ptr bp, 이전 블록과 다음 블록의 주소를 계산*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

static char *heap_listp = 0;
static char *start_nextfit = 0;

static void *coalesce(void *bp)
{
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc)
  { // case 1 - 이전과 다음 블록이 모두 할당 되어있는 경우, 현재 블록의 상태는 할당에서 가용으로 변경
    return bp;
  }
  else if (prev_alloc && !next_alloc)
  {                                        // case2 - 이전 블록은 할당 상태, 다음 블록은 가용상태. 현재 블록은 다음 블록과 통합 됨.
    size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록의 헤더만큼 사이즈 추가?
    PUT(HDRP(bp), PACK(size, 0));          // 헤더 갱신
    PUT(FTRP(bp), PACK(size, 0));          // 푸터 갱신
  }
  else if (!prev_alloc && next_alloc)
  { // case 3 - 이전 블록은 가용상태, 다음 블록은 할당 상태. 이전 블록은 현재 블록과 통합.
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 헤더를 이전 블록의 BLKP만큼 통합?
    bp = PREV_BLKP(bp);
  }
  else
  {                                                                        // case 4- 이전 블록과 다음 블록 모두 가용상태. 이전,현재,다음 3개의 블록 모두 하나의 가용 블록으로 통합.
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 이전 블록 헤더, 다음 블록 푸터 까지로 사이즈 늘리기
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  // if (start_nextfit >= (char *)bp && start_nextfit < NEXT_BLKP((char *)bp))
  // {
  start_nextfit = bp;
  //}
  return bp;
}
static void *extend_heap(size_t words)
{ // 새 가용 블록으로 힙 확장
  char *bp;
  size_t size;
  /* alignment 유지를 위해 짝수 개수의 words를 Allocate */
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1)
  {
    return NULL;
  }

  /* free block 헤더와 푸터를 init하고 epilogue 헤더를 init*/
  PUT(HDRP(bp), PACK(size, 0));         // free block header
  PUT(FTRP(bp), PACK(size, 0));         // free block footer
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header 추가

  /* 만약 prev block이 free였다면, coalesce해라.*/
  return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

  /* create 초기 빈 heap*/
  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
  {
    return -1;
  }
  PUT(heap_listp, 0);
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
  heap_listp += (2 * WSIZE);
  start_nextfit = heap_listp;

  if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    return -1;
  return 0;
}

// 블록을 반환하고 경계 태그 연결 사용 -> 상수 시간 안에 인접한 가용 블록들과 통합하는 함수들
void mm_free(void *bp)
{
  size_t size = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(size, 0)); // header, footer 들을 free 시킨다.
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

static void *find_fit(size_t asize)
{ // first fit 검색을 수행
  void *bp;
  // printf("**********find _ fit **********\n");

  for (bp = start_nextfit; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
  {
    // printf("case1 \n");
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    {
      // printf("case2 \n");
      return bp;
    }
  }
  for (bp = heap_listp; bp != start_nextfit; bp = NEXT_BLKP(bp))
  {
    // printf("case3 \n");
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    {
      // printf("case4 \n");
      return bp;
    }
  }
  /*
    이 아래 코드는 제가 next_fit을 구현했던 코드인데, 한번 보시고 지워주시면 됩니당
    저는 coalesce함수 맨 아래에서 이런 방식으로 if문을 걸어서 
    start_nextfit 포인터가 시작점 <=> NEXT_BLKP 사이에 있는 경우에만 시작점으로 당겨주도록 했는데,
    보석님 코드에서 이 조건을 걸면 오히려 점수가 떨어지더라고요?
    제 코드에서 if문을 빼도 점수는 같았습니다. (보석님 원래 코드는 77점, 제 원래코드는 76점으로 거의동일했습니다.)
    find_fit함수 로직차이같은데 시간동안 무슨 차이인지 찾질 못해서 일단 코드라도 첨부해서 보여드립니다,,
    
    next_fit함수말고 나머지 코드는 거의 같았어요!
    
    if (start_nextfit >= (char *)bp && start_nextfit < NEXT_BLKP((char *)bp))
    {
        start_nextfit = bp;
    }

  */
  /*
    bp = start_nextfit;
    // Search from next_fit to the end of the heap
    for (start_nextfit = bp; GET_SIZE(HDRP(start_nextfit)) > 0; start_nextfit = NEXT_BLKP(start_nextfit))
    {
        if (!GET_ALLOC(HDRP(start_nextfit)) && (asize <= GET_SIZE(HDRP(start_nextfit))))
        {
            // If a fit is found, return the address the of block pointer
            return start_nextfit;
        }
    }
    // If no fit is found by the end of the heap, start the search from the
    // beginning of the heap to the original next_fit location
    for (start_nextfit = heap_listp; start_nextfit < bp; start_nextfit = NEXT_BLKP(start_nextfit))
    {
        if (!GET_ALLOC(HDRP(start_nextfit)) && (asize <= GET_SIZE(HDRP(start_nextfit))))
        {
            return start_nextfit;
        }
    }
    */
  // printf("case5 \n");
  return NULL;
}

static void place(void *bp, size_t asize)
{ // 요청한 블록을 가용 블록의 시작 부분에 배치, 나머지 부분의 크기가 최소 블록크기와 같거나 큰 경우에만 분할하는 함수.
  size_t csize = GET_SIZE(HDRP(bp));

  if ((csize - asize) >= (2 * DSIZE))
  {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize - asize, 0));
    PUT(FTRP(bp), PACK(csize - asize, 0));
  }
  else
  {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

void *mm_malloc(size_t size)
{
  size_t asize;      // 블록 사이즈 조정
  size_t extendsize; // heap에 맞는 fit이 없으면 확장하기 위한 사이즈
  char *bp;

  /* 거짓된 요청 무시*/
  if (size == 0)
    return NULL;

  /* overhead, alignment 요청 포함해서 블록 사이즈 조정*/
  if (size <= DSIZE)
  {
    asize = 2 * DSIZE;
  }
  else
  {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }
  /* fit에 맞는 free 리스트를 찾는다.*/
  if ((bp = find_fit(asize)) != NULL)
  {
    place(bp, asize);
    return bp;
  }
  /* fit 맞는게 없다. 메모리를 더 가져와 block을 위치시킨다.*/
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
  {
    return NULL;
  }
  place(bp, asize);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
  void *oldptr = ptr;
  void *newptr;
  size_t copySize;

  newptr = mm_malloc(size);
  if (newptr == NULL)
    return NULL;
  copySize = GET_SIZE(HDRP(oldptr));
  if (size < copySize)
    copySize = size;
  memcpy(newptr, oldptr, copySize);
  mm_free(oldptr);
  return newptr;
}
