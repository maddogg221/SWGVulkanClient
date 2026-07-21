#include "clientcommon/ObjControllerHandlers.h"

#include "swgproto/Animation.h"
#include "swgproto/Buffs.h"
#include "swgproto/CommandQueueAdd.h"
#include "swgproto/CombatSpam.h"
#include "swgproto/CommandQueueRemove.h"
#include "swgproto/DataTransform.h"
#include "swgproto/DataTransformWithParent.h"
#include "swgproto/Emote.h"
#include "swgproto/Flourish.h"
#include "swgproto/HarvesterResourceDataMessage.h"
#include "swgproto/ObjectMenuResponse.h"
#include "swgproto/PostureMessage.h"
#include "swgproto/ShowFlyText.h"
#include "swgproto/SpatialChat.h"
#include "swgproto/StartingLocationListMessage.h"
#include "swgproto/WeaponRanges.h"

namespace clientcommon {

namespace {

// ASCII-only preview, matching the same simplification dummyclient's own
// toUtf8Preview() uses - good enough for logging typed chat text.
std::string toUtf8Preview(const std::u16string& s) {
    std::string out;
    out.reserve(s.size());
    for (char16_t ch : s) {
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

} // namespace

void registerObjControllerHandlers(swgproto::ObjControllerDispatcher& dispatcher,
                                    std::ostream& out, const std::string& prefix,
                                    ObjControllerHook onDecoded) {
    dispatcher.on(swgproto::kAnimationControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto anim = swgproto::Animation::parse(buf);
                      out << prefix << "Animation: objectId=" << envelope.objectId << " anim=\""
                          << anim.animationName << "\"\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kPostureMessageControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto posture = swgproto::PostureMessage::parse(buf);
                      out << prefix << "PostureMessage: objectId=" << envelope.objectId
                          << " posture=" << static_cast<int>(posture.posture)
                          << " unknownFlag=" << static_cast<int>(posture.unknownFlag) << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kDataTransformControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto dt = swgproto::DataTransform::parse(buf);
                      out << prefix << "DataTransform (idle sync): objectId=" << envelope.objectId
                          << " counter=" << dt.counter << " pos=(" << dt.x << ", " << dt.y << ", "
                          << dt.z << ") speed=" << dt.speed << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kDataTransformWithParentControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto dtp = swgproto::DataTransformWithParent::parse(buf);
                      out << prefix
                          << "DataTransformWithParent (idle sync): objectId=" << envelope.objectId
                          << " parentId=" << dtp.parentId << " counter=" << dtp.counter
                          << " pos=(" << dtp.x << ", " << dtp.y << ", " << dtp.z
                          << ") speed=" << dtp.speed << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kShowFlyTextControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto flyText = swgproto::ShowFlyText::parse(buf);
                      out << prefix << "ShowFlyText: objectId=" << envelope.objectId
                          << " targetObjectId=" << flyText.targetObjectId << " \""
                          << flyText.file << "/" << flyText.entry << "\" scale=" << flyText.scale
                          << " rgb=(" << static_cast<int>(flyText.red) << ", "
                          << static_cast<int>(flyText.green) << ", "
                          << static_cast<int>(flyText.blue)
                          << ") flags=" << static_cast<int>(flyText.flags) << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kCombatSpamControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto spam = swgproto::CombatSpam::parse(buf);
                      out << prefix << "CombatSpam: objectId=" << envelope.objectId
                          << " attackerId=" << spam.attackerId
                          << " defenderId=" << spam.defenderId
                          << " itemId=" << spam.itemId << " damage=" << spam.damage
                          << " file=\"" << spam.file << "\" stringName=\""
                          << spam.stringName << "\" color="
                          << static_cast<int>(spam.color) << " message=\""
                          << toUtf8Preview(spam.message) << "\"\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kHarvesterResourceDataMessageControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto msg = swgproto::HarvesterResourceDataMessage::parse(buf);
                      out << prefix << "HarvesterResourceDataMessage: requestingPlayerId="
                          << envelope.objectId << " harvesterObjectId=" << msg.harvesterObjectId
                          << " resources=" << msg.resources.size() << "\n";
                      for (const auto& entry : msg.resources) {
                          out << prefix << "  resourceSpawnId=" << entry.resourceSpawnId
                              << " name=\"" << entry.name << "\" resourceType=\""
                              << entry.resourceType << "\" densityPercent="
                              << static_cast<int>(entry.densityPercent) << "\n";
                      }
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kStartingLocationListMessageControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto msg = swgproto::StartingLocationListMessage::parse(buf);
                      out << prefix << "StartingLocationListMessage: objectId="
                          << envelope.objectId << " locations=" << msg.locations.size()
                          << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kAddBuffMessageControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto msg = swgproto::AddBuffMessage::parse(buf);
                      out << prefix << "AddBuffMessage: objectId=" << envelope.objectId
                          << " buffCrc=0x" << std::hex << msg.buffCrc << std::dec
                          << " duration=" << msg.duration << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kRemoveBuffMessageControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto msg = swgproto::RemoveBuffMessage::parse(buf);
                      out << prefix << "RemoveBuffMessage: objectId=" << envelope.objectId
                          << " buffCrc=0x" << std::hex << msg.buffCrc << std::dec << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kFlourishControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto flourish = swgproto::Flourish::parse(buf);
                      out << prefix << "Flourish: objectId=" << envelope.objectId
                          << " flourishId=" << flourish.flourishId << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kEmoteControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto emote = swgproto::Emote::parse(buf);
                      out << prefix << "Emote: objectId=" << envelope.objectId
                          << " senderId=" << emote.senderId
                          << " emoteTargetId=" << emote.emoteTargetId
                          << " emoteId=" << emote.emoteId << " animTextFlags="
                          << static_cast<int>(emote.animTextFlags) << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kCommandQueueAddControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto cqa = swgproto::CommandQueueAdd::parse(buf);
                      out << prefix << "CommandQueueAdd: objectId=" << envelope.objectId
                          << " actionCount=" << cqa.actionCount
                          << " actionCrc=0x" << std::hex << cqa.actionCrc << std::dec << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kObjectMenuResponseControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto omr = swgproto::ObjectMenuResponse::parse(buf);
                      out << prefix << "ObjectMenuResponse: objectId=" << envelope.objectId
                          << " target=" << omr.target << " player=" << omr.player
                          << " items=" << omr.items.size() << " count=" << static_cast<int>(omr.count)
                          << "\n";
                      for (const auto& item : omr.items) {
                          out << prefix << "  itemIndex=" << static_cast<int>(item.itemIndex)
                              << " parentIndex=" << static_cast<int>(item.parentIndex)
                              << " radialId=" << static_cast<int>(item.radialId)
                              << " callback=" << static_cast<int>(item.callback) << " text=\""
                              << toUtf8Preview(item.text) << "\"\n";
                      }
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kCommandQueueRemoveControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto cqr = swgproto::CommandQueueRemove::parse(buf);
                      out << prefix << "CommandQueueRemove: objectId=" << envelope.objectId
                          << " actionCount=" << cqr.actionCount << " timer=" << cqr.timer
                          << " tab1=" << cqr.tab1 << " tab2=" << cqr.tab2 << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kWeaponRangesControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto wr = swgproto::WeaponRanges::parse(buf);
                      out << prefix << "WeaponRanges: objectId=" << envelope.objectId
                          << " weaponObjectId=" << wr.weaponObjectId
                          << " idealRange=" << wr.idealRange << " maxRange=" << wr.maxRange
                          << " pointBlankAccuracy=" << wr.pointBlankAccuracy
                          << " idealAccuracy=" << wr.idealAccuracy
                          << " maxRangeAccuracy=" << wr.maxRangeAccuracy << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });

    dispatcher.on(swgproto::kSpatialChatControllerType,
                  [&out, prefix, onDecoded](const swgproto::ObjControllerMessage& envelope,
                                             soe::PacketBuffer& buf) {
                      auto chat = swgproto::SpatialChat::parse(buf);
                      out << prefix << "SpatialChat: objectId=" << envelope.objectId
                          << " senderId=" << chat.senderId
                          << " chatTargetId=" << chat.chatTargetId << " message=\""
                          << toUtf8Preview(chat.message) << "\" volume=" << chat.volume
                          << " spatialChatType=" << chat.spatialChatType
                          << " moodType=" << chat.moodType
                          << " chatFlags=" << static_cast<int>(chat.chatFlags)
                          << " languageId=" << static_cast<int>(chat.languageId) << "\n";
                      if (onDecoded) onDecoded(envelope);
                  });
}

} // namespace clientcommon
