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

#ifndef PAWNRAKNET_BITSTREAM_HANDLE_H_
#define PAWNRAKNET_BITSTREAM_HANDLE_H_

// Registry that maps 32-bit handles to BitStream* pointers.
// Needed because AMX `cell` is always 32-bit, so on 64-bit builds we can't
// pass raw pointers to Pawn scripts. Handle 0 is reserved as "invalid".
class BitStreamHandleTable {
 public:
  static cell Register(BitStream *bs);
  static void Unregister(cell handle);
  static void UnregisterByPointer(BitStream *bs);
  static BitStream *Lookup(cell handle);
};

// RAII wrapper for transient handles (event dispatch). Registers on
// construction, unregisters on destruction.
class ScopedBitStreamHandle {
 public:
  explicit ScopedBitStreamHandle(BitStream *bs)
      : handle_(BitStreamHandleTable::Register(bs)) {}

  ScopedBitStreamHandle(const ScopedBitStreamHandle &) = delete;
  ScopedBitStreamHandle &operator=(const ScopedBitStreamHandle &) = delete;

  ~ScopedBitStreamHandle() {
    if (handle_) {
      BitStreamHandleTable::Unregister(handle_);
    }
  }

  cell get() const { return handle_; }

 private:
  cell handle_;
};

#endif  // PAWNRAKNET_BITSTREAM_HANDLE_H_
