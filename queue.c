#include "queue.h"
#include <stdlib.h>

FrameQueue *fq_alloc() {
  FrameQueue *fq = (FrameQueue *)malloc(sizeof(FrameQueue));
  if (!fq)
    return NULL;

  fq->length = 0;
  fq->head = NULL;
  fq->tail = NULL;

  return fq;
}

void fq_enqueue(FrameQueue *fq, AVFrame *frame) {
  Node *new_node = (Node *)malloc(sizeof(Node));
  if (!new_node) {
    fprintf(stderr, "Failed to allocate memory for new node\n");
    return;
  }

  new_node->frame = frame;
  new_node->next = NULL;

  if (!fq->head) {
    // Queue is empty
    fq->head = fq->tail = new_node;
  } else {
    // Add to the tail
    fq->tail->next = new_node;
    fq->tail = new_node;
  }

  fq->length++;
  fprintf(stderr, "Enqueued frame, queue length: %d\n", fq->length);
}

int fq_empty(FrameQueue *fq) {
  return fq->length == 0;
}

Node *fq_dequeue(FrameQueue *fq) {
  if (fq_empty(fq)) {
    fprintf(stderr, "Queue is empty, cannot dequeue\n");
    return NULL;
  }

  Node *ret = fq->head;
  fq->head = fq->head->next;
  if (!fq->head)
    fq->tail = NULL;

  fq->length--;
  fprintf(stderr, "Dequeued frame, queue length: %d\n", fq->length);

  return ret;
}

void fq_free(FrameQueue *fq) {
  Node *current = fq->head;

  while (current) {
    Node *next = current->next;
    if (current->frame) {
      av_frame_free(&(current->frame));
    }

    free(current);
    current = next;
  }

  free(fq);
  fprintf(stderr, "Freed frame queue\n");
}