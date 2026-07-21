#include "assets/IffReader.h"

namespace assets {

std::string readNulTerminatedString(soe::PacketBuffer& buf) {
    std::string result;
    for (;;) {
        uint8_t ch = buf.readByte();
        if (ch == 0) {
            return result;
        }
        result.push_back(static_cast<char>(ch));
    }
}

const IffChunk* IffChunk::findChild(uint32_t childId) const {
    for (const auto& child : children) {
        if (child.id == childId) {
            return &child;
        }
    }
    return nullptr;
}

const IffChunk* findFirstForm(const IffChunk& chunk, uint32_t formTypeTag) {
    if (chunk.id != kFormTag) {
        return nullptr;
    }
    if (chunk.formType == formTypeTag) {
        return &chunk;
    }
    for (const auto& child : chunk.children) {
        if (const IffChunk* found = findFirstForm(child, formTypeTag)) {
            return found;
        }
    }
    return nullptr;
}

const IffChunk* findFirstChunk(const IffChunk& chunk, uint32_t tag) {
    if (chunk.id != kFormTag && chunk.id == tag) {
        return &chunk;
    }
    for (const auto& child : chunk.children) {
        if (const IffChunk* found = findFirstChunk(child, tag)) {
            return found;
        }
    }
    return nullptr;
}

std::vector<const IffChunk*> findAllForms(const IffChunk& chunk, uint32_t formTypeTag) {
    std::vector<const IffChunk*> result;
    if (chunk.id != kFormTag) {
        return result;
    }
    if (chunk.formType == formTypeTag) {
        result.push_back(&chunk);
        return result;
    }
    for (const auto& child : chunk.children) {
        auto nested = findAllForms(child, formTypeTag);
        result.insert(result.end(), nested.begin(), nested.end());
    }
    return result;
}

std::vector<IffChunk> IffReader::parse(const std::vector<uint8_t>& bytes) {
    soe::PacketBuffer buf(bytes.data(), bytes.size());
    std::vector<IffChunk> topLevel;
    // Real asset files have exactly one top-level FORM in practice, but
    // mirrors IffStream::loadMainChunks() in reading a sequence rather than
    // assuming that.
    while (buf.remaining() >= 8) {
        topLevel.push_back(parseOneChunk(buf));
    }
    return topLevel;
}

IffChunk IffReader::parseOneChunk(soe::PacketBuffer& buf) {
    IffChunk chunk;
    chunk.id = buf.readUint32BE();
    uint32_t size = buf.readUint32BE();
    std::vector<uint8_t> chunkBytes = buf.readBytes(size);

    if (chunk.id == kFormTag) {
        soe::PacketBuffer formBuf(chunkBytes.data(), chunkBytes.size());
        chunk.formType = formBuf.readUint32BE();
        while (formBuf.remaining() >= 8) {
            chunk.children.push_back(parseOneChunk(formBuf));
        }
    } else {
        chunk.data = soe::PacketBuffer(chunkBytes.data(), chunkBytes.size());
    }
    return chunk;
}

} // namespace assets
