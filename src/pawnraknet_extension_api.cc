/*
 * IPawnRakNetComponent implementation. Operates independently from any AMX
 * Script — owns its own BitStream heap allocations tracked by handle.
 */

#include "main.h"
#include "pawnraknet_extension_api.h"
#include "RakNet/Encoding/str_compress.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace {

// ---- BitStream ownership ---------------------------------------------------
// Each extension-allocated BitStream lives here; the int handle returned to
// callers comes from BitStreamHandleTable so it's interchangeable with handles
// used by Pawn scripts and event callbacks.
std::unordered_map<int, std::unique_ptr<BitStream>>& ExtensionOwnedBitStreams()
{
    static std::unordered_map<int, std::unique_ptr<BitStream>> m;
    return m;
}

BitStream* LookupBS(int handle)
{
    return BitStreamHandleTable::Lookup(static_cast<cell>(handle));
}

// ---- Custom RPC id set -----------------------------------------------------
bool& CustomRPCSlot(int rpcId)
{
    static std::array<bool, PR_MAX_HANDLERS> table{};
    // PR_MAX_HANDLERS = 256 so any valid RPC byte fits
    static bool invalid = false;
    if (rpcId < 0 || rpcId >= PR_MAX_HANDLERS) return invalid;
    return table[rpcId];
}

// ---- Handler registry ------------------------------------------------------
std::vector<IPawnRakNetEventHandler*>& HandlerList()
{
    static std::vector<IPawnRakNetEventHandler*> s;
    return s;
}

// ---- Compressed writer helpers mirroring Script::WriteValue<T, true> ------
template <typename T>
inline void WriteUncompressed(BitStream* bs, T v) { bs->Write(v); }
template <typename T>
inline void WriteCompressed(BitStream* bs, T v) { bs->WriteCompressed(v); }

template <typename T>
inline bool ReadUncompressed(BitStream* bs, T& out) { return bs->Read(out); }
template <typename T>
inline bool ReadCompressed(BitStream* bs, T& out) { return bs->ReadCompressed(out); }

} // namespace

const std::vector<IPawnRakNetEventHandler*>& GetPawnRakNetEventHandlers()
{
    return HandlerList();
}

// ============================================================================
// Extension implementation.
// ============================================================================

class PawnRakNetExtension : public IPawnRakNetComponent
{
public:
    // -------- BitStream lifecycle ------------------------------------------

    int bitStreamNew() override
    {
        auto bs = std::make_unique<BitStream>();
        BitStream* raw = bs.get();
        const cell handle = BitStreamHandleTable::Register(raw);
        if (!handle) return 0;
        ExtensionOwnedBitStreams().emplace(static_cast<int>(handle), std::move(bs));
        return static_cast<int>(handle);
    }

    int bitStreamNewCopy(int srcHandle) override
    {
        auto* src = LookupBS(srcHandle);
        if (!src) return 0;
        auto bs = std::make_unique<BitStream>(src->GetData(), src->GetNumberOfBytesUsed(), true);
        BitStream* raw = bs.get();
        const cell handle = BitStreamHandleTable::Register(raw);
        if (!handle) return 0;
        ExtensionOwnedBitStreams().emplace(static_cast<int>(handle), std::move(bs));
        return static_cast<int>(handle);
    }

    void bitStreamDelete(int handle) override
    {
        if (!handle) return;
        auto& owned = ExtensionOwnedBitStreams();
        auto it = owned.find(handle);
        if (it == owned.end())
        {
            // Not our handle (could be Pawn-allocated or scoped); refuse to free,
            // just unregister in case the caller wants the handle gone.
            return;
        }
        BitStreamHandleTable::Unregister(static_cast<cell>(handle));
        owned.erase(it);
    }

    void bitStreamReset(int handle) override
    { if (auto* bs = LookupBS(handle)) bs->reset(); }
    void bitStreamResetReadPointer(int handle) override
    { if (auto* bs = LookupBS(handle)) bs->resetReadPointer(); }
    void bitStreamResetWritePointer(int handle) override
    { if (auto* bs = LookupBS(handle)) bs->resetWritePointer(); }
    void bitStreamIgnoreBits(int handle, int numberOfBits) override
    { if (auto* bs = LookupBS(handle)) bs->IgnoreBits(numberOfBits); }

    int bitStreamGetReadOffset(int handle) override
    { auto* bs = LookupBS(handle); return bs ? bs->GetReadOffset() : 0; }
    void bitStreamSetReadOffset(int handle, int offset) override
    { if (auto* bs = LookupBS(handle)) bs->SetReadOffset(offset); }
    int bitStreamGetWriteOffset(int handle) override
    { auto* bs = LookupBS(handle); return bs ? bs->GetWriteOffset() : 0; }
    void bitStreamSetWriteOffset(int handle, int offset) override
    { if (auto* bs = LookupBS(handle)) bs->SetWriteOffset(offset); }

    int bitStreamNumberOfBitsUsed(int handle) override
    { auto* bs = LookupBS(handle); return bs ? bs->GetNumberOfBitsUsed() : 0; }
    int bitStreamNumberOfBytesUsed(int handle) override
    { auto* bs = LookupBS(handle); return bs ? bs->GetNumberOfBytesUsed() : 0; }
    int bitStreamNumberOfUnreadBits(int handle) override
    { auto* bs = LookupBS(handle); return bs ? bs->GetNumberOfUnreadBits() : 0; }
    int bitStreamNumberOfBitsAllocated(int handle) override
    { auto* bs = LookupBS(handle); return bs ? static_cast<int>(bs->GetNumberOfBitsAllocated()) : 0; }

    // -------- Write primitives ---------------------------------------------

#define WRITE_IMPL(method, CType)                                                 \
    void method(int handle, CType v, bool compressed) override                    \
    {                                                                             \
        auto* bs = LookupBS(handle); if (!bs) return;                             \
        if (compressed) WriteCompressed<CType>(bs, v);                            \
        else WriteUncompressed<CType>(bs, v);                                     \
    }

    WRITE_IMPL(bitStreamWriteInt8,   int8_t)
    WRITE_IMPL(bitStreamWriteInt16,  int16_t)
    WRITE_IMPL(bitStreamWriteInt32,  int32_t)
    WRITE_IMPL(bitStreamWriteUint8,  uint8_t)
    WRITE_IMPL(bitStreamWriteUint16, uint16_t)
    WRITE_IMPL(bitStreamWriteUint32, uint32_t)
    WRITE_IMPL(bitStreamWriteFloat,  float)
    WRITE_IMPL(bitStreamWriteBool,   bool)

#undef WRITE_IMPL

    void bitStreamWriteString(int handle, const char* data, int length) override
    {
        auto* bs = LookupBS(handle); if (!bs || !data) return;
        bs->Write(data, length);
    }

    void bitStreamWriteStringCompressed(int handle, const char* data, int length) override
    {
        auto* bs = LookupBS(handle); if (!bs || !data) return;
        stringCompressor->EncodeString(data, length + 1, bs);
    }

    void bitStreamWriteString8(int handle, const char* data, int length) override
    {
        auto* bs = LookupBS(handle); if (!bs || !data) return;
        bs->Write(static_cast<uint8_t>(length));
        bs->Write(data, length);
    }

    void bitStreamWriteString32(int handle, const char* data, int length) override
    {
        auto* bs = LookupBS(handle); if (!bs || !data) return;
        bs->Write(static_cast<uint32_t>(length));
        bs->Write(data, length);
    }

    void bitStreamWriteBits(int handle, const uint8_t* data, int numberOfBits, bool rightAlignedBits) override
    {
        auto* bs = LookupBS(handle); if (!bs || !data) return;
        bs->WriteBits(data, numberOfBits, rightAlignedBits);
    }

    void bitStreamWriteFloat3(int handle, float x, float y, float z) override
    {
        auto* bs = LookupBS(handle); if (!bs) return;
        bs->Write(x); bs->Write(y); bs->Write(z);
    }

    void bitStreamWriteFloat4(int handle, float x, float y, float z, float w) override
    {
        auto* bs = LookupBS(handle); if (!bs) return;
        bs->Write(x); bs->Write(y); bs->Write(z); bs->Write(w);
    }

    void bitStreamWriteVector(int handle, float x, float y, float z) override
    { if (auto* bs = LookupBS(handle)) bs->WriteVector(x, y, z); }

    void bitStreamWriteNormQuat(int handle, float x, float y, float z, float w) override
    { if (auto* bs = LookupBS(handle)) bs->WriteNormQuat(x, y, z, w); }

    // -------- Read primitives ----------------------------------------------

#define READ_IMPL(method, CType)                                                  \
    bool method(int handle, CType& outValue, bool compressed) override            \
    {                                                                             \
        auto* bs = LookupBS(handle); if (!bs) return false;                       \
        return compressed ? ReadCompressed<CType>(bs, outValue)                   \
                          : ReadUncompressed<CType>(bs, outValue);                \
    }

    READ_IMPL(bitStreamReadInt8,   int8_t)
    READ_IMPL(bitStreamReadInt16,  int16_t)
    READ_IMPL(bitStreamReadInt32,  int32_t)
    READ_IMPL(bitStreamReadUint8,  uint8_t)
    READ_IMPL(bitStreamReadUint16, uint16_t)
    READ_IMPL(bitStreamReadUint32, uint32_t)
    READ_IMPL(bitStreamReadFloat,  float)
    READ_IMPL(bitStreamReadBool,   bool)

#undef READ_IMPL

    bool bitStreamReadString(int handle, char* outBuffer, int maxLength) override
    {
        auto* bs = LookupBS(handle); if (!bs || !outBuffer || maxLength <= 0) return false;
        return bs->Read(outBuffer, maxLength);
    }

    bool bitStreamReadStringCompressed(int handle, char* outBuffer, int maxLength) override
    {
        auto* bs = LookupBS(handle); if (!bs || !outBuffer || maxLength <= 0) return false;
        return stringCompressor->DecodeString(outBuffer, maxLength, bs);
    }

    int bitStreamReadString8(int handle, char* outBuffer, int maxLength) override
    {
        auto* bs = LookupBS(handle); if (!bs || !outBuffer || maxLength <= 0) return 0;
        uint8_t len = 0;
        if (!bs->Read(len)) return 0;
        const int actual = std::min<int>(len, maxLength);
        return bs->Read(outBuffer, actual) ? actual : 0;
    }

    int bitStreamReadString32(int handle, char* outBuffer, int maxLength) override
    {
        auto* bs = LookupBS(handle); if (!bs || !outBuffer || maxLength <= 0) return 0;
        uint32_t len = 0;
        if (!bs->Read(len)) return 0;
        const int actual = std::min<int>(static_cast<int>(len), maxLength);
        return bs->Read(outBuffer, actual) ? actual : 0;
    }

    bool bitStreamReadBits(int handle, uint8_t* outData, int numberOfBits, bool rightAlignedBits) override
    {
        auto* bs = LookupBS(handle); if (!bs || !outData) return false;
        return bs->ReadBits(outData, numberOfBits, rightAlignedBits);
    }

    bool bitStreamReadFloat3(int handle, float& x, float& y, float& z) override
    {
        auto* bs = LookupBS(handle); if (!bs) return false;
        return bs->Read(x) && bs->Read(y) && bs->Read(z);
    }

    bool bitStreamReadFloat4(int handle, float& x, float& y, float& z, float& w) override
    {
        auto* bs = LookupBS(handle); if (!bs) return false;
        return bs->Read(x) && bs->Read(y) && bs->Read(z) && bs->Read(w);
    }

    bool bitStreamReadVector(int handle, float& x, float& y, float& z) override
    { auto* bs = LookupBS(handle); return bs ? bs->ReadVector(x, y, z) : false; }

    bool bitStreamReadNormQuat(int handle, float& x, float& y, float& z, float& w) override
    { auto* bs = LookupBS(handle); return bs ? bs->ReadNormQuat(x, y, z, w) : false; }

    // -------- Send / Emulate -----------------------------------------------

    bool sendPacket(int handle, int playerId, int /*priority*/, int /*reliability*/, uint8_t orderingChannel) override
    {
        auto* bs = LookupBS(handle); if (!bs) return false;
        auto* core = PluginComponent::getCore(); if (!core) return false;

        Span<uint8_t> data(bs->GetData(), bs->GetNumberOfBitsUsed());
        if (playerId == -1)
        {
            core->getPlayers().broadcastPacket(data, orderingChannel, nullptr, false);
            return true;
        }
        auto player = core->getPlayers().get(playerId);
        return player && player->sendPacket(data, orderingChannel, false);
    }

    bool sendRPC(int handle, int playerId, int rpcId, int /*priority*/, int /*reliability*/, uint8_t orderingChannel) override
    {
        auto* bs = LookupBS(handle); if (!bs) return false;
        auto* core = PluginComponent::getCore(); if (!core) return false;

        Span<uint8_t> data(bs->GetData(), bs->GetNumberOfBitsUsed());
        if (playerId == -1)
        {
            core->getPlayers().broadcastRPC(static_cast<uint8_t>(rpcId), data, orderingChannel, nullptr, false);
            return true;
        }
        auto player = core->getPlayers().get(playerId);
        return player && player->sendRPC(static_cast<uint8_t>(rpcId), data, orderingChannel, false);
    }

    bool emulateIncomingPacket(int handle, int playerId) override
    {
        auto* bs = LookupBS(handle); if (!bs) return false;
        auto* core = PluginComponent::getCore(); if (!core) return false;
        auto player = core->getPlayers().get(playerId); if (!player) return false;
        // Trigger the network's inEventDispatcher so both Pawn and C++ handlers fire.
        // NOTE: open.mp doesn't expose a direct "emulate" on the public interface;
        // the closest portable path is running our own handler chain.
        const int id = bs->GetNumberOfBytesUsed() > 0 ? bs->GetData()[0] : 0;
        bs->resetReadPointer();
        return Plugin::OnEvent<PR_INCOMING_PACKET>(playerId, static_cast<uint8_t>(id), bs);
    }

    bool emulateIncomingRPC(int handle, int playerId, int rpcId) override
    {
        auto* bs = LookupBS(handle); if (!bs) return false;
        bs->resetReadPointer();
        return Plugin::OnEvent<PR_INCOMING_RPC>(playerId, static_cast<uint8_t>(rpcId), bs);
    }

    // -------- Custom RPC ---------------------------------------------------

    void setCustomRPC(int rpcId) override
    {
        // Delegate to Plugin so incoming RPC dispatch routes through custom path.
        Plugin::Get().SetCustomRPC(static_cast<RPCIndex>(rpcId));
        CustomRPCSlot(rpcId) = true;
    }

    bool isCustomRPC(int rpcId) override
    {
        return Plugin::Get().IsCustomRPC(static_cast<RPCIndex>(rpcId));
    }

    // -------- Handler registry ---------------------------------------------

    void addEventHandler(IPawnRakNetEventHandler* h) override
    {
        if (!h) return;
        auto& list = HandlerList();
        if (std::find(list.begin(), list.end(), h) == list.end())
            list.push_back(h);
    }

    void removeEventHandler(IPawnRakNetEventHandler* h) override
    {
        auto& list = HandlerList();
        list.erase(std::remove(list.begin(), list.end(), h), list.end());
    }
};

static PawnRakNetExtension g_extension;

IPawnRakNetComponent* GetPawnRakNetExtension()
{
    return &g_extension;
}
