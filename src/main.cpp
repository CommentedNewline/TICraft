#include <graphx.h>
#include <fileioc.h>
#include <ti/getcsc.h>
#include <sys/power.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "renderer.hpp"
#include "textures.hpp"
#include "saves.hpp"
#include "crc32.h"
#include "cursor.hpp"
#define freeMem 0xD02587

extern "C" {
    void ti_CleanAll();
    int ti_MemChk();
    #include "printString.h"
}

/*
Implement chunking
Idea:
Load 1 chunk into the list of cubes at a time (say, when the cursor moves into a new chunk or the camera moves)
Generate the polygons for each chunk, then delete it from the list of cubes and load the next one
At the end load the current chunk into the list of cubes
Pros:
Allows many more blocks at theoretically no additional RAM cost
Cons:
Doing a re-render will be INSANELY expensive
*/

struct packEntry {
    texturePack* pack;
    unsigned int size;
};

void selectBlock();
void drawSelection(int offset);
void texturePackError(const char* message);
void texturePackMenu();
void drawTexturePackSelection(texturePack* pack, int row, bool selected);
bool verifyTexturePack(packEntry pack);
void exitOverlay(int code) {memset((void*) 0xD031F6, 0, 69090); exit(code);};

bool fineMovement;

int main() {
    os_ClrHomeFull();
    ti_SetGCBehavior(gfx_End, texturePackMenu);
    // what exactly do you do?
    // ti_CleanAll();
    // sizeof(object*)*1000 + (255*255) + 2 = 68027B (66.43 KiB)
    if (ti_MemChk() < (255*255) + 2) {
        os_PutStrFull("WARNING!! Not enough free user mem to run TICRAFT! Continuing may result in corruption or your calculator crashing and resetting. Try deleting or archiving some programs or AppVars. Press \"Enter\" to continue or press any other key to exit.");
        uint8_t key;
        while (!(key = os_GetCSC()));
        if (key != sk_Enter) {
            return 1;
        }
        os_ClrHomeFull();
    }
    cursorBackground = *((gfx_sprite_t**) freeMem);
    // texture pack selection menu goes here
    texturePackMenu();
    ti_SetGCBehavior(gfx_End, gfxStart);
    bool userSelected = false;
    bool toSaveOrNotToSave = true;
    gfxStart();
    gfx_SetColor(0);
    gfx_SetTextScale(1, 1);
    char nameBuffer[10];
    bool emergencyExit = false;
    bool usb = false;
    load_save:
    while (!emergencyExit && mainMenu(nameBuffer, 10)) {
        bool quit = false;
        while (!quit) {
            fillDirt();
            gfx_SetTextXY(0, 105);
            gfx_SetTextFGColor(254);
            printStringAndMoveDownCentered("Would you like to load from archive or USB?");
            printStringAndMoveDownCentered("Press 1 for archive, press 2 for USB.");
            printStringAndMoveDownCentered("Or press clear to cancel.");
            userSelected = false;
            while (!userSelected) {
                switch (os_GetCSC()) {
                    case sk_1:
                        usb = false;
                        quit = true;
                        userSelected = true;
                        break;
                    case sk_2:
                        printStringAndMoveDownCentered("Please plug in a FAT32 formatted USB drive now.");
                        printStringAndMoveDownCentered("Do not unplug the drive until");
                        printStringAndMoveDownCentered("the save is loaded.");
                        printStringAndMoveDownCentered("Press any key to cancel loading from USB");
                        printStringAndMoveDownCentered("(Doing so will result in the genration of");
                        printStringAndMoveDownCentered("a new world)");
                        usb = true;
                        quit = true;
                        userSelected = true;
                        break;
                    case sk_Clear:
                        goto load_save;
                        break;
                    default:
                        break;
                }
            }
        }
        toSaveOrNotToSave = checkSave(nameBuffer, usb);
        fillDirt();
        gfx_sprite_t* background2 = (gfx_sprite_t*)(((uint8_t*) cursorBackground) + 6402);
        cursorBackground->width = 160;
        background2->width = 160;
        cursorBackground->height = 40;
        background2->height = 40;
        gfx_GetSprite(cursorBackground, 0, 0);
        gfx_GetSprite(background2, 160, 0);
        gfx_SetTextFGColor(254);
        gfx_SetTextScale(4, 4);
        gfx_SetTextXY(0, 0);
        printStringCentered("Loading...", 5);
        gfx_SetTextXY(0, 40);
        gfx_SetTextScale(1,1);
        printStringAndMoveDownCentered("Welcome to TICraft v2.0.1! While you wait:");
        printStringAndMoveDownCentered("Controls:");
        printStringAndMoveDownCentered("Graph/Clear: Exit/Emergency Exit");
        printStringAndMoveDownCentered("Cursor controls:");
        printStringAndMoveDownCentered("5: Place/Remove a block");
        printStringAndMoveDownCentered("8/2/4/6: Move forwards/backwards/left/right");
        printStringAndMoveDownCentered("Multiply/Subtract: Move up/down");
        printStringAndMoveDownCentered("9/1: Move diagonally forward/backward");
        printStringAndMoveDownCentered("7/3: Move diagonally left/right");
        printStringAndMoveDownCentered("Camera controls:");
        printStringAndMoveDownCentered("Up/Down: Move forwards/backwards");
        printStringAndMoveDownCentered("Left/Right: Move left/right");
        printStringAndMoveDownCentered("Del/Stat: Move up/down");
        printStringAndMoveDownCentered("Prgm/Cos/Sin/Tan: Rotate up/down/left/right");
        printStringAndMoveDownCentered("Alpha: Move to show the cursor");
        printStringAndMoveDownCentered("Enter: Show the block selection menu");
        printStringAndMoveDownCentered("Mode: Perform a full re-render of the screen");
        printStringAndMoveDownCentered("Made by Logan C.");
        // implement more controls in the near future
        if (!toSaveOrNotToSave) {
            playerCursor.moveTo(20, 20, 20);
            resetCamera();
            selectedObject = 10;
            for (int i = 0; i < 400; i++) {
                if (numberOfObjects < maxNumberOfObjects) {
                    // workaround for compiler bug
                    div_t xy = div(i, 20);
                    objects[numberOfObjects] = new object((xy.rem)*20, 0, (xy.quot)*20, 10, false);
                    numberOfObjects++;
                }
            }
        } else {
            load();
        }
        fineMovement = false;
        __asm__ ("di");
        for (unsigned int i = 0; i < numberOfObjects; i++) {
            objects[i]->generatePoints();
        }
        __asm__ ("ei");
        memcpy(zSortedObjects, objects, sizeof(object*) * numberOfObjects);
        xSort();
        gfx_Sprite_NoClip(cursorBackground, 0, 0);
        gfx_Sprite_NoClip(background2, 160, 0);
        cursorBackground->height = 1;
        cursorBackground->width = 1;
        printStringAndMoveDownCentered("Press any key to begin.");
        gfx_SetTextScale(4,4);
        printStringCentered("Loaded!", 5);
        gfx_SetTextScale(1,1);
        while (!os_GetCSC());
        drawScreen(true);
        getBuffer();
        drawCursor();
        quit = false;
        while (!quit) {
            switch (os_GetCSC()) {
                // forward
                case sk_9:
                    if (angleY < -67.5f) {
                        moveCursor(6);
                    } else if (angleY < -22.5f) {
                        moveCursor(2);
                    } else if (angleY < 22.5f) {
                        moveCursor(8);
                    } else if (angleY < 67.5f) {
                        moveCursor(4);
                    } else if (angleY < 112.5f) {
                        moveCursor(7);
                    } else if (angleY < 157.5f) {
                        moveCursor(3);
                    } else if (angleY < 202.5f) {
                        moveCursor(9);
                    } else if (angleY < 247.5f) {
                        moveCursor(5);
                    } else {
                        moveCursor(6);
                    }
                    break;
                // backward
                case sk_1:
                    if (angleY < -67.5f) {
                        moveCursor(7);
                    } else if (angleY < -22.5f) {
                        moveCursor(3);
                    } else if (angleY < 22.5f) {
                        moveCursor(9);
                    } else if (angleY < 67.5f) {
                        moveCursor(5);
                    } else if (angleY < 112.5f) {
                        moveCursor(6);
                    } else if (angleY < 157.5f) {
                        moveCursor(2);
                    } else if (angleY < 202.5f) {
                        moveCursor(8);
                    } else if (angleY < 247.5f) {
                        moveCursor(4);
                    } else {
                        moveCursor(7);
                    }
                    break;
                // left
                case sk_7:
                    if (angleY < -67.5f) {
                        moveCursor(9);
                    } else if (angleY < -22.5f) {
                        moveCursor(5);
                    } else if (angleY < 22.5f) {
                        moveCursor(6);
                    } else if (angleY < 67.5f) {
                        moveCursor(2);
                    } else if (angleY < 112.5f) {
                        moveCursor(8);
                    } else if (angleY < 157.5f) {
                        moveCursor(4);
                    } else if (angleY < 202.5f) {
                        moveCursor(7);
                    } else if (angleY < 247.5f) {
                        moveCursor(3);
                    } else {
                        moveCursor(9);
                    }
                    break;
                // right
                case sk_3:
                    if (angleY < -67.5f) {
                        moveCursor(8);
                    } else if (angleY < -22.5f) {
                        moveCursor(4);
                    } else if (angleY < 22.5f) {
                        moveCursor(7);
                    } else if (angleY < 67.5f) {
                        moveCursor(3);
                    } else if (angleY < 112.5f) {
                        moveCursor(9);
                    } else if (angleY < 157.5f) {
                        moveCursor(5);
                    } else if (angleY < 202.5f) {
                        moveCursor(6);
                    } else if (angleY < 247.5f) {
                        moveCursor(2);
                    } else {
                        moveCursor(8);
                    }
                    break;
                // up
                case sk_Mul:
                    moveCursor(0);
                    break;
                case sk_Sub:
                    moveCursor(1);
                    break;
                case sk_8:
                    if (angleY < -67.5f) {
                        moveCursor(5);
                    } else if (angleY < -22.5f) {
                        moveCursor(6);
                    } else if (angleY < 22.5f) {
                        moveCursor(2);
                    } else if (angleY < 67.5f) {
                        moveCursor(8);
                    } else if (angleY < 112.5f) {
                        moveCursor(4);
                    } else if (angleY < 157.5f) {
                        moveCursor(7);
                    } else if (angleY < 202.5f) {
                        moveCursor(3);
                    } else if (angleY < 247.5f) {
                        moveCursor(9);
                    } else {
                        moveCursor(5);
                    }
                    break;
                case sk_2:
                    if (angleY < -67.5f) {
                        moveCursor(4);
                    } else if (angleY < -22.5f) {
                        moveCursor(7);
                    } else if (angleY < 22.5f) {
                        moveCursor(3);
                    } else if (angleY < 67.5f) {
                        moveCursor(9);
                    } else if (angleY < 112.5f) {
                        moveCursor(5);
                    } else if (angleY < 157.5f) {
                        moveCursor(6);
                    } else if (angleY < 202.5f) {
                        moveCursor(2);
                    } else if (angleY < 247.5f) {
                        moveCursor(8);
                    } else {
                        moveCursor(4);
                    }
                    break;
                case sk_4:
                    if (angleY < -67.5f) {
                        moveCursor(3);
                    } else if (angleY < -22.5f) {
                        moveCursor(9);
                    } else if (angleY < 22.5f) {
                        moveCursor(5);
                    } else if (angleY < 67.5f) {
                        moveCursor(6);
                    } else if (angleY < 112.5f) {
                        moveCursor(2);
                    } else if (angleY < 157.5f) {
                        moveCursor(8);
                    } else if (angleY < 202.5f) {
                        moveCursor(4);
                    } else if (angleY < 247.5f) {
                        moveCursor(7);
                    } else {
                        moveCursor(3);
                    }
                    break;
                case sk_6:
                    if (angleY < -67.5f) {
                        moveCursor(2);
                    } else if (angleY < -22.5f) {
                        moveCursor(8);
                    } else if (angleY < 22.5f) {
                        moveCursor(4);
                    } else if (angleY < 67.5f) {
                        moveCursor(7);
                    } else if (angleY < 112.5f) {
                        moveCursor(3);
                    } else if (angleY < 157.5f) {
                        moveCursor(9);
                    } else if (angleY < 202.5f) {
                        moveCursor(5);
                    } else if (angleY < 247.5f) {
                        moveCursor(6);
                    } else {
                        moveCursor(2);
                    }
                    break;
                case sk_5: {
                    object* annoyingPointer = &playerCursor;
                    object** matchingObject = (object**) bsearch(&annoyingPointer, objects, numberOfObjects, sizeof(object*), xCompare);
                    bool deletedObject = false;
                    drawBuffer();
                    if (matchingObject != nullptr) {
                        while (matchingObject > &objects[0] && (*(matchingObject-1))->x == playerCursor.x) {
                            matchingObject--;
                        }
                        while (matchingObject < &objects[numberOfObjects] && (*matchingObject)->x == playerCursor.x) {
                            if ((*matchingObject)->y == playerCursor.y && (*matchingObject)->z == playerCursor.z) {
                                deletedObject = true;
                                break;
                            }
                            matchingObject++;
                        }
                    }
                    if (deletedObject) {
                        deletedObject = false;
                        object** zSortedPointer = (object**) bsearch(matchingObject, zSortedObjects, numberOfObjects, sizeof(object*), distanceCompare);
                        while (zSortedPointer > zSortedObjects && distanceCompare(zSortedPointer - 1, matchingObject) == 0) {
                            zSortedPointer--;
                        }
                        while (zSortedPointer < zSortedObjects + numberOfObjects && distanceCompare(zSortedPointer, matchingObject) == 0) {
                            if ((*zSortedPointer)->x == playerCursor.x && (*zSortedPointer)->y == playerCursor.y && (*zSortedPointer)->z == playerCursor.z) {
                                deletedObject = true;
                                memmove(zSortedPointer, zSortedPointer + 1, sizeof(object*) * (size_t)(zSortedObjects + numberOfObjects - zSortedPointer - 1));
                                break;
                            }
                            zSortedPointer++;
                        }
                        // just incase something goes wrong in the search
                        if (!deletedObject) {
                            gfx_End();
                            exitOverlay(1);
                        }
                        object* matchingObjectReference = *matchingObject;
                        memmove(matchingObject, matchingObject + 1, sizeof(object*) * (size_t)(objects + numberOfObjects - matchingObject - 1));
                        numberOfObjects--;
                        matchingObjectReference->deleteObject();
                        getBuffer();
                    } else {
                        if (numberOfObjects < maxNumberOfObjects) {
                            object* newObject = new object(playerCursor.x, playerCursor.y, playerCursor.z, selectedObject, false);
                            __asm__ ("di");
                            newObject->generatePoints();
                            __asm__ ("ei");
                            matchingObject = objects;
                            while (matchingObject < objects + numberOfObjects) {
                                if ((*matchingObject)->x >= playerCursor.x) {
                                    memmove(matchingObject + 1, matchingObject, sizeof(object*) * (size_t)(objects + numberOfObjects - matchingObject));
                                    break;
                                }
                                matchingObject++;
                            }
                            *matchingObject = newObject;
                            matchingObject = zSortedObjects;
                            while (matchingObject < zSortedObjects + numberOfObjects) {
                                if (distanceCompare(matchingObject, &newObject) >= 0) {
                                    memmove(matchingObject + 1, matchingObject, sizeof(object*) * (size_t)(zSortedObjects + numberOfObjects - matchingObject));
                                    break;
                                }
                                matchingObject++;
                            }
                            *matchingObject = newObject;
                            numberOfObjects++;
                            newObject->generatePolygons();
                            getBuffer();
                        }
                    }
                    drawCursor();
                    break;
                }
                case sk_Mode:
                    drawScreen(true);
                    getBuffer();
                    drawCursor();
                    break;
                // exit
                case sk_Graph:
                    gfx_SetDrawBuffer();
                    gfx_SetTextFGColor(254);
                    gfx_SetTextXY(0, 105);
                    fillDirt();
                    printStringAndMoveDownCentered("Would you like to save?");
                    printStringAndMoveDownCentered("Press 1 for yes, 2 for no.");
                    printStringAndMoveDownCentered("Or press graph again to return to the game.");
                    gfx_BlitBuffer();
                    gfx_SetDrawScreen();
                    userSelected = false;
                    while (!userSelected) {
                        switch (os_GetCSC()) {
                            case sk_1:
                                save(nameBuffer);
                            case sk_2:
                                userSelected = true;
                                quit = true;
                                break;
                            case sk_Graph:
                                quit = false;
                                userSelected = true;
                                drawScreen(true);
                                getBuffer();
                                drawCursor();
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case sk_Clear:
                    quit = true;
                    emergencyExit = true;
                    break;
                case sk_Enter:
                    selectBlock();
                    break;
                case sk_Up:
                    if (fineMovement) {
                        cameraXYZ[0] += (Fixed24)10*sy;
                        cameraXYZ[2] += (Fixed24)10*cy;
                    } else {
                        cameraXYZ[0] += (Fixed24)40*sy;
                        cameraXYZ[2] += (Fixed24)40*cy;
                    }
                    redrawScreen();
                    break;
                case sk_Down:
                    if (fineMovement) {
                        cameraXYZ[0] -= (Fixed24)10*sy;
                        cameraXYZ[2] -= (Fixed24)10*cy;
                    } else {
                        cameraXYZ[0] -= (Fixed24)40*sy;
                        cameraXYZ[2] -= (Fixed24)40*cy;
                    }
                    redrawScreen();
                    break;
                case sk_Left:
                    if (fineMovement) {
                        cameraXYZ[0] -= (Fixed24)10*cy;
                        cameraXYZ[2] += (Fixed24)10*sy;
                    } else {
                        cameraXYZ[0] -= (Fixed24)40*cy;
                        cameraXYZ[2] += (Fixed24)40*sy;
                    }
                    redrawScreen();
                    break;
                case sk_Right:
                    if (fineMovement) {
                        cameraXYZ[0] += (Fixed24)10*cy;
                        cameraXYZ[2] -= (Fixed24)10*sy;
                    } else {
                        cameraXYZ[0] += (Fixed24)40*cy;
                        cameraXYZ[2] -= (Fixed24)40*sy;
                    }
                    redrawScreen();
                    break;
                case sk_Del:
                    if (fineMovement) {
                        cameraXYZ[1] += 5;
                    } else {
                        cameraXYZ[1] += 20;
                    }
                    redrawScreen();
                    break;
                case sk_Stat:
                    if (fineMovement) {
                        cameraXYZ[1] -= 5;
                    } else {
                        cameraXYZ[1] -= 20;
                    }
                    redrawScreen();
                    break;
                case sk_Prgm:
                    if (fineMovement) {
                        rotateCamera(-2.5, 0);
                    } else {
                        rotateCamera(-10, 0);
                    }
                    break;
                case sk_Cos:
                    if (fineMovement) {
                        rotateCamera(2.5, 0);
                    } else {
                        rotateCamera(10, 0);
                    }
                    break;
                case sk_Sin:
                    if (fineMovement) {
                        rotateCamera(0, -2.5);
                    } else {
                        rotateCamera(0, -10);
                    }
                    break;
                case sk_Tan:
                    if (fineMovement) {
                        rotateCamera(0, 2.5);
                    } else {
                        rotateCamera(0, 10);
                    }
                    break;
                case sk_Alpha:
                    cameraXYZ[0] = (Fixed24)playerCursor.x - (((Fixed24)84.85281375f)*sy);
                    cameraXYZ[1] = (Fixed24)playerCursor.y + (Fixed24)75;
                    cameraXYZ[2] = (Fixed24)playerCursor.z - (((Fixed24)84.85281375f)*cy);
                    redrawScreen();
                    break;
                case sk_Window:
                    takeScreenshot();
                    break;
                case sk_Trace:
                    fineMovement = !fineMovement;
                    break;
                default:
                    break;
            }
        }
        deleteEverything();
    }
    gfx_SetDrawScreen();
    gfx_FillScreen(254);
    gfx_End();
    // clear out pixel shadow
    memset((void*) 0xD031F6, 0, 69090);
    os_ClrHomeFull();
    return 0;
}

void selectBlock() {
    drawBuffer();
    shadeScreen();
    cursorBackground->width = 16;
    cursorBackground->height = 16;
    gfx_SetColor(253);
    gfx_FillRectangle(72, 42, 176, 156);
    for (uint8_t i = 0; i < 48; i++) {
        if (i == selectedObject) {
            gfx_SetColor(254);
            gfx_FillRectangle(80 + (20*(i%8)), 50 + (24*(i/8)), 20, 20);
        }
        memcpy(cursorBackground->data, textures[i][1], 256);
        gfx_Sprite_NoClip(cursorBackground, 82 + (20*(i%8)), 52 + (24*(i/8)));
    }
    bool quit = false;
    while (!quit) {
        uint8_t key = os_GetCSC();
        if (key) {
            switch (key) {
                case sk_Up:
                case sk_8:
                    if (selectedObject/8 > 0) {
                        drawSelection(-8);
                    } else {
                        drawSelection(40);
                    }
                    break;
                case sk_Down:
                case sk_2:
                    if (selectedObject/8 < 5) {
                        drawSelection(8);
                    } else {
                        drawSelection(-40);
                    }
                    break;
                case sk_Left:
                case sk_4:
                    if (selectedObject > 0) {
                        drawSelection(-1);
                    } else {
                        drawSelection(((int)-selectedObject) + 47);
                    }
                    break;
                case sk_Right:
                case sk_6:
                    if (selectedObject < 47) {
                        drawSelection(1);
                    } else {
                        drawSelection(((int)-selectedObject));
                    }
                    break;
                case sk_Enter:
                    quit = true;
                    break;
                case sk_Clear:
                    quit = true;
                    break;
            }
        }
    }
    gfx_palette[255] = texPalette[255];
    drawScreen(true);
    getBuffer();
    drawCursor();
}

void drawSelection(int offset) {
    gfx_SetColor(253);
    gfx_FillRectangle(80 + (20*(selectedObject%8)), 50 + (24*(selectedObject/8)), 20, 20);
    memcpy(cursorBackground->data, textures[selectedObject][1], 256);
    gfx_Sprite_NoClip(cursorBackground, 82 + (20*(selectedObject%8)), 52 + (24*(selectedObject/8)));
    selectedObject += offset;
    gfx_SetColor(254);
    gfx_FillRectangle(80 + (20*(selectedObject%8)), 50 + (24*(selectedObject/8)), 20, 20);
    memcpy(cursorBackground->data, textures[selectedObject][1], 256);
    gfx_Sprite_NoClip(cursorBackground, 82 + (20*(selectedObject%8)), 52 + (24*(selectedObject/8)));
}

void texturePackError(const char* message) {
    gfx_End();
    os_ClrHomeFull();
    os_PutStrFull("Texture pack is invalid (");
    os_PutStrFull(message);
    os_PutStrFull("). Please select another texture pack or load on the texture pack again. Press any key to continue.");
    while (!os_GetCSC());
}

void fontPrintString(const char* string, uint16_t row) {
    os_FontDrawText(string, (LCD_WIDTH - os_FontGetWidth(string))>>1, row);
}

void texturePackMenu() {
    char* name;
    void* vat_ptr = nullptr;
    char** texturePackNames = (char**) malloc(0);
    unsigned int numberOfTexturePacks = 0;
    while ((name = ti_Detect(&vat_ptr, TICRAFTTexturePackMagic))) {
        uint8_t texturePackHandle = ti_Open(name, "r");
        ti_SetArchiveStatus(true, texturePackHandle);
        texturePack* pack = (texturePack*) ti_GetDataPtr(texturePackHandle);
        ti_Close(texturePackHandle);
        if (pack->version == TICRAFTTexturePackVersion) {
            texturePackNames = (char**) realloc(texturePackNames, sizeof(char*) * (numberOfTexturePacks + 1));
            // just remember to delete all of these strings when we're done
            texturePackNames[numberOfTexturePacks] = new char[strlen(name) + 1];
            strcpy(texturePackNames[numberOfTexturePacks], name);
            numberOfTexturePacks++;
        }
    }
    if (numberOfTexturePacks == 0) {
        texturePackError("none were found");
        exitOverlay(1);
    }
    packEntry* packs = new packEntry[numberOfTexturePacks];
    for (unsigned int i = 0; i < numberOfTexturePacks; i++) {
        uint8_t texturePackHandle = ti_Open(texturePackNames[i], "r");
        packs[i].pack = (texturePack*)ti_GetDataPtr(texturePackHandle);
        packs[i].size = ti_GetSize(texturePackHandle);
        ti_Close(texturePackHandle);
    }
    unsigned int offset = 0;
    unsigned int selectedPack = 0;
    bool quit = false;
    if (numberOfTexturePacks == 1) {
        if (verifyTexturePack(packs[0])) {
            goto texturePackEnd;
        }
        exitOverlay(1);
    }
    // here we make the actual menu
    os_FontSelect(os_LargeFont);
    while (!quit) {
        memset(gfx_vram, 0, 320*240*sizeof(uint16_t));
        os_SetDrawFGColor(65535);
        os_SetDrawBGColor(0);
        fontPrintString("Please select a", 4);
        fontPrintString("texture pack.", 23);
        for (unsigned int i = 0; i + offset < numberOfTexturePacks && i < 5; i++) {
            drawTexturePackSelection(packs[i + offset].pack, i, i == selectedPack);
        }
        uint8_t key;
        bool quit2 = false;
        while (!quit2) {
            while (!(key = os_GetCSC()));
            switch (key) {
                case sk_5:
                case sk_Enter:
                    quit2 = true;
                    if (verifyTexturePack(packs[selectedPack + offset])) {
                        quit = true;
                    }
                    break;
                case sk_Up:
                case sk_8:
                    if (selectedPack > 0) {
                        drawTexturePackSelection(packs[selectedPack + offset].pack, selectedPack, false);
                        selectedPack--;
                        drawTexturePackSelection(packs[selectedPack + offset].pack, selectedPack, true);
                    }
                    break;
                case sk_Down:
                case sk_2:
                    if (selectedPack + offset + 1 < numberOfTexturePacks && selectedPack < 4) {
                        drawTexturePackSelection(packs[selectedPack + offset].pack, selectedPack, false);
                        selectedPack++;
                        drawTexturePackSelection(packs[selectedPack + offset].pack, selectedPack, true);
                    }
                    break;
                case sk_Left:
                case sk_4:
                    if (offset > 4) {
                        offset -= 5;
                        quit2 = true;
                    }
                    break;
                case sk_Right:
                case sk_6:
                    if (numberOfTexturePacks > offset + 5) {
                        offset += 5;
                        if (selectedPack + offset >= numberOfTexturePacks) {
                            selectedPack = numberOfTexturePacks - offset - 1;
                        }
                        quit2 = true;
                    }
                    break;
                case sk_Graph:
                case sk_Clear:
                    exitOverlay(0);
                default:
                    break;
            }
        }
    }
    texturePackEnd:
    texturePackName = new char[strlen(texturePackNames[selectedPack + offset]) + 1];
    strcpy(texturePackName, texturePackNames[selectedPack + offset]);
    for (unsigned int i = 0; i < numberOfTexturePacks; i++) {
        delete[] texturePackNames[i];
    }
    free(texturePackNames);
    delete[] packs;
    return;
}

void drawTexturePackSelection(texturePack* pack, int row, bool selected) {
    if (selected) {
        drawRectangle(0, (40*row)+40, 320, 40, 65535);
        os_SetDrawFGColor(0);
        os_SetDrawBGColor(65535);
    } else {
        drawRectangle(0, (40*row)+40, 320, 40, 0);
        os_SetDrawFGColor(65535);
        os_SetDrawBGColor(0);
    }
    drawImage(4, (40*row)+44, 32, 32, pack->icon);
    char nameString[32];
    strncpy(nameString, pack->metadata, 32);
    nameString[31] = 0;
    os_FontDrawText(nameString, 40, (40*row)+44);
}

bool verifyTexturePack(packEntry pack) {
    os_ClrHomeFull();
    os_PutStrFull("Verifying texture pack, please wait...");
    if (strcmp(pack.pack->magic, TICRAFTTexturePackMagic) != 0) {
        texturePackError("bad magic bytes");
        return false;
    }
    if (pack.pack->version != TICRAFTTexturePackVersion) {
        texturePackError("for the wrong version of TICRAFT");
        return false;
    }
    #define texturePackCRC *((uint32_t*)(((char*)pack.pack) + pack.size - sizeof(uint32_t)))
    #define trueCRC crc32((const char*)pack.pack, pack.size - sizeof(uint32_t))
    if (texturePackCRC != trueCRC) {
        texturePackError("bad checksum");
        return false;
    }
    return true;
}