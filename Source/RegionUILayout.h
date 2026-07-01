#ifndef _REGIONUILAYOUT_H_
#define _REGIONUILAYOUT_H_

#include "raylib.h"

#include <string>

constexpr int kRegionSidePanelWidth = 120;
constexpr int kRegionMinimapPixelSize = 80;
constexpr int kRegionMinimapMargin = 8;

Rectangle GetRegionWorldViewBounds();
Rectangle GetRegionSidePanelBounds();
Rectangle GetRegionMinimapBounds();
Rectangle GetRegionSidePanelTextBounds();

int GetRegionWorldViewWidth();
int GetRegionWorldViewHeight();

bool IsPointInRegionWorldView(Vector2 point);
bool IsPointInRegionSidePanel(Vector2 point);

void DrawRegionSidePanelBackground();

float DrawRegionSidePanelOutlinedParagraph(const std::string& text, float y, float fontSize, Color color);
float DrawRegionSidePanelOutlinedLine(const std::string& text, float y, Color color);

#endif