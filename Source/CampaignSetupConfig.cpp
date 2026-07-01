#include "CampaignSetupConfig.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "../Geist/Source/Logging.h"
#include <json.hpp>

using json = nlohmann::json;

CampaignSetupScreenConfig g_CampaignSetupScreenConfig;

namespace
{
    constexpr const char* kCampaignSetupConfigPath = "Data/campaign_setup.json";

    int CycleIndex(int value, int delta, int count)
    {
        if (count <= 0)
        {
            return 0;
        }

        value += delta;
        while (value < 0)
        {
            value += count;
        }
        value %= count;
        return value;
    }

    std::string ReplaceToken(std::string text, const std::string& token, const std::string& replacement)
    {
        const size_t position = text.find(token);
        if (position == std::string::npos)
        {
            return text;
        }

        text.replace(position, token.size(), replacement);
        return text;
    }

    bool ParseOptionType(const std::string& typeName, SetupOptionType& outType)
    {
        if (typeName == "enum")
        {
            outType = SetupOptionType::Enum;
            return true;
        }

        if (typeName == "range")
        {
            outType = SetupOptionType::Range;
            return true;
        }

        if (typeName == "action")
        {
            outType = SetupOptionType::Action;
            return true;
        }

        return false;
    }

    bool ParseOption(const json& optionJson, SetupOptionDefinition& outOption)
    {
        if (!optionJson.contains("id") || !optionJson.contains("label") || !optionJson.contains("type"))
        {
            return false;
        }

        outOption = SetupOptionDefinition{};
        outOption.m_Id = optionJson["id"].get<std::string>();
        outOption.m_Label = optionJson["label"].get<std::string>();

        const std::string typeName = optionJson["type"].get<std::string>();
        if (!ParseOptionType(typeName, outOption.m_Type))
        {
            return false;
        }

        if (outOption.m_Type == SetupOptionType::Action)
        {
            return true;
        }

        if (!optionJson.contains("field"))
        {
            return false;
        }

        outOption.m_Field = optionJson["field"].get<std::string>();

        if (outOption.m_Type == SetupOptionType::Range)
        {
            if (!optionJson.contains("min") || !optionJson.contains("max") || !optionJson.contains("default"))
            {
                return false;
            }

            outOption.m_Min = optionJson["min"].get<int>();
            outOption.m_Max = optionJson["max"].get<int>();
            outOption.m_DefaultRangeValue = optionJson["default"].get<int>();
            return outOption.m_Min <= outOption.m_Max;
        }

        if (!optionJson.contains("values") || !optionJson["values"].is_array() || optionJson["values"].empty())
        {
            return false;
        }

        if (optionJson.contains("defaultValue"))
        {
            outOption.m_DefaultEnumValue = optionJson["defaultValue"].get<std::string>();
        }

        if (optionJson.contains("valueFormat"))
        {
            outOption.m_ValueFormat = optionJson["valueFormat"].get<std::string>();
        }

        for (const json& valueJson : optionJson["values"])
        {
            if (!valueJson.contains("id") || !valueJson.contains("label"))
            {
                return false;
            }

            SetupEnumValue value{};
            value.m_Id = valueJson["id"].get<std::string>();
            value.m_Label = valueJson["label"].get<std::string>();
            if (valueJson.contains("regionCount"))
            {
                value.m_RegionCount = valueJson["regionCount"].get<int>();
            }

            outOption.m_Values.push_back(value);
        }

        return !outOption.m_Values.empty();
    }

    bool ParseLayout(const json& layoutJson, SetupOptionLayout& outLayout)
    {
        if (!layoutJson.is_object())
        {
            return false;
        }

        auto readInt = [&](const char* key, int& value)
        {
            if (layoutJson.contains(key))
            {
                value = layoutJson[key].get<int>();
            }
        };

        readInt("rowHeight", outLayout.m_RowHeight);
        readInt("rowStartY", outLayout.m_RowStartY);
        readInt("panelX", outLayout.m_PanelX);
        readInt("panelWidth", outLayout.m_PanelWidth);
        readInt("labelX", outLayout.m_LabelX);
        readInt("valueX", outLayout.m_ValueX);
        readInt("arrowWidth", outLayout.m_ArrowWidth);
        readInt("startButtonHeight", outLayout.m_StartButtonHeight);
        readInt("startButtonX", outLayout.m_StartButtonX);
        readInt("startButtonWidth", outLayout.m_StartButtonWidth);
        return true;
    }
}

void CampaignSetupScreenConfig::LoadDefaults()
{
    if (LoadFromFile(kCampaignSetupConfigPath))
    {
        return;
    }

    Log("CampaignSetupScreenConfig::LoadDefaults - using built-in fallback definitions");
    m_Title = "New Campaign";
    m_HelpText = "Arrows: select/adjust   Enter: start   Esc: title";
    m_Layout = SetupOptionLayout{};
    m_Options.clear();
    m_StartOptionIndex = -1;

    m_Options.push_back(SetupOptionDefinition{
            "difficulty", "Difficulty", "difficulty", SetupOptionType::Enum,
            {
                { "squire", "Squire", 0 },
                { "baron", "Baron", 0 },
                { "viscount", "Viscount", 0 },
                { "marquis", "Marquis", 0 },
                { "king", "King", 0 }
            },
            0, 0, 0, "baron", ""
        });
        m_Options.push_back(SetupOptionDefinition{
            "opponents", "Opponents", "enemyCount", SetupOptionType::Range,
            {}, 3, 7, 4, "", ""
        });
        m_Options.push_back(SetupOptionDefinition{
            "mapSize", "Map Size", "mapSize", SetupOptionType::Enum,
            {
                { "small", "Small", 24 },
                { "medium", "Medium", 35 },
                { "large", "Large", 64 },
                { "huge", "Huge", 80 }
            },
            0, 0, 0, "medium", "{label} ({regionCount})"
        });
        m_Options.push_back(SetupOptionDefinition{
            "battleMode", "Battles", "battleMode", SetupOptionType::Enum,
            {
                { "automatic", "Automatic", 0 },
                { "onMap", "On Map", 0 }
            },
            0, 0, 0, "onMap", ""
        });
        m_Options.push_back(SetupOptionDefinition{
            "resourceDistribution", "Resources", "resourceDistribution", SetupOptionType::Enum,
            {
                { "clumped", "Clumped", 0 },
                { "random", "Random", 0 },
                { "balanced", "Balanced", 0 }
            },
            0, 0, 0, "balanced", ""
        });
        m_Options.push_back(SetupOptionDefinition{
            "start", "Start Game", "", SetupOptionType::Action,
            {}, 0, 0, 0, "", ""
        });
    m_StartOptionIndex = static_cast<int>(m_Options.size()) - 1;
}

bool CampaignSetupScreenConfig::LoadFromFile(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream)
    {
        Log("CampaignSetupScreenConfig::LoadFromFile - failed to open " + path);
        return false;
    }

    json root;
    try
    {
        stream >> root;
    }
    catch (const std::exception& exception)
    {
        Log(std::string("CampaignSetupScreenConfig::LoadFromFile - parse error: ") + exception.what());
        return false;
    }

    if (!root.contains("options") || !root["options"].is_array() || root["options"].empty())
    {
        Log("CampaignSetupScreenConfig::LoadFromFile - missing options array in " + path);
        return false;
    }

    CampaignSetupScreenConfig parsed{};
    parsed.m_Title = root.value("title", "New Campaign");
    parsed.m_HelpText = root.value("helpText", "Arrows: select/adjust   Enter: start   Esc: title");

    if (root.contains("layout"))
    {
        ParseLayout(root["layout"], parsed.m_Layout);
    }

    for (const json& optionJson : root["options"])
    {
        SetupOptionDefinition option{};
        if (!ParseOption(optionJson, option))
        {
            Log("CampaignSetupScreenConfig::LoadFromFile - invalid option in " + path);
            return false;
        }

        if (option.m_Type == SetupOptionType::Action)
        {
            parsed.m_StartOptionIndex = static_cast<int>(parsed.m_Options.size());
        }

        parsed.m_Options.push_back(option);
    }

    if (parsed.m_StartOptionIndex < 0)
    {
        Log("CampaignSetupScreenConfig::LoadFromFile - missing action option in " + path);
        return false;
    }

    *this = std::move(parsed);
    Log("CampaignSetupScreenConfig::LoadFromFile - loaded " + path);
    return true;
}

const SetupOptionDefinition& CampaignSetupScreenConfig::GetOption(int optionIndex) const
{
    static const SetupOptionDefinition kEmptyOption{};
    if (optionIndex < 0 || optionIndex >= static_cast<int>(m_Options.size()))
    {
        return kEmptyOption;
    }

    return m_Options[static_cast<size_t>(optionIndex)];
}

bool CampaignSetupScreenConfig::IsActionOption(int optionIndex) const
{
    return optionIndex == m_StartOptionIndex;
}

int CampaignSetupScreenConfig::FindEnumIndexById(const SetupOptionDefinition& option,
    const std::string& valueId) const
{
    for (int index = 0; index < static_cast<int>(option.m_Values.size()); ++index)
    {
        if (option.m_Values[static_cast<size_t>(index)].m_Id == valueId)
        {
            return index;
        }
    }

    return 0;
}

int CampaignSetupScreenConfig::GetEnumIndex(const SetupOptionDefinition& option,
    const CampaignSetup& setup) const
{
    if (option.m_Field == "difficulty")
    {
        return static_cast<int>(setup.m_Difficulty);
    }

    if (option.m_Field == "mapSize")
    {
        return static_cast<int>(setup.m_MapSize);
    }

    if (option.m_Field == "battleMode")
    {
        return static_cast<int>(setup.m_BattleMode);
    }

    if (option.m_Field == "resourceDistribution")
    {
        return static_cast<int>(setup.m_ResourceDistribution);
    }

    return 0;
}

void CampaignSetupScreenConfig::SetEnumIndex(const SetupOptionDefinition& option, int index,
    CampaignSetup& setup) const
{
    index = std::clamp(index, 0, static_cast<int>(option.m_Values.size()) - 1);

    if (option.m_Field == "difficulty")
    {
        setup.m_Difficulty = static_cast<Difficulty>(index);
        return;
    }

    if (option.m_Field == "mapSize")
    {
        setup.m_MapSize = static_cast<MapSize>(index);
        return;
    }

    if (option.m_Field == "battleMode")
    {
        setup.m_BattleMode = static_cast<BattleMode>(index);
        return;
    }

    if (option.m_Field == "resourceDistribution")
    {
        setup.m_ResourceDistribution = static_cast<ResourceDistribution>(index);
    }
}

std::string CampaignSetupScreenConfig::FormatEnumValueLabel(const SetupOptionDefinition& option,
    int valueIndex) const
{
    if (valueIndex < 0 || valueIndex >= static_cast<int>(option.m_Values.size()))
    {
        return "";
    }

    const SetupEnumValue& value = option.m_Values[static_cast<size_t>(valueIndex)];
    if (option.m_ValueFormat.empty())
    {
        return value.m_Label;
    }

    std::string formatted = option.m_ValueFormat;
    formatted = ReplaceToken(formatted, "{label}", value.m_Label);
    formatted = ReplaceToken(formatted, "{regionCount}", std::to_string(value.m_RegionCount));
    formatted = ReplaceToken(formatted, "{id}", value.m_Id);
    return formatted;
}

void CampaignSetupScreenConfig::ApplyDefaults(CampaignSetup& setup) const
{
    setup = CampaignSetup{};

    for (const SetupOptionDefinition& option : m_Options)
    {
        if (option.m_Type == SetupOptionType::Range)
        {
            if (option.m_Field == "enemyCount")
            {
                setup.m_EnemyCount = std::clamp(option.m_DefaultRangeValue, option.m_Min, option.m_Max);
            }

            continue;
        }

        if (option.m_Type != SetupOptionType::Enum)
        {
            continue;
        }

        const int defaultIndex = FindEnumIndexById(option, option.m_DefaultEnumValue);
        SetEnumIndex(option, defaultIndex, setup);
    }

    ClampCampaignSetup(setup);
}

std::string CampaignSetupScreenConfig::GetValueLabel(int optionIndex, const CampaignSetup& setup) const
{
    const SetupOptionDefinition& option = GetOption(optionIndex);
    if (option.m_Type == SetupOptionType::Action)
    {
        return "";
    }

    if (option.m_Type == SetupOptionType::Range)
    {
        if (option.m_Field == "enemyCount")
        {
            return std::to_string(setup.m_EnemyCount);
        }

        return "";
    }

    return FormatEnumValueLabel(option, GetEnumIndex(option, setup));
}

void CampaignSetupScreenConfig::AdjustOption(int optionIndex, int delta, CampaignSetup& setup) const
{
    const SetupOptionDefinition& option = GetOption(optionIndex);
    if (option.m_Type == SetupOptionType::Action)
    {
        return;
    }

    if (option.m_Type == SetupOptionType::Range)
    {
        if (option.m_Field == "enemyCount")
        {
            setup.m_EnemyCount = std::clamp(setup.m_EnemyCount + delta, option.m_Min, option.m_Max);
        }

        return;
    }

    const int nextIndex = CycleIndex(GetEnumIndex(option, setup), delta,
        static_cast<int>(option.m_Values.size()));
    SetEnumIndex(option, nextIndex, setup);
}