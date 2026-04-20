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

namespace {

// Slot 0 is reserved as the "invalid" handle.
std::vector<BitStream *> &Items() {
  static std::vector<BitStream *> items{nullptr};
  return items;
}

std::vector<cell> &FreeList() {
  static std::vector<cell> free_list;
  return free_list;
}

}  // namespace

cell BitStreamHandleTable::Register(BitStream *bs) {
  if (!bs) {
    return 0;
  }

  auto &items = Items();
  auto &free_list = FreeList();

  if (!free_list.empty()) {
    const cell handle = free_list.back();
    free_list.pop_back();
    items[handle] = bs;
    return handle;
  }

  const cell handle = static_cast<cell>(items.size());
  items.push_back(bs);
  return handle;
}

void BitStreamHandleTable::Unregister(cell handle) {
  auto &items = Items();
  if (handle <= 0 || static_cast<std::size_t>(handle) >= items.size()) {
    return;
  }
  if (!items[handle]) {
    return;
  }
  items[handle] = nullptr;
  FreeList().push_back(handle);
}

void BitStreamHandleTable::UnregisterByPointer(BitStream *bs) {
  if (!bs) {
    return;
  }
  auto &items = Items();
  auto &free_list = FreeList();
  for (std::size_t i = 1; i < items.size(); ++i) {
    if (items[i] == bs) {
      items[i] = nullptr;
      free_list.push_back(static_cast<cell>(i));
    }
  }
}

BitStream *BitStreamHandleTable::Lookup(cell handle) {
  auto &items = Items();
  if (handle <= 0 || static_cast<std::size_t>(handle) >= items.size()) {
    return nullptr;
  }
  return items[handle];
}
