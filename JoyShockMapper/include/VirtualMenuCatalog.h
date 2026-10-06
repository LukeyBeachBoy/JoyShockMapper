#pragma once
#include "VirtualMenuRouting.h"
#include "Mapping.h"
#include <memory>
#include <mutex>
#include <map>
#include <optional>
#include <array>

enum class VirtualMenuSource { LEFT, RIGHT, LSTICK, RSTICK, DPAD, ABXY, COUNT };
using VirtualMenuOwners = std::array<int, int(VirtualMenuSource::COUNT)>;
struct VirtualMenuDefinition {
  VirtualMenuType type = VirtualMenuType::RADIAL;
  int count = 8, columns = 8;
  float deadzone = .2f;
  std::vector<Mapping> actions;
  std::optional<Mapping> centerAction;
};
struct VirtualMenuAttachment {
  std::string menu;
  VirtualMenuSource source = VirtualMenuSource::RIGHT;
  VirtualMenuActivation activation = VirtualMenuActivation::HOLD;
  VirtualMenuSelection selection = VirtualMenuSelection::ACTIVATION_RELEASE;
  ButtonID input = ButtonID::L, confirm = ButtonID::NONE, cancel = ButtonID::NONE;
  bool joystickCursor = false;
};
struct VirtualMenuCatalog {
  std::map<std::string, VirtualMenuDefinition> definitions;
  std::vector<VirtualMenuAttachment> attachments;
};
namespace VirtualMenus {
  std::shared_ptr<const VirtualMenuCatalog> snapshot();
  void clear();
  void clearScope(const std::string &scope);
  void adaptSinglePad(bool left);
  bool define(std::string_view arguments);
  bool action(std::string_view arguments);
  bool attach(std::string_view arguments);
  bool replace(std::string_view encoded);
}
