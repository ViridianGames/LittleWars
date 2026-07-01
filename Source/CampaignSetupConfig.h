#ifndef _CAMPAIGNSETUPCONFIG_H_
#define _CAMPAIGNSETUPCONFIG_H_

#include "GameGlobals.h"

#include <string>
#include <vector>

enum class SetupOptionType : int
{
    Enum = 0,
    Range,
    Action
};

struct SetupEnumValue
{
    std::string m_Id;
    std::string m_Label;
    int m_RegionCount = 0;
};

struct SetupOptionLayout
{
    int m_RowHeight = 18;
    int m_RowStartY = 44;
    int m_PanelX = 24;
    int m_PanelWidth = 220;
    int m_LabelX = 32;
    int m_ValueX = 150;
    int m_ArrowWidth = 14;
    int m_StartButtonHeight = 20;
    int m_StartButtonX = 96;
    int m_StartButtonWidth = 96;
};

struct SetupOptionDefinition
{
    std::string m_Id;
    std::string m_Label;
    std::string m_Field;
    SetupOptionType m_Type = SetupOptionType::Enum;
    std::vector<SetupEnumValue> m_Values;
    int m_Min = 0;
    int m_Max = 0;
    int m_DefaultRangeValue = 0;
    std::string m_DefaultEnumValue;
    std::string m_ValueFormat;
};

class CampaignSetupScreenConfig
{
public:
    bool LoadFromFile(const std::string& path);
    void LoadDefaults();

    const std::string& GetTitle() const { return m_Title; }
    const std::string& GetHelpText() const { return m_HelpText; }
    const SetupOptionLayout& GetLayout() const { return m_Layout; }
    int GetOptionCount() const { return static_cast<int>(m_Options.size()); }
    const SetupOptionDefinition& GetOption(int optionIndex) const;
    int GetStartOptionIndex() const { return m_StartOptionIndex; }

    void ApplyDefaults(CampaignSetup& setup) const;
    std::string GetValueLabel(int optionIndex, const CampaignSetup& setup) const;
    void AdjustOption(int optionIndex, int delta, CampaignSetup& setup) const;
    bool IsActionOption(int optionIndex) const;

private:
    std::string m_Title;
    std::string m_HelpText;
    SetupOptionLayout m_Layout;
    std::vector<SetupOptionDefinition> m_Options;
    int m_StartOptionIndex = -1;

    int GetEnumIndex(const SetupOptionDefinition& option, const CampaignSetup& setup) const;
    void SetEnumIndex(const SetupOptionDefinition& option, int index, CampaignSetup& setup) const;
    int FindEnumIndexById(const SetupOptionDefinition& option, const std::string& valueId) const;
    std::string FormatEnumValueLabel(const SetupOptionDefinition& option, int valueIndex) const;
};

extern CampaignSetupScreenConfig g_CampaignSetupScreenConfig;

#endif