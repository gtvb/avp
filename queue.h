#ifndef FRAME_QUEUE_H
#define FRAME_QUEUE_H

#include "common.h"
#include <libavcodec/avcodec.h>

typedef struct _Node {
  AVFrame *frame;
  enum FrameType type;
  struct _Node *next;
} Node;

typedef struct FrameQueue {
  int length;
  Node *head;
} FrameQueue;

void node_free(Node *node);

FrameQueue *fq_alloc();
int fq_empty(FrameQueue *fq);
void fq_enqueue(FrameQueue *fq, AVFrame *frame, enum FrameType type);
Node *fq_dequeue(FrameQueue *fq);
void fq_free(FrameQueue *);

#endif // FRAME_QUEUE
