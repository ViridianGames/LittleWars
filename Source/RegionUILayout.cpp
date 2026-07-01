#include "RegionUILayout.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "GameGlobals.h"

namespace
{
    constexpr float kParagraphLineSpacing = 1.2f;

    std::vector<std::string> WrapParagraphLines(const std::shared_ptr<Font>& font, const std::string& text,
        float maxWidth, float fontSize, int spacing)
    {
        std::vector<std::string> lines;
        if (!font)
        {
            return lines;
        }

        std::istringstream stream(text);
        std::string rawLine;
        while (std::getline(stream, rawLine))
        {
            std::stringstream lineStream(rawLine);
            std::string word;
            std::string line;
            while (lineStream >> word)
            {
                const std::string candidate = line.empty() ? word : (line + " " + word);
                const float candidateWidth = MeasureTextEx(*font, candidate.c_str(), fontSize, spacing).x;
                if (candidateWidth > maxWidth && !line.empty())
                {
                    lines.push_back(line);
                    line = word + " ";
                }
                else
                {
                    line = candidate + " ";
                }
            }

            if (!line.empty())
            {
                lines.push_back(line);
            }
        }

        return lines;
    }

    float GetParagraphHeight(const std::vector<std::string>& lines, float fontSize)
    {
        if (lines.empty())
        {
            return 0.0f;
        }

        return static_cast<float>(lines.size()) * fontSize * kParagraphLineSpacing;
    }
}

Rectangle GetRegionWorldViewBounds()
{
    const float renderWidth = g_Engine->m_RenderWidth;
    const float renderHeight = g_Engine->m_RenderHeight;
    const float worldViewWidth = std::max(1.0f, renderWidth - static_cast<float>(kRegionSidePanelWidth));

    return Rectangle{
        0.0f,
        0.0f,
        worldViewWidth,
        renderHeight
    };
}

Rectangle GetRegionSidePanelBounds()
{
    const Rectangle worldView = GetRegionWorldViewBounds();
    return Rectangle{
        worldView.width,
        0.0f,
        static_cast<float>(kRegionSidePanelWidth),
        worldView.height
    };
}

Rectangle GetRegionMinimapBounds()
{
    const Rectangle panel = GetRegionSidePanelBounds();
    const float minimapX = panel.x + (panel.width - static_cast<float>(kRegionMinimapPixelSize)) * 0.5f;
    const float minimapY = static_cast<float>(kRegionMinimapMargin);

    return Rectangle{
        minimapX,
        minimapY,
        static_cast<float>(kRegionMinimapPixelSize),
        static_cast<float>(kRegionMinimapPixelSize)
    };
}

Rectangle GetRegionSidePanelTextBounds()
{
    const Rectangle panel = GetRegionSidePanelBounds();
    const Rectangle minimap = GetRegionMinimapBounds();
    constexpr float kTextMargin = 4.0f;
    constexpr float kTextGapBelowMinimap = 8.0f;

    const float textX = panel.x + kTextMargin;
    const float textY = minimap.y + minimap.height + kTextGapBelowMinimap;
    return Rectangle{
        textX,
        textY,
        panel.width - (kTextMargin * 2.0f),
        panel.height - (textY - panel.y) - kTextMargin
    };
}

int GetRegionWorldViewWidth()
{
    return static_cast<int>(GetRegionWorldViewBounds().width);
}

int GetRegionWorldViewHeight()
{
    return static_cast<int>(GetRegionWorldViewBounds().height);
}

bool IsPointInRegionWorldView(Vector2 point)
{
    return CheckCollisionPointRec(point, GetRegionWorldViewBounds());
}

bool IsPointInRegionSidePanel(Vector2 point)
{
    return CheckCollisionPointRec(point, GetRegionSidePanelBounds());
}

void DrawRegionSidePanelBackground()
{
    const Rectangle panel = GetRegionSidePanelBounds();
    DrawRectangleRec(panel, Color{ 22, 24, 30, 255 });
    DrawLineEx(
        Vector2{ panel.x, panel.y },
        Vector2{ panel.x, panel.y + panel.height },
        1.0f,
        Color{ 48, 52, 62, 255 });
}

float DrawRegionSidePanelOutlinedParagraph(const std::string& text, float y, float fontSize, Color color)
{
    const Rectangle textBounds = GetRegionSidePanelTextBounds();
    const std::vector<std::string> lines = WrapParagraphLines(g_smallFont, text, textBounds.width, fontSize, 1);
    DrawParagraph(g_smallFont, text, Vector2{ textBounds.x, y }, textBounds.width, fontSize, 1, color, true);
    return y + GetParagraphHeight(lines, fontSize);
}

float DrawRegionSidePanelOutlinedLine(const std::string& text, float y, Color color)
{
    const Rectangle textBounds = GetRegionSidePanelTextBounds();
    DrawOutlinedText(g_smallFont, text, Vector2{ textBounds.x, y }, g_smallFontDrawSize, 1, color);
    return y + g_smallFontDrawSize + 2.0f;
}