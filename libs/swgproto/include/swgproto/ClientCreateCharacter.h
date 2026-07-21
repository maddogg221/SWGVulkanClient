#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swgproto {

// Parameters for a headless character creation request. Defaults mirror
// Core3's own "simple constructor for backward compatibility" - a known
// reasonable/working combination (a human, no customization, crafting
// artisan profession, tutorial skipped) rather than an arbitrary guess.
struct ClientCreateCharacterParams {
    std::u16string characterName;
    std::string templateName = "object/creature/player/human_male.iff"; // race template
    float scaleFactor = 1.0f; // height
    std::string customAppearanceData;
    std::string hairTemplateName;
    std::string hairAppearanceData;
    std::string profession = "crafting_artisan";
    std::u16string biography;
    bool useNewbieTutorial = false;
};

// Builds the opCount(2)+hash(4)+fields payload for ClientCreateCharacter,
// ready to hand to SoeSession::sendMessage(). Wire layout, in this exact
// order (verified against Core3's own server-side parse(), not just its
// constructor - see DISCOVERY.txt): ASCII customAppearanceData + Unicode
// characterName + ASCII templateName + ASCII "" (starting location,
// ignored by server) + ASCII hairTemplateName + ASCII hairAppearanceData +
// ASCII profession + byte 0x00 (unused) + float scaleFactor + Unicode
// biography + byte tutorialFlag.
std::vector<uint8_t> buildClientCreateCharacter(const ClientCreateCharacterParams& params);

} // namespace swgproto
