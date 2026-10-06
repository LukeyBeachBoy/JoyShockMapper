#include "VirtualMenuCatalog.h"
#include <sstream>
#include <regex>
#include <atomic>
#include "ControllerContext.h"

namespace VirtualMenus {
namespace {
  std::mutex writer;
  std::atomic<std::shared_ptr<const VirtualMenuCatalog>> current{ std::make_shared<VirtualMenuCatalog>() };
  std::map<std::string, std::shared_ptr<const VirtualMenuCatalog>> scoped;
  bool identifier(const std::string &id) { return std::regex_match(id, std::regex("[a-zA-Z][a-zA-Z0-9_-]{0,63}")); }
  bool end(std::istream &stream) { stream >> std::ws; return stream.eof() || stream.peek() == '#'; }
  void publish(std::shared_ptr<VirtualMenuCatalog> next) {
    if (!ControllerContext::writeScope.empty()) { scoped[ControllerContext::writeScope] = next; return; }
    current.store(std::shared_ptr<const VirtualMenuCatalog>(std::move(next)));
  }
}
std::shared_ptr<const VirtualMenuCatalog> snapshot() {
  for (const auto &scope : ControllerContext::readScopes()) { auto it = scoped.find(scope); if (it != scoped.end()) return it->second; }
  return current.load();
}
void clear() { std::lock_guard<std::mutex> lock(writer); if (ControllerContext::writeScope.empty()) scoped.clear(); publish(std::make_shared<VirtualMenuCatalog>()); }
void clearScope(const std::string &scope) { std::lock_guard<std::mutex> lock(writer); scoped.erase(scope); }
void adaptSinglePad(bool left) {
  auto next = std::make_shared<VirtualMenuCatalog>(*snapshot());
  const auto chosen = left ? VirtualMenuSource::LEFT : VirtualMenuSource::RIGHT;
  next->attachments.erase(std::remove_if(next->attachments.begin(), next->attachments.end(), [chosen](const auto &attachment) {
    return (attachment.source == VirtualMenuSource::LEFT || attachment.source == VirtualMenuSource::RIGHT) && attachment.source != chosen;
  }), next->attachments.end());
  for (auto &attachment : next->attachments) if (attachment.source == chosen) {
    attachment.source = VirtualMenuSource::RIGHT;
    auto translate = [left](ButtonID input) { return input == (left ? ButtonID::MISC3 : ButtonID::MISC2) ? ButtonID::CAPTURE : left && input == ButtonID::MISC4 ? ButtonID::TOUCH : input; };
    attachment.input = translate(attachment.input); attachment.confirm = translate(attachment.confirm); attachment.cancel = translate(attachment.cancel);
  }
  std::lock_guard<std::mutex> lock(writer); publish(next);
}
static bool parseDefinition(VirtualMenuCatalog &next, std::string_view arguments) {
  std::istringstream stream{ std::string(arguments) };
  std::string id, type; VirtualMenuDefinition definition;
  if (!(stream >> id >> type >> definition.count >> definition.columns >> definition.deadzone) ||
    !end(stream) || !identifier(id) || definition.count < 1 || definition.count > 25 ||
    definition.columns < 1 || definition.columns > definition.count || !std::isfinite(definition.deadzone) ||
    definition.deadzone < 0.f || definition.deadzone >= 1.f) return false;
  if (type == "RADIAL") { if (definition.count < 2) return false; definition.type = VirtualMenuType::RADIAL; }
  else if (type == "TOUCH") definition.type = VirtualMenuType::TOUCH;
  else if (type == "HOTBAR") definition.type = VirtualMenuType::HOTBAR;
  else return false;
  if (!next.definitions.count(id) && next.definitions.size() >= 16) return false;
  auto previous = next.definitions.find(id);
  if (previous != next.definitions.end()) { definition.actions = previous->second.actions; definition.centerAction = previous->second.centerAction; }
  definition.actions.resize(definition.count, Mapping::NO_MAPPING);
  next.definitions[id] = std::move(definition); return true;
}
static bool parseAction(VirtualMenuCatalog &next, std::string_view arguments) {
  std::istringstream stream{ std::string(arguments) }; std::string id; int index = 0;
  if (!(stream >> id >> index)) return false;
  std::string binding; std::getline(stream >> std::ws, binding);
  Mapping mapping; std::istringstream values(binding); values >> mapping;
  if (!mapping.isValid() || values.fail()) return false;
  auto found = next.definitions.find(id);
  if (found == next.definitions.end() || index < 0 || index > found->second.count) return false;
  if (index == 0) found->second.centerAction = std::move(mapping);
  else found->second.actions[index - 1] = std::move(mapping);
  return true;
}
static bool parseAttachment(VirtualMenuCatalog &next, std::string_view arguments) {
  std::istringstream stream{ std::string(arguments) };
  VirtualMenuAttachment attachment; std::string source, activation, selection;
  if (!(stream >> attachment.menu >> source >> activation >> attachment.input >> selection >> attachment.confirm >> attachment.cancel)) return false;
  if (!end(stream)) {
    std::string navigation;
    if (!(stream >> navigation) || !end(stream)) return false;
    if (navigation == "JOYSTICK_CURSOR") attachment.joystickCursor = true;
    else if (navigation != "JOYSTICK") return false;
    if (source != "LSTICK" && source != "RSTICK") return false;
  }
  if (source == "LEFT") attachment.source = VirtualMenuSource::LEFT;
  else if (source == "RIGHT") attachment.source = VirtualMenuSource::RIGHT;
  else if (source == "LSTICK") attachment.source = VirtualMenuSource::LSTICK;
  else if (source == "RSTICK") attachment.source = VirtualMenuSource::RSTICK;
  else if (source == "DPAD") attachment.source = VirtualMenuSource::DPAD;
  else if (source == "ABXY") attachment.source = VirtualMenuSource::ABXY;
  else return false;
  if (activation == "HOLD") attachment.activation = VirtualMenuActivation::HOLD;
  else if (activation == "TOGGLE") attachment.activation = VirtualMenuActivation::TOGGLE;
  else if (activation == "ALWAYS") attachment.activation = VirtualMenuActivation::ALWAYS;
  else if (activation == "COMMAND") attachment.activation = VirtualMenuActivation::COMMAND;
  else return false;
  if (selection == "CLICK") attachment.selection = VirtualMenuSelection::CLICK;
  else if (selection == "TOUCH_RELEASE") attachment.selection = VirtualMenuSelection::TOUCH_RELEASE;
  else if (selection == "ACTIVATION_RELEASE") attachment.selection = VirtualMenuSelection::ACTIVATION_RELEASE;
  else if (selection == "CONTINUOUS") attachment.selection = VirtualMenuSelection::CONTINUOUS;
  else return false;
  const auto validInput = [](ButtonID id) {
    if (isInvertedChord(id)) {
      const auto base = invertedChordBase(id);
      return base > ButtonID::NONE && base < ButtonID::SIZE;
    }
    return id >= ButtonID::NONE && id != ButtonID::SIZE && id <= ButtonID::RM25;
  };
  if (!validInput(attachment.input) || !validInput(attachment.confirm) || !validInput(attachment.cancel) ||
    ((attachment.activation == VirtualMenuActivation::HOLD || attachment.activation == VirtualMenuActivation::TOGGLE) && attachment.input == ButtonID::NONE) ||
    (attachment.activation == VirtualMenuActivation::ALWAYS && attachment.selection == VirtualMenuSelection::ACTIVATION_RELEASE) ||
    ((attachment.source == VirtualMenuSource::DPAD || attachment.source == VirtualMenuSource::ABXY) && attachment.selection == VirtualMenuSelection::TOUCH_RELEASE)) return false;
  auto found = next.definitions.find(attachment.menu);
  if (found == next.definitions.end() ||
    (attachment.joystickCursor && found->second.type == VirtualMenuType::HOTBAR) ||
    ((attachment.source == VirtualMenuSource::DPAD || attachment.source == VirtualMenuSource::ABXY) && found->second.type != VirtualMenuType::HOTBAR)) return false;
  // Repeating a source/input attachment updates its intent instead of firing twice.
  auto same = std::find_if(next.attachments.begin(), next.attachments.end(), [&](const auto &old) {
    return old.source == attachment.source && (attachment.activation == VirtualMenuActivation::COMMAND
      ? old.activation == VirtualMenuActivation::COMMAND && old.menu == attachment.menu
      : old.activation != VirtualMenuActivation::COMMAND && old.input == attachment.input);
  });
  if (same == next.attachments.end()) {
    if (next.attachments.size() >= 32) return false;
    next.attachments.push_back(attachment);
  } else *same = attachment;
  return true;
}
using Parser = bool (*)(VirtualMenuCatalog &, std::string_view);
static bool edit(std::string_view arguments, Parser parser) {
  std::lock_guard<std::mutex> lock(writer);
  auto next = std::make_shared<VirtualMenuCatalog>(*snapshot());
  if (!parser(*next, arguments)) return false;
  publish(next); return true;
}
bool define(std::string_view arguments) { return edit(arguments, parseDefinition); }
bool action(std::string_view arguments) { return edit(arguments, parseAction); }
bool attach(std::string_view arguments) { return edit(arguments, parseAttachment); }
bool replace(std::string_view encoded) {
  // One profile/layer assignment owns an atomic catalog. Hex protects quoted
  // bindings, UTF-8 labels and '#' from the legacy config-line comment splitter.
  std::istringstream arguments{ std::string(encoded) };
  arguments >> std::ws;
  if (arguments.peek() == '=') arguments.get();
  std::string token; arguments >> token;
  if (!end(arguments) || token.size() > 400004 || token.rfind("HEX:", 0) != 0 || (token.size() - 4) % 2) return false;
  std::string decoded; decoded.reserve((token.size() - 4) / 2);
  auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
  for (size_t i = 4; i < token.size(); i += 2) {
    const int a = digit(token[i]), b = digit(token[i + 1]);
    if (a < 0 || b < 0 || a * 16 + b == 0) return false;
    decoded += char(a * 16 + b);
  }
  auto next = std::make_shared<VirtualMenuCatalog>();
  std::istringstream lines(decoded); std::string line;
  while (std::getline(lines, line)) {
    const auto space = line.find(' ');
    if (space == std::string::npos) return false;
    const auto name = line.substr(0, space), data = line.substr(space + 1);
    if (name == "DEFINE") { if (!parseDefinition(*next, data)) return false; }
    else if (name == "ACTION") { if (!parseAction(*next, data)) return false; }
    else if (name == "SOURCE") { if (!parseAttachment(*next, data)) return false; }
    else if (name == "PRESENTATION") { /* Renderer-owned labels, icons and placement; no input logic. */ }
    else return false;
  }
  std::lock_guard<std::mutex> lock(writer); publish(next); return true;
}
}
