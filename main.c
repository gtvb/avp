#include <stdio.h>

#include "media.h"
#include "raylib.h"
#include "gui.h"


// int main(void) {
//     const char *filename = "assets/casa_monstro.mp4";
//     int ret;

//     Media *media = media_alloc();
//     if (!media) {
//         printf("Failed to allocate media.\n");
//         return 1;
//     }

//     strcpy(media->filename, filename);

//     if ((ret = media_init(media, 1280, 720, AV_PIX_FMT_RGBA, filename)) < 0) {
//         printf("Failed to initialize media: %d\n", ret);
//         media_free(media);
//         return 1;
//     }

//     // Perform operations with the media
//     // For example, print media information
//     printf("Media filename: %s\n", media->filename);

//     while (1) {
//         if ((ret = media_read_frame(media)) < 0) {
//             if (ret == MEDIA_ERR_EOF) {
//                 printf("End of file\n");
//                 break;
//             } else {
//                 printf("Failed to read frame: %d\n", ret);
//                 break;
//             }
//         }

//         if ((ret = media_decode(media, 1)) < 0 && ret != MEDIA_ERR_MORE_DATA) {
//             if(ret == MEDIA_ERR_EOF) {
//                 break;
//             }

//             printf("Failed to decode frame: %d\n", ret);
//             break;
//         }

//         if(fq_empty(media->queue) && ret == MEDIA_ERR_MORE_DATA) {
//             continue;
//         }

//         Node *node = fq_dequeue(media->queue);
//         if (!node && ret != MEDIA_ERR_MORE_DATA) {
//             printf("Failed to dequeue frame\n");
//             break;
//         }

//         if (node->type == FRAME_TYPE_VIDEO) {
//             AVFrame *frame = node->frame;
//             printf("Decoded video frame: %dx%d\n", frame->width,
//             frame->height);
//         } else {
//             AVFrame *frame = node->frame;
//             printf("Decoded audio frame: %d samples\n", frame->nb_samples);
//         }
//     }

//     // Free the media
//     media_free(media);
//     return 0;
// }

int main(void) {
    GuiState *state = gui_state_alloc();
    if (!state) {
        printf("Failed to allocate gui state\n");
        return 1;
    }

    gui_state_init(state);
    gui_state_run(state);

    gui_state_free(state);

    return 0;
}