/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2023 katursis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "main.h"
#include "pawnraknet_extension_api.h"

namespace {

// Iterate C++ event handlers registered via IPawnRakNetComponent::addEventHandler.
// Returns false if any handler vetoes the event.
//
// IMPORTANT: pawnraknet handlers are called from open.mp's
// NetworkInEventDispatcher (in onReceivePacket / onReceiveRPC). Open.mp then
// dispatches the same BitStream through SingleNetworkInEventDispatcher (the
// per-packet / per-RPC dispatchers, where SAMP's PlayerFootSyncHandler etc.
// live). Those downstream handlers expect the read pointer to be positioned
// at the data BODY, not at the packet/RPC ID byte (compare PR_EmulateIncomingPacket
// in script.cc which explicitly does bs->SetReadOffset(8) before invoking
// per-packet handlers).
//
// Calling bs->resetReadPointer() here (offset=0) leaves the bitstream in a
// state that confuses open.mp's downstream handlers — they'd start reading
// from the packet ID byte rather than the data body, producing garbage
// positions / failing internal validation. We therefore explicitly position
// the read pointer at bit 8 (after the packet/RPC ID byte) on exit.
//
// Between our own handlers we still reset to 0 — pawnraknet's contract is
// that handlers see the full bitstream from the start.
bool FireCppHandlers(PR_EventType type, int playerId, int eventId, BitStream *bs) {
  const auto &handlers = GetPawnRakNetEventHandlers();
  if (handlers.empty()) return true;

  // Event handlers receive an int BitStream handle that stays alive only for the
  // duration of the callback; same contract as for Pawn publics.
  ScopedBitStreamHandle scoped(bs);
  const int h = static_cast<int>(scoped.get());

  for (auto *handler : handlers) {
    bs->resetReadPointer();
    bool cont = true;
    switch (type) {
      case PR_INCOMING_PACKET:
        cont = handler->onIncomingPacket(playerId, eventId, h);
        break;
      case PR_INCOMING_RPC:
        cont = handler->onIncomingRPC(playerId, eventId, h);
        break;
      case PR_INCOMING_CUSTOM_RPC:
        cont = handler->onIncomingCustomRPC(playerId, eventId, h);
        break;
      case PR_OUTGOING_PACKET:
        cont = handler->onOutgoingPacket(playerId, eventId, h);
        break;
      case PR_OUTGOING_RPC:
        cont = handler->onOutgoingRPC(playerId, eventId, h);
        break;
      default:
        break;
    }
    if (!cont) {
      bs->SetReadOffset(8);
      return false;
    }
  }

  bs->SetReadOffset(8);
  return true;
}

}  // namespace

StringView PluginComponent::componentName() const {
  return Plugin::Instance().Name();
}

SemanticVersion PluginComponent::componentVersion() const {
  auto [major, minor, patch] =
      Plugin::VersionToTuple(Plugin::Instance().Version());

  return SemanticVersion(0, major, minor, patch);
}

void PluginComponent::onLoad(ICore *c) {
  core_ = c;

  getCore() = c;
  get() = this;
}

void PluginComponent::onInit(IComponentList *components) {
  pawn_component_ = components->queryComponent<IPawnComponent>();
  if (!pawn_component_) {
    StringView name = componentName();

    core_->logLn(LogLevel::Error,
                 "Error loading component %.*s: Pawn component not loaded",
                 name.length(), name.data());

    return;
  }

  core_->getEventDispatcher().addEventHandler(this);
  pawn_component_->getEventDispatcher().addEventHandler(this);

  for (auto network : core_->getNetworks()) {
    network->getInEventDispatcher().addEventHandler(this);
    network->getOutEventDispatcher().addEventHandler(this);
  }

  plugin_data_[PLUGIN_DATA_LOGPRINTF] =
      reinterpret_cast<void *>(&PluginLogprintf);
  plugin_data_[PLUGIN_DATA_AMX_EXPORTS] =
      const_cast<void **>(pawn_component_->getAmxFunctions().data());

  Plugin::DoLoad(plugin_data_);
}

void PluginComponent::onAmxLoad(IPawnScript &script) {
  Plugin::DoAmxLoad(static_cast<AMX *>(script.GetAMX()));
};

void PluginComponent::onAmxUnload(IPawnScript &script) {
  Plugin::DoAmxUnload(static_cast<AMX *>(script.GetAMX()));
};

void PluginComponent::onTick(Microseconds elapsed, TimePoint now) {
  Plugin::DoProcessTick();
}

bool PluginComponent::onReceivePacket(IPlayer &peer, int id,
                                      NetworkBitStream &bs) {
  if (!Plugin::OnEvent<PR_INCOMING_PACKET>(peer.getID(), id, &bs)) return false;
  return FireCppHandlers(PR_INCOMING_PACKET, peer.getID(), id, &bs);
}

bool PluginComponent::onReceiveRPC(IPlayer &peer, int id,
                                   NetworkBitStream &bs) {
  auto &plugin = Plugin::Get();
  const bool custom = plugin.IsCustomRPC(static_cast<RPCIndex>(id));

  const auto on_event = custom ? Plugin::OnEvent<PR_INCOMING_CUSTOM_RPC>
                               : Plugin::OnEvent<PR_INCOMING_RPC>;
  if (!on_event(peer.getID(), id, &bs)) return false;

  return FireCppHandlers(
      custom ? PR_INCOMING_CUSTOM_RPC : PR_INCOMING_RPC,
      peer.getID(), id, &bs);
}

bool PluginComponent::onSendPacket(IPlayer *peer, int id,
                                   NetworkBitStream &bs) {
  const int pid = peer ? peer->getID() : -1;
  if (!Plugin::OnEvent<PR_OUTGOING_PACKET>(pid, id, &bs)) return false;
  return FireCppHandlers(PR_OUTGOING_PACKET, pid, id, &bs);
}

bool PluginComponent::onSendRPC(IPlayer *peer, int id, NetworkBitStream &bs) {
  const int pid = peer ? peer->getID() : -1;
  if (!Plugin::OnEvent<PR_OUTGOING_RPC>(pid, id, &bs)) return false;
  return FireCppHandlers(PR_OUTGOING_RPC, pid, id, &bs);
}

IExtension *PluginComponent::getExtension(UID id) {
  if (id == IPawnRakNetComponent::ExtensionIID) {
    return GetPawnRakNetExtension();
  }
  return nullptr;
}

void PluginComponent::onFree(IComponent *component) {
  if (!pawn_component_) {
    return;
  }

  if (component == pawn_component_ || component == this) {
    Plugin::DoUnload();

    core_->getEventDispatcher().removeEventHandler(this);
    pawn_component_->getEventDispatcher().removeEventHandler(this);

    pawn_component_ = nullptr;
  }
}

void PluginComponent::reset() {}

void PluginComponent::free() {
  delete this;
}

void PluginComponent::PluginLogprintf(const char *fmt, ...) {
  auto core = getCore();
  if (!core) {
    return;
  }

  va_list args{};

  va_start(args, fmt);

  core->vprintLn(fmt, args);

  va_end(args);
}

ICore *&PluginComponent::getCore() {
  static ICore *core{};

  return core;
}

PluginComponent *&PluginComponent::get() {
  static PluginComponent *component{};

  return component;
}

COMPONENT_ENTRY_POINT() { return new PluginComponent(); }
