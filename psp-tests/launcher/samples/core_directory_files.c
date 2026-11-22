/*******************************************************************************************
*
*   raylib [core] example - directory files
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.6
*
*   Example contributed by Hugo ARNAL (@hugoarnal) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Hugo ARNAL (@hugoarnal)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define PSP_INPUT_SCREEN_WIDTH 480
#define PSP_INPUT_SCREEN_HEIGHT 272
#include "psp_input.h"
#include "raygui.h"                 // Required for GUI controls

#define MAX_FILEPATH_SIZE       1024
#define FILE_FILTER             "DIRS*;.png;.c"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_core_directory_files(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    char directory[MAX_FILEPATH_SIZE] = { 0 };
    strcpy(directory, GetWorkingDirectory());

    // Load file-paths on current working directory
    // NOTE: LoadDirectoryFiles() loads files and directories by default,
    // use LoadDirectoryFilesEx() for custom filters and recursive directories loading
    //FilePathList files = LoadDirectoryFiles(directory);
    FilePathList files = LoadDirectoryFilesEx(directory, FILE_FILTER, false);

    int btnBackPressed = false;

    int listScrollIndex = 0;
    int listItemActive = -1;
    int listItemFocused = -1;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frame++)
    {
        pspFrame = frame;
        // Update
        //----------------------------------------------------------------------------------
        if (btnBackPressed)
        {
            TextCopy(directory, GetPrevDirectoryPath(directory));
            UnloadDirectoryFiles(files);
            files = LoadDirectoryFilesEx(directory, FILE_FILTER, false);

            listScrollIndex = 0;
            listItemActive = -1;
            listItemFocused = -1;
        }

        if ((listItemActive >= 0) && (listItemActive < (int)files.count) && DirectoryExists(files.paths[listItemActive]))
        {
            TextCopy(directory, files.paths[listItemActive]);
            UnloadDirectoryFiles(files);
            files = LoadDirectoryFilesEx(directory, FILE_FILTER, false);

            listScrollIndex = 0;
            listItemActive = -1;
            listItemFocused = -1;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);

            btnBackPressed = GuiButton((Rectangle){ 40.0f, 10.0f, 48, 28 }, "<");

            GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize*2);
            GuiLabel((Rectangle){ 40 + 48 + 10, 10, 700, 28 }, directory);
            GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize);

            GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
            GuiSetStyle(LISTVIEW, TEXT_PADDING, 40);
            GuiListViewEx((Rectangle){ 0, 50, (float)GetScreenWidth(), (float)GetScreenHeight() - 50 },
                files.paths, files.count, &listScrollIndex, &listItemActive, &listItemFocused);

            DrawText("core_directory_files", 8, 8, 10, DARKGRAY);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadDirectoryFiles(files);
    //--------------------------------------------------------------------------------------

    return;
}
