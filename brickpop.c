#include <stdlib.h>
#include <stdio.h>
#include <png.h>
#include <stdint.h>

png_bytep *row_pointers;

// Grabbed from an example online
void read_png_file(char *filename) {
    FILE *fp = fopen(filename, "rb");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) abort();

    png_infop info = png_create_info_struct(png);
    if(!info) abort();

    if(setjmp(png_jmpbuf(png))) abort();

    png_init_io(png, fp);

    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    // Read any color_type into 8bit depth, RGBA format.
    // See http://www.libpng.org/pub/png/libpng-manual.txt

    if(bit_depth == 16)
        png_set_strip_16(png);

    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
       png_set_expand_gray_1_2_4_to_8(png);

    if(png_get_valid(png, info, PNG_INFO_tRNS))
       png_set_tRNS_to_alpha(png);

    // These color_type don't have an alpha channel then fill it with 0xff.
    if(color_type == PNG_COLOR_TYPE_RGB ||
       color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if(color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for(int y = 0; y < height; y++){
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
    }

    png_read_image(png, row_pointers);

    fclose(fp);
}

unsigned char isColor(png_bytep pixel, unsigned char r, unsigned char g, unsigned char b){
    if(pixel[0] == r && pixel[1] == g && pixel[2] == b)
        return 1;
    return 0;
}

struct brickColor{
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

unsigned char isBrickColor(png_bytep pixel, struct brickColor brick){
    return isColor(pixel, brick.r, brick.g, brick.b);
}

#define getTile(X, Y) ((Y)*10+(X))

#define NUM_COLORS 6

// No 7th color
#define EMPTY NUM_COLORS

// Starting height offset of middle of first block
#define START_Y 525
// Height distance from middle of one block to another
#define Y_HEIGHT 108

// Starting width offset of middle of first block
#define START_X 52
// Width distance from middle of one block to another
#define X_WIDTH 108

char* getGameArray() {

    struct brickColor colors[6];
    int numColors = 0;
    
    // Need to find background color incase starting with a non full board
    struct brickColor backgroundColor;
    png_bytep empty_row = &row_pointers[START_Y - Y_HEIGHT][START_X*4];
    backgroundColor.r = empty_row[0];
    backgroundColor.g = empty_row[1];
    backgroundColor.b = empty_row[2];

    char *startArray = malloc(100);
    memset(startArray, EMPTY, 100);
    for(int y = 0; y < 10; y++) {
        png_bytep row = row_pointers[START_Y + y*Y_HEIGHT];
        for(int x = 0; x < 10; x++){
            png_bytep px = &(row[(START_X + x*X_WIDTH)*4]); //*4 is for 4 bytes per pixel

            if(isBrickColor(px, backgroundColor)){
                continue;
            }

            int found = 0;
            for(int color = 0; color < numColors; color++){
                if(isBrickColor(px, colors[color])){
                    found = 1;
                    startArray[getTile(x, y)] = color;
                    break;
                }
            }
            if(!found){
                if(numColors == NUM_COLORS){
                    printf("Unexpected number of colors\n");
                    exit(1);
                }
                colors[numColors].r = px[0];
                colors[numColors].g = px[1];
                colors[numColors].b = px[2];
                startArray[getTile(x, y)] = numColors;
                numColors++;
            }
        }
    }
    return startArray;
}


void printArray(char *array){
    printf("Board:\n");
    printf("----------------------\n");
    for(int y = 0; y < 10; y++){
        printf("|");
        for(int x = 0; x < 10; x++){
            char color = array[getTile(x, y)];
            if(color == EMPTY)
                printf("  ", array[getTile(x, y)]);
            else
                printf("%d ", array[getTile(x, y)]);
        }
        printf("|\n");
    }
    printf("----------------------\n");
}

int clearBlock(char *array, int color, int x, int y){
    //printf("Clear block %dx%d\n", x, y);
    int clearCount = 1;
    array[getTile(x, y)] = EMPTY;
    if(x > 0)
        if(array[getTile(x-1, y)] == color)
            clearCount += clearBlock(array, color, x-1, y);

    if(x < 9)
        if(array[getTile(x+1, y)] == color)
            clearCount += clearBlock(array, color, x+1, y);

    if(y > 0)
        if(array[getTile(x, y-1)] == color)
            clearCount += clearBlock(array, color, x, y-1);

    if(y < 9)
        if(array[getTile(x, y+1)] == color)
            clearCount += clearBlock(array, color, x, y+1);
    return clearCount;
}

// Perform a move at position x,y and update game board
void performMove(char *array, int x, int y){

    // Clear blocks
    clearBlock(array, array[getTile(x, y)], x, y);

    // Drop blocks down
    for(int x = 0; x < 10; x++){
        int y = 9;
        int lowestY = 9;
        while(y > 0){
            if(array[getTile(x, y)] == EMPTY){
    
                for(int newY = y-1; newY >= 0; newY--){
                    if(array[getTile(x, newY)] != EMPTY){
                        //printf("Dropping block at %d %d\n", x, y);
                        array[getTile(x, y)] = array[getTile(x, newY)];
                        array[getTile(x, newY)] = EMPTY;
                        break;
                    }else if(newY == 0){
                        y = 0;
                    }
                }

            }
            y--;
        }
    }


    // Find empty columns
    char emptyCols[10];
    for(int x = 0; x < 10; x++){
        unsigned char isEmpty = 1;
        for(int y = 0; y < 10; y++){
            if(array[getTile(x, y)] != EMPTY){
                isEmpty = 0;
                break;
            } 
        }
        if(isEmpty)
            emptyCols[x] = 1;
        else
            emptyCols[x] = 0;
    }


    // Shift cols left if empty
    for(int x = 0; x < 9; x++){
        if(emptyCols[x]){
            for(int newX = x+1; newX < 10; newX++){
                if(emptyCols[newX] == 0){
                    //printf("Shifting col at %d\n", x);
                    for(int y = 0; y < 10; y++){
                        array[getTile(x, y)] = array[getTile(newX, y)];
                        array[getTile(newX, y)] = EMPTY;
                        emptyCols[newX] = 1;
                    }
                    break;
                }
            }
        }
    }
}

struct move{
    struct move* next;
    char x;
    char y;
    char size;
    char color;
};

struct movesInfo{
    struct move moves[50]; // Max number of moves available on one board
    int numMoves;
    int singleBlocks[6];
    int movesByColor[6];
};

// Get all available moves and info for given board
void getMoves(char* array, struct movesInfo* moves){
    
    char *movesArray = malloc(100);
    memcpy(movesArray, array, 100);
    for(int color = 0; color < 6; color++){
        moves->singleBlocks[color] = 0;
        moves->movesByColor[color] = 0;
    }

    int numMoves = 0;
    for(int y = 0; y < 10; y++){
        for(int x = 0; x < 10; x++){
            char blockColor = movesArray[getTile(x, y)];
            if (blockColor == EMPTY)
                continue;
            if((x < 9 && blockColor == movesArray[getTile(x+1, y)]) || (y < 9 && blockColor == movesArray[getTile(x, y+1)])){
                //printf("We have a move at %dx%d\n", x, y);
                int clearCount = clearBlock(movesArray, blockColor, x, y);
                moves->movesByColor[blockColor]++;
                struct move* newMove = &moves->moves[numMoves];
                newMove->x = x;
                newMove->y = y;
                newMove->size = clearCount;
                newMove->color = blockColor;
                numMoves++;
            }else{
                moves->singleBlocks[blockColor]++;
            }
        }
    }
    moves->numMoves = numMoves;

    if(movesArray[getTile(9,9)] != EMPTY)
        moves->singleBlocks[movesArray[getTile(9,9)]]++;

    free(movesArray);
}

int sizeCompare(const void * a, const void * b){
    return ((struct move*)b)->size - ((struct move*)a)->size;
}

#define MAX_MOVES 100000000
struct move* searchMoves(char* array, int* movesChecked, int depth){

    struct movesInfo moves;
    getMoves(array, &moves);

    
    for(int color = 0; color < 6; color++){
        if(moves.singleBlocks[color] == 1 && moves.movesByColor[color] == 0){
            //printf("Should not be reached\n");
            return NULL;
        }
    }

    qsort((struct move*)moves.moves, moves.numMoves, sizeof(struct move), sizeCompare);

    for(int i = 0; i < moves.numMoves; i++){
        struct move* move = &moves.moves[i];

        int x = move->x;
        int y = move->y;
        char color = move->color;
        if(moves.singleBlocks[color] == 1 && moves.movesByColor[color] == 1){
            (*movesChecked)++;
            continue;
        }

        char *moveArray = malloc(100);
        memcpy(moveArray, array, 100);

        performMove(moveArray, x, y);
        //printArray(moveArray);
                
        char isEmpty = 0;
        if(moves.numMoves == 1){
            isEmpty = 1;
            for(int color = 0; color < 6; color++){
                if(moves.singleBlocks[color] != 0){
                    isEmpty = 0;
                    break;
                }
            }
        }

        if(isEmpty){
            //printf("Is empty, returning\n");
            free(moveArray);
            struct move* newMove = malloc(sizeof(struct move));
            newMove->x = x;
            newMove->y = y;
            newMove->next = NULL;
            return newMove;
        }

        struct move* resultMove = searchMoves(moveArray, movesChecked, depth+1);
        free(moveArray);
        if(resultMove/* || (*movesChecked > MAX_MOVES && depth == 0)*/){
            struct move* newMove = malloc(sizeof(struct move));
            newMove->x = x;
            newMove->y = y;
            newMove->next = resultMove;
            return newMove;
        }

        (*movesChecked)++;
        //if(*movesChecked > MAX_MOVES)
        //    return NULL;
    }
    //printf("Dead end ");
    //printArray(array);
    return NULL;
}


int main(int argc, char *argv[]) {
    
    while(1){
        // Grab a screenshot and download it
        system("adb shell screencap -p /sdcard/screen.png");
        system("adb pull /sdcard/screen.png");

        read_png_file("screen.png");
        char* startArray = getGameArray();

        printArray(startArray);
        int movesChecked = 0;
        struct move* firstMove = searchMoves(startArray, &movesChecked, 0);
        printf("Done searching for moves, movesChecked: %d\n", movesChecked);

        if(firstMove == NULL){
            printf("No moves found, game over\n");
            return 0;
        }
        while(firstMove){
            printf("Move %dx%d\n", firstMove->x, firstMove->y);

            char input[100];
            sprintf(input, "adb shell input tap %d %d\n", START_X + firstMove->x*X_WIDTH, START_Y + firstMove->y*Y_HEIGHT);
            system(input);
            usleep(2500*1000); //2.5sec wait between every tap for animations

            firstMove = firstMove->next;
        }
        sleep(6);
    }
    return 0;
}
