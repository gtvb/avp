#ifndef FRAME_QUEUE_H
#define FRAME_QUEUE_H

#include <libavcodec/avcodec.h>

typedef struct _Node {
  AVFrame *frame;
  struct _Node *next;
} Node;

typedef struct FrameQueue {
  int length;
  Node *head, *tail;
} FrameQueue;

FrameQueue *fq_alloc();
int fq_empty(FrameQueue *fq);
void fq_enqueue(FrameQueue *fq, AVFrame *frame);
Node *fq_dequeue(FrameQueue *fq);
void fq_free(FrameQueue *);

#endif // FRAME_QUEUE
