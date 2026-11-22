/*******************************************************************************************
*
*   raylib [core] example - input gamepad
*
*   Example complexity rating: [*] 1/4
*
*   Ported for this tree from examples/core/core_input_gamepad.c (raylib master).
*   Adjustments: 480x272; one built-in pad; no keyboard, mouse or vibration; the
*   800x450 pad art is replaced by a primitive layout. Input uses only the public
*   raylib gamepad API, so this file has no platform headers or conditionals.
*
*   Example originally created with raylib 1.1, last time updated with raylib 4.2
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2013-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#ifndef FRAMES
    #define FRAMES 1800
#endif

void sample_core_input_gamepad(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;
    SetTargetFPS(60);

    int lastPressed = 0;

    for (int frames = 0; frames < FRAMES; frames++)
    {
        int gamepad = 0;
        int pressed = GetGamepadButtonPressed();
        if ((pressed != 0) && (pressed != lastPressed)) TraceLog(LOG_INFO, "PAD pressed=%d frame=%d", pressed, frames);
        lastPressed = pressed;

        float leftStickX = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
        float leftStickY = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);
        float leftTrigger = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_TRIGGER);
        float rightTrigger = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER);

        BeginDrawing();

            ClearBackground(RAYWHITE);

            if (IsGamepadAvailable(gamepad))
            {
                DrawText(TextFormat("GP%d: %s", gamepad, GetGamepadName(gamepad)), 10, 10, 10, BLACK);
                DrawText(TextFormat("AXES %d", GetGamepadAxisCount(gamepad)), 10, 24, 10, MAROON);
                DrawText(TextFormat("LT %.2f  RT %.2f", leftTrigger, rightTrigger), 250, 24, 10, MAROON);

                for (int i = 0; i < GetGamepadAxisCount(gamepad); i++)
                {
                    DrawText(TextFormat("AXIS %i: %.02f", i, GetGamepadAxisMovement(gamepad, i)),
                        20, 38 + 12*i, 10, DARKGRAY);
                }

                // Controller body
                DrawRectangleRounded((Rectangle){ 15, 65, 450, 180 }, 0.15f, 12, LIGHTGRAY);

                // D-pad
                DrawRectangle(60, 120, 28, 40, IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP)? RED : BLACK);
                DrawRectangle(60, 170, 28, 40, IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN)? RED : BLACK);
                DrawRectangle(45, 135, 40, 28, IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT)? RED : BLACK);
                DrawRectangle(95, 135, 40, 28, IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)? RED : BLACK);
                DrawRectangle(60, 135, 28, 28, BLACK);

                // Left stick
                DrawCircle(200, 150, 40, BLACK);
                DrawCircle(200, 150, 34, LIGHTGRAY);
                DrawCircle(200 + (int)(leftStickX*22), 150 + (int)(leftStickY*22), 18, BLACK);
                DrawText("NUB", 185, 195, 10, DARKGRAY);

                // Face buttons (positional enum: up/right/down/left)
                DrawCircle(370, 115, 14, IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP)? GREEN : DARKGRAY);
                DrawCircle(410, 155, 14, IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)? RED : DARKGRAY);
                DrawCircle(370, 195, 14, IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)? BLUE : DARKGRAY);
                DrawCircle(330, 155, 14, IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)? PINK : DARKGRAY);
                DrawText("TRI", 355, 96, 10, DARKGRAY);
                DrawText("CIR", 425, 150, 10, DARKGRAY);
                DrawText("CRO", 355, 212, 10, DARKGRAY);
                DrawText("SQU", 300, 150, 10, DARKGRAY);

                // Shoulders
                DrawRectangleRounded((Rectangle){ 45, 80, 90, 16 }, 0.5f, 8,
                    IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1)? RED : DARKGRAY);
                DrawRectangleRounded((Rectangle){ 345, 80, 90, 16 }, 0.5f, 8,
                    IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)? RED : DARKGRAY);
                DrawText("L", 85, 82, 10, RAYWHITE);
                DrawText("R", 385, 82, 10, RAYWHITE);

                // Select / Start
                DrawRectangle(170, 230, 50, 14,
                    IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT)? RED : DARKGRAY);
                DrawRectangle(250, 230, 50, 14,
                    IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT)? RED : DARKGRAY);
                DrawText("SELECT", 174, 232, 10, RAYWHITE);
                DrawText("START", 254, 232, 10, RAYWHITE);

                if (pressed != 0) DrawText(TextFormat("DETECTED BUTTON: %i", pressed), 10, 255, 10, RED);
                else DrawText("DETECTED BUTTON: NONE", 10, 255, 10, GRAY);
            }
            else
            {
                DrawText(TextFormat("GP%d: NOT DETECTED", gamepad), 10, 10, 10, GRAY);
                DrawText("Connect a gamepad and restart", 10, 40, 20, DARKGRAY);
            }

        EndDrawing();

        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
    }
    // Stay resident without drawing: the host captures after DONE and resets.
    for (;;) WaitTime(0.1);

    return;
}
