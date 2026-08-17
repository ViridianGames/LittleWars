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

// Solid panel text — no outline (pinch reads better plain on dark fill).
float DrawRegionSidePanelParagraph(const std::string& text, float y, float fontSize, Color color);
float DrawRegionSidePanelLine(const std::string& text, float y, Color color);

// Back-compat aliases (same plain draw path).
inline float DrawRegionSidePanelOutlinedParagraph(const std::string& text, float y, float fontSize, Color color)
{
    return DrawRegionSidePanelParagraph(text, y, fontSize, color);
}
inline float DrawRegionSidePanelOutlinedLine(const std::string& text, float y, Color color)
{
    return DrawRegionSidePanelLine(text, y, color);
}

#endif