#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace jtml {

// Offsets are bytes. Columns count UTF-8 scalar starts, including a tab as one.
struct OriginalSource {
    std::string file;
    std::string text;
    std::vector<size_t> lineStarts{0};

    OriginalSource(std::string path, std::string bytes)
        : file(std::move(path)), text(std::move(bytes)) {
        for (size_t i = 0; i < text.size(); ++i)
            if (text[i] == '\n') lineStarts.push_back(i + 1);
    }
    std::pair<size_t, size_t> coordinates(size_t offset) const {
        if (offset > text.size()) throw std::out_of_range("Original source offset");
        const auto line = std::upper_bound(lineStarts.begin(), lineStarts.end(), offset)
                        - lineStarts.begin();
        size_t column = 1;
        for (size_t i = lineStarts[line - 1]; i < offset; ++i)
            if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) ++column;
        return {static_cast<size_t>(line), column};
    }
};

struct OriginalRange {
    std::shared_ptr<const OriginalSource> source;
    size_t begin = 0;
    size_t end = 0;
};

// Transformations copy slices, append other mapped buffers, or explicitly emit
// synthetic bytes. No matching of generated spelling against original text.
class MappedSource {
public:
    struct Segment {
        size_t begin;
        size_t end;
        std::shared_ptr<const OriginalSource> source;
        size_t originalBegin;
    };

    const std::string& text() const { return bytes; }
    size_t size() const { return bytes.size(); }
    const std::vector<Segment>& mapping() const { return segments; }

    static MappedSource original(std::string file, std::string text) {
        auto source = std::make_shared<const OriginalSource>(std::move(file), std::move(text));
        MappedSource result;
        result.appendOriginal(source, 0, source->text.size());
        return result;
    }
    void appendOriginal(const std::shared_ptr<const OriginalSource>& source,
                        size_t begin, size_t count) {
        if (!source || begin > source->text.size() || count > source->text.size() - begin)
            throw std::out_of_range("Original source slice");
        appendSegment(source->text.substr(begin, count), source, begin);
    }
    void appendSynthetic(const std::string& text) { appendSegment(text, nullptr, 0); }
    void append(const MappedSource& other) {
        // Copy first also makes self-append well-defined.
        const auto copy = other;
        for (const auto& s : copy.segments)
            appendSegment(copy.bytes.substr(s.begin, s.end - s.begin), s.source, s.originalBegin);
    }
    MappedSource slice(size_t begin, size_t count) const {
        if (begin > size() || count > size() - begin) throw std::out_of_range("Mapped source slice");
        MappedSource result;
        const size_t end = begin + count;
        for (const auto& s : segments) {
            const auto lo = std::max(begin, s.begin), hi = std::min(end, s.end);
            if (lo < hi)
                result.appendSegment(bytes.substr(lo, hi - lo), s.source,
                                     s.originalBegin + (lo - s.begin));
        }
        return result;
    }
    // Empty, synthetic, discontinuous, or mixed-file ranges are not exact.
    std::optional<OriginalRange> exactRange(size_t begin, size_t end) const {
        if (begin >= end || end > size()) return std::nullopt;
        std::optional<OriginalRange> result;
        size_t covered = begin;
        for (const auto& s : segments) {
            const auto lo = std::max(begin, s.begin), hi = std::min(end, s.end);
            if (lo >= hi) continue;
            if (!s.source || lo != covered) return std::nullopt;
            const auto original = s.originalBegin + lo - s.begin;
            if (!result) result = OriginalRange{s.source, original, original};
            if (result->source != s.source || result->end != original) return std::nullopt;
            result->end += hi - lo;
            covered = hi;
        }
        return covered == end ? result : std::nullopt;
    }
private:
    std::string bytes;
    std::vector<Segment> segments;
    void appendSegment(const std::string& text, std::shared_ptr<const OriginalSource> source,
                       size_t originalBegin) {
        if (text.empty()) return;
        const size_t begin = bytes.size();
        bytes += text;
        if (!segments.empty() && segments.back().source == source &&
            (!source || segments.back().originalBegin + segments.back().end - segments.back().begin == originalBegin)) {
            segments.back().end = bytes.size();
        } else segments.push_back({begin, bytes.size(), std::move(source), originalBegin});
    }
};

} // namespace jtml
