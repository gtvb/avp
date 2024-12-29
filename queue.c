#include "queue.h"

#include <stdlib.h>

FrameQueue *fq_alloc() {
    FrameQueue *fq = malloc(sizeof(FrameQueue));
    if (!fq) {
        return NULL;
    }

    fq->length = 0;
    fq->head = NULL;

    return fq;
}

int fq_empty(FrameQueue *fq) { return fq->length == 0; }

void fq_enqueue(FrameQueue *fq, AVFrame *frame, enum FrameType type) {
    Node *node = malloc(sizeof(Node));
    if (!node) {
        return;
    }

    node->frame = frame;
    node->type = type;
    node->next = NULL;

    // This is the first element being inserted
    if (fq_empty(fq)) {
        fq->head = node;
        fq->length++;
        return;
    }

    // For all other cases, search for the last element
    // on the list and insert the new node there
    Node *current = fq->head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = node;
    fq->length++;
}

Node *fq_dequeue(FrameQueue *fq) {
    if (fq_empty(fq)) {
        return NULL;
    }

    Node *node = fq->head;
    if (node) {
        fq->head = node->next;
        node->next = NULL;
        fq->length--;
    }

    return node;
}

void fq_free(FrameQueue *fq) {
    while (!fq_empty(fq)) {
        Node *node = fq_dequeue(fq);
        node->next = NULL;
        av_frame_free(&node->frame);
        free(node);
    }
    free(fq);
}

void node_free(Node *node) {
    node->next = NULL;
    av_frame_free(&node->frame);
    free(node);
}