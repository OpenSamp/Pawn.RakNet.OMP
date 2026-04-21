/*
 * Pawn.RakNet public C++ API for open.mp components.
 *
 * Exposes packet/RPC send + emulate + network event hooks as an IExtension so
 * other components (e.g. SampSharp.RakNet bridge) can interact with the RakNet
 * pipeline without going through AMX.
 *
 * Consumer pattern:
 *     auto* rakNetComp = list->queryComponent(UID{0x4a8b15c16d23e42fULL});
 *     if (rakNetComp) {
 *         auto* rn = queryExtension<IPawnRakNetComponent>(rakNetComp);
 *         int bs = rn->bitStreamNew();
 *         // ... rn->bitStreamWriteUint8(bs, 0); ...
 *         rn->sendPacket(bs, playerId, 3 /@ HIGH @/, 9 /@ RELIABLE_ORDERED @/, 0);
 *         rn->bitStreamDelete(bs);
 *     }
 */

#pragma once

#include <component.hpp>
#include <cstdint>
#include <vector>

// Component UID matches PluginComponent; extension lives on its own IID slot.
constexpr UID kPawnRakNetComponentUID = UID(0x4a8b15c16d23e42fULL);
constexpr UID kPawnRakNetExtensionUID = UID(0x4a8b15c16d23e430ULL);

// C++ event handler — C equivalent of Pawn publics registered via PR_RegHandler.
// Return true → continue propagation; return false → veto the packet/RPC (same
// semantics as Pawn returning 0 from the public).
struct IPawnRakNetEventHandler
{
    virtual ~IPawnRakNetEventHandler() = default;

    virtual bool onIncomingPacket(int playerId, int packetId, int bitStreamHandle) { return true; }
    virtual bool onIncomingRPC(int playerId, int rpcId, int bitStreamHandle) { return true; }
    virtual bool onIncomingCustomRPC(int playerId, int rpcId, int bitStreamHandle) { return true; }
    virtual bool onOutgoingPacket(int playerId, int packetId, int bitStreamHandle) { return true; }
    virtual bool onOutgoingRPC(int playerId, int rpcId, int bitStreamHandle) { return true; }
};

struct IPawnRakNetComponent : public IExtension
{
    PROVIDE_EXT_UID(kPawnRakNetExtensionUID)

    // ================================================================================
    // BitStream lifecycle (handle-based — matches the existing BitStreamHandleTable
    // so handles are interchangeable with Pawn-side BitStream: tags).
    // ================================================================================

    /// Allocate a new BitStream from the plugin pool. Returns a non-zero handle
    /// or 0 on failure. Must be freed via <see cref="bitStreamDelete"/>.
    virtual int  bitStreamNew() = 0;

    /// Allocate a BitStream that copies contents of an existing one. Returns 0
    /// if the source handle is invalid.
    virtual int  bitStreamNewCopy(int handle) = 0;

    /// Release a handle back to the pool. No-op if the handle is invalid.
    virtual void bitStreamDelete(int handle) = 0;

    virtual void bitStreamReset(int handle) = 0;
    virtual void bitStreamResetReadPointer(int handle) = 0;
    virtual void bitStreamResetWritePointer(int handle) = 0;
    virtual void bitStreamIgnoreBits(int handle, int numberOfBits) = 0;

    virtual int  bitStreamGetReadOffset(int handle) = 0;
    virtual void bitStreamSetReadOffset(int handle, int offset) = 0;
    virtual int  bitStreamGetWriteOffset(int handle) = 0;
    virtual void bitStreamSetWriteOffset(int handle, int offset) = 0;

    virtual int  bitStreamNumberOfBitsUsed(int handle) = 0;
    virtual int  bitStreamNumberOfBytesUsed(int handle) = 0;
    virtual int  bitStreamNumberOfUnreadBits(int handle) = 0;
    virtual int  bitStreamNumberOfBitsAllocated(int handle) = 0;

    // ================================================================================
    // BitStream write primitives. Mirror PR_INT8 … PR_NORM_QUAT. The "compressed"
    // flag toggles between uncompressed and RakNet's ::WriteCompressed path for
    // numeric/bool types (PR_CINT*, PR_CUINT*, PR_CFLOAT, PR_CBOOL).
    // ================================================================================

    virtual void bitStreamWriteInt8(int handle, int8_t value, bool compressed) = 0;
    virtual void bitStreamWriteInt16(int handle, int16_t value, bool compressed) = 0;
    virtual void bitStreamWriteInt32(int handle, int32_t value, bool compressed) = 0;
    virtual void bitStreamWriteUint8(int handle, uint8_t value, bool compressed) = 0;
    virtual void bitStreamWriteUint16(int handle, uint16_t value, bool compressed) = 0;
    virtual void bitStreamWriteUint32(int handle, uint32_t value, bool compressed) = 0;
    virtual void bitStreamWriteFloat(int handle, float value, bool compressed) = 0;
    virtual void bitStreamWriteBool(int handle, bool value, bool compressed) = 0;

    /// Writes raw bytes, no length prefix. Corresponds to PR_STRING.
    virtual void bitStreamWriteString(int handle, const char* data, int length) = 0;
    /// Writes via RakNet's StringCompressor. Corresponds to PR_CSTRING.
    virtual void bitStreamWriteStringCompressed(int handle, const char* data, int length) = 0;
    /// Writes uint8 length prefix then raw bytes. PR_STRING8.
    virtual void bitStreamWriteString8(int handle, const char* data, int length) = 0;
    /// Writes uint32 length prefix then raw bytes. PR_STRING32.
    virtual void bitStreamWriteString32(int handle, const char* data, int length) = 0;

    /// Raw bit write. PR_BITS.
    virtual void bitStreamWriteBits(int handle, const uint8_t* data, int numberOfBits, bool rightAlignedBits) = 0;

    virtual void bitStreamWriteFloat3(int handle, float x, float y, float z) = 0;
    virtual void bitStreamWriteFloat4(int handle, float x, float y, float z, float w) = 0;
    virtual void bitStreamWriteVector(int handle, float x, float y, float z) = 0;
    virtual void bitStreamWriteNormQuat(int handle, float x, float y, float z, float w) = 0;

    // ================================================================================
    // BitStream read primitives. Each returns true on success. For types with
    // compile-time size this means the read didn't run off the end.
    // ================================================================================

    virtual bool bitStreamReadInt8(int handle, int8_t& outValue, bool compressed) = 0;
    virtual bool bitStreamReadInt16(int handle, int16_t& outValue, bool compressed) = 0;
    virtual bool bitStreamReadInt32(int handle, int32_t& outValue, bool compressed) = 0;
    virtual bool bitStreamReadUint8(int handle, uint8_t& outValue, bool compressed) = 0;
    virtual bool bitStreamReadUint16(int handle, uint16_t& outValue, bool compressed) = 0;
    virtual bool bitStreamReadUint32(int handle, uint32_t& outValue, bool compressed) = 0;
    virtual bool bitStreamReadFloat(int handle, float& outValue, bool compressed) = 0;
    virtual bool bitStreamReadBool(int handle, bool& outValue, bool compressed) = 0;

    /// Reads exactly `maxLength` bytes into `outBuffer`. PR_STRING.
    virtual bool bitStreamReadString(int handle, char* outBuffer, int maxLength) = 0;
    virtual bool bitStreamReadStringCompressed(int handle, char* outBuffer, int maxLength) = 0;
    /// Reads uint8 length prefix then up to `maxLength` bytes. Returns actually read length.
    /// Buffer is NOT null-terminated by this call.
    virtual int  bitStreamReadString8(int handle, char* outBuffer, int maxLength) = 0;
    virtual int  bitStreamReadString32(int handle, char* outBuffer, int maxLength) = 0;

    virtual bool bitStreamReadBits(int handle, uint8_t* outData, int numberOfBits, bool rightAlignedBits) = 0;

    virtual bool bitStreamReadFloat3(int handle, float& x, float& y, float& z) = 0;
    virtual bool bitStreamReadFloat4(int handle, float& x, float& y, float& z, float& w) = 0;
    virtual bool bitStreamReadVector(int handle, float& x, float& y, float& z) = 0;
    virtual bool bitStreamReadNormQuat(int handle, float& x, float& y, float& z, float& w) = 0;

    // ================================================================================
    // Send / Emulate. playerId == -1 broadcasts. Priority/reliability values match
    // PR_PacketPriority / PR_PacketReliability enums in Pawn.RakNet.inc.
    // ================================================================================

    virtual bool sendPacket(int handle, int playerId, int priority, int reliability, uint8_t orderingChannel) = 0;
    virtual bool sendRPC(int handle, int playerId, int rpcId, int priority, int reliability, uint8_t orderingChannel) = 0;

    virtual bool emulateIncomingPacket(int handle, int playerId) = 0;
    virtual bool emulateIncomingRPC(int handle, int playerId, int rpcId) = 0;

    // ================================================================================
    // Custom RPC classification — routes incoming RPC with matching id through
    // onIncomingCustomRPC instead of onIncomingRPC.
    // ================================================================================

    virtual void setCustomRPC(int rpcId) = 0;
    virtual bool isCustomRPC(int rpcId) = 0;

    // ================================================================================
    // Event handler registry. Handlers are caller-owned. Same pointer can be
    // added multiple times; duplicates are ignored.
    // ================================================================================

    virtual void addEventHandler(IPawnRakNetEventHandler* handler) = 0;
    virtual void removeEventHandler(IPawnRakNetEventHandler* handler) = 0;

    void reset() override { /* handled by PluginComponent::reset */ }
};

// Defined in pawnraknet_extension_api.cc. Returns process-wide singleton that
// PluginComponent::getExtension() hands out when asked for IPawnRakNetComponent.
IPawnRakNetComponent* GetPawnRakNetExtension();

// Flat list of registered C++ event handlers — used by main.cc network hooks.
const std::vector<IPawnRakNetEventHandler*>& GetPawnRakNetEventHandlers();
